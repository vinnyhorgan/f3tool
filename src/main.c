#include "../include/bsa.h"
#include "../include/esm.h"
#include "../include/archive.h"
#include "../include/audio.h"
#include "../include/image.h"
#include "../include/util.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>

static void print_usage(const char *prog)
{
    printf("Usage: %s [options] <f3_path> [output_path]\n", prog);
    printf("\nOptions:\n");
    printf("  -e, --esm         Extract ESM files only\n");
    printf("  -a, --audio       Extract audio files only\n");
    printf("  -i, --art         Extract UI art only\n");
    printf("  -V, --video       Extract video files only\n");
    printf("  -A, --all         Extract everything (default)\n");
    printf("  -l, --list        List files in archive\n");
    printf("  -n, --limit N    Limit extraction to N files per type\n");
    printf("  -v, --verbose     Verbose output\n");
    printf("  -h, --help        Show this help\n");
    printf("\nExamples:\n");
    printf("  %s /path/to/fallout3              Extract all to ./f3_extracted\n", prog);
    printf("  %s /path/to/fallout3 -l           List files\n", prog);
    printf("  %s /path/to/fallout3 -e ./esm     Extract ESMs to ./esm\n", prog);
    printf("  %s /path/to/fallout3 -a -n 10    Extract 10 audio files for testing\n", prog);
}

typedef struct {
    int extract_esm;
    int extract_audio;
    int extract_art;
    int extract_video;
    int list_only;
    int verbose;
    int limit;
    char *f3_path;
    char *output_path;
} AppOptions;

static int parse_args(int argc, char **argv, AppOptions *opts)
{
    memset(opts, 0, sizeof(*opts));
    opts->extract_esm = 1;
    opts->extract_audio = 1;
    opts->extract_art = 1;

    static struct option long_options[] = {
        {"esm", no_argument, 0, 'e'},
        {"audio", no_argument, 0, 'a'},
        {"art", no_argument, 0, 'i'},
        {"video", no_argument, 0, 'V'},
        {"all", no_argument, 0, 'A'},
        {"list", no_argument, 0, 'l'},
        {"limit", required_argument, 0, 'n'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "eaiVAln:vh", long_options, NULL)) != -1) {
        switch (c) {
            case 'e':
                opts->extract_esm = 1;
                opts->extract_audio = 0;
                opts->extract_art = 0;
                opts->extract_video = 0;
                break;
            case 'a':
                opts->extract_audio = 1;
                opts->extract_esm = 0;
                opts->extract_art = 0;
                opts->extract_video = 0;
                break;
            case 'i':
                opts->extract_art = 1;
                opts->extract_esm = 0;
                opts->extract_audio = 0;
                opts->extract_video = 0;
                break;
            case 'V':
                opts->extract_video = 1;
                opts->extract_esm = 0;
                opts->extract_audio = 0;
                opts->extract_art = 0;
                break;
            case 'A':
                opts->extract_esm = 1;
                opts->extract_audio = 1;
                opts->extract_art = 1;
                opts->extract_video = 1;
                break;
            case 'l':
                opts->list_only = 1;
                break;
            case 'n':
                opts->limit = atoi(optarg);
                break;
            case 'v':
                opts->verbose = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                return -1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: Fallout 3 path required\n");
        return -1;
    }

    opts->f3_path = argv[optind++];

    if (optind < argc) {
        opts->output_path = argv[optind];
    } else {
        opts->output_path = "./f3_extracted";
    }

    return 0;
}

static int is_ui_art_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;
    
    if (strcasecmp(ext, ".dds") != 0 && strcasecmp(ext, ".tga") != 0) {
        return 0;
    }
    
    if (strncasecmp(filename, "textures\\", 9) == 0) {
        return 1;
    }
    
    return 0;
}

static int is_audio_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;
    
    if (strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".mp3") == 0 ||
        strcasecmp(ext, ".ogg") == 0 || strcasecmp(ext, ".xwm") == 0) {
        return 1;
    }
    
    return 0;
}

static int process_bsa_file(const char *bsa_path, AppOptions *opts);

static int list_bsa_files(const char *f3_path, AppOptions *opts)
{
    char bsa_path[512];
    const char *bsa_files[] = {
        "Fallout - Sound.bsa",
        "Fallout - Voices.bsa",
        "Fallout - MenuVoices.bsa",
        "Fallout - Textures.bsa",
        "Fallout - Meshes.bsa",
        "Fallout - Misc.bsa",
        NULL
    };
    
    for (int i = 0; bsa_files[i]; i++) {
        snprintf(bsa_path, sizeof(bsa_path), "%s/Data/%s", f3_path, bsa_files[i]);
        
        if (!file_exists(bsa_path)) {
            continue;
        }
        
        printf("\n=== %s ===\n", bsa_files[i]);
        process_bsa_file(bsa_path, opts);
    }
    
    return 0;
}

static int process_bsa_file(const char *bsa_path, AppOptions *opts)
{
    BSArchive *archive;
    char **files = NULL;
    int file_count;
    
    if (opts->verbose) {
        printf("Processing BSA: %s\n", bsa_path);
    }
    
    if (bsa_open(&archive, bsa_path) != 0) {
        fprintf(stderr, "Failed to open BSA: %s\n", bsa_path);
        return -1;
    }
    
    file_count = bsa_list_files(archive, &files);
    if (file_count < 0) {
        bsa_close(archive);
        return -1;
    }
    
    if (opts->verbose) {
        printf("  Found %d files\n", file_count);
    }
    
    for (int i = 0; i < file_count; i++) {
        if (opts->verbose) {
            printf("    %s\n", files[i]);
        }
        free(files[i]);
    }
    
    free(files);
    bsa_close(archive);
    
    return 0;
}

static int extract_esm_files(const char *f3_path, const char *output_path)
{
    char esm_path[512];
    const char *esm_files[] = {
        "Fallout3.esm",
        "Anchorage.esm",
        "BrokenSteel.esm",
        "PointLookout.esm",
        "ThePitt.esm",
        "Zeta.esm",
        NULL
    };
    
    create_directory_recursive(output_path);
    
    for (int i = 0; esm_files[i]; i++) {
        snprintf(esm_path, sizeof(esm_path), "%s/Data/%s", f3_path, esm_files[i]);
        
        if (!file_exists(esm_path)) {
            continue;
        }
        
        char out_path[512];
        snprintf(out_path, sizeof(out_path), "%s/%s", output_path, esm_files[i]);
        
        FILE *in = fopen(esm_path, "rb");
        if (!in) continue;
        
        fseek(in, 0, SEEK_END);
        long size = ftell(in);
        fseek(in, 0, SEEK_SET);
        
        uint8_t *data = (uint8_t *)malloc(size);
        if (data && fread(data, 1, size, in) == (size_t)size) {
            FILE *out = fopen(out_path, "wb");
            if (out) {
                fwrite(data, 1, size, out);
                fclose(out);
                printf("Extracted: %s (%ld bytes)\n", esm_files[i], size);
            }
        }
        
        free(data);
        fclose(in);
    }
    
    return 0;
}

static int convert_audio_to_ogg_ffmpeg(const uint8_t *data, size_t data_size, 
                                       uint8_t **out_ogg, size_t *out_size,
                                       const char *original_path)
{
    char input_file[64];
    char output_file[64];
    char command[1024];
    
    snprintf(input_file, sizeof(input_file), "/tmp/f3tool_audio_in_%d.wav", (int)time(NULL));
    snprintf(output_file, sizeof(output_file), "/tmp/f3tool_audio_out_%d.ogg", (int)time(NULL));
    
    FILE *f = fopen(input_file, "wb");
    if (!f) return -1;
    fwrite(data, 1, data_size, f);
    fclose(f);
    
    snprintf(command, sizeof(command),
             "ffmpeg -y -i %s -c:a libvorbis -q:a 4 -ac 2 %s 2>/dev/null",
             input_file, output_file);
    
    int ret = system(command);
    
    unlink(input_file);
    
    if (ret != 0) {
        unlink(output_file);
        return -1;
    }
    
    f = fopen(output_file, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    *out_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    *out_ogg = (uint8_t *)malloc(*out_size);
    if (*out_ogg) {
        fread(*out_ogg, 1, *out_size, f);
    }
    fclose(f);
    
    unlink(output_file);
    
    return (*out_ogg && *out_size > 0) ? 0 : -1;
}

static int convert_mp3_to_ogg_ffmpeg(const uint8_t *data, size_t data_size,
                                     uint8_t **out_ogg, size_t *out_size)
{
    char input_file[64];
    char output_file[64];
    char command[1024];
    
    snprintf(input_file, sizeof(input_file), "/tmp/f3tool_music_in_%d.mp3", (int)time(NULL));
    snprintf(output_file, sizeof(output_file), "/tmp/f3tool_music_out_%d.ogg", (int)time(NULL));
    
    FILE *f = fopen(input_file, "wb");
    if (!f) return -1;
    fwrite(data, 1, data_size, f);
    fclose(f);
    
    snprintf(command, sizeof(command),
             "ffmpeg -y -i %s -c:a libvorbis -q:a 4 -ac 2 %s 2>/dev/null",
             input_file, output_file);
    
    int ret = system(command);
    
    unlink(input_file);
    
    if (ret != 0) {
        unlink(output_file);
        return -1;
    }
    
    f = fopen(output_file, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    *out_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    *out_ogg = (uint8_t *)malloc(*out_size);
    if (*out_ogg) {
        fread(*out_ogg, 1, *out_size, f);
    }
    fclose(f);
    
    unlink(output_file);
    
    return (*out_ogg && *out_size > 0) ? 0 : -1;
}

static int extract_music_folder(CapitalArchive *archive, const char *f3_path, int *music_count)
{
    char music_dir[512];
    snprintf(music_dir, sizeof(music_dir), "%s/Data/Music", f3_path);
    
    if (!file_exists(music_dir)) {
        *music_count = 0;
        return 0;
    }
    
    printf("Extracting music...\n");
    
    char find_cmd[1024];
    snprintf(find_cmd, sizeof(find_cmd), "find \"%s\" -type f -name \"*.mp3\" 2>/dev/null", music_dir);
    
    FILE *fp = popen(find_cmd, "r");
    if (!fp) return -1;
    
    char line[1024];
    int count = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        
        FILE *mp3_f = fopen(line, "rb");
        if (!mp3_f) continue;
        
        fseek(mp3_f, 0, SEEK_END);
        long size = ftell(mp3_f);
        fseek(mp3_f, 0, SEEK_SET);
        
        uint8_t *data = (uint8_t *)malloc(size);
        if (data && fread(data, 1, size, mp3_f) == (size_t)size) {
            uint8_t *ogg_data = NULL;
            size_t ogg_size = 0;
            
            if (convert_mp3_to_ogg_ffmpeg(data, size, &ogg_data, &ogg_size) == 0) {
                char relative_path[512];
                const char *music_ptr = strstr(line, "Music/");
                if (music_ptr) {
                    snprintf(relative_path, sizeof(relative_path), "music/%s", music_ptr + 6);
                    char *ext = strrchr(relative_path, '.');
                    if (ext) strcpy(ext, ".ogg");
                    
                    if (capital_add_audio(archive, relative_path, ogg_data, ogg_size, 44100, 2, 16) == 0) {
                        printf("  Music: %s\n", relative_path);
                        count++;
                    }
                }
                free(ogg_data);
            }
            free(data);
        }
        fclose(mp3_f);
    }
    
    pclose(fp);
    *music_count = count;
    return 0;
}

static int convert_bik_to_mpg_ffmpeg(const uint8_t *data, size_t data_size,
                                     uint8_t **out_mpg, size_t *out_size)
{
    char input_file[64];
    char output_file[64];
    char command[1024];
    
    snprintf(input_file, sizeof(input_file), "/tmp/f3tool_video_in_%d.bik", (int)time(NULL));
    snprintf(output_file, sizeof(output_file), "/tmp/f3tool_video_out_%d.mpg", (int)time(NULL));
    
    FILE *f = fopen(input_file, "wb");
    if (!f) return -1;
    fwrite(data, 1, data_size, f);
    fclose(f);
    
    snprintf(command, sizeof(command),
             "ffmpeg -y -i %s -c:v mpeg1video -c:a mp2 -b:v 2M -b:a 192k -qscale:v 2 %s 2>/dev/null",
             input_file, output_file);
    
    int ret = system(command);
    
    unlink(input_file);
    
    if (ret != 0) {
        unlink(output_file);
        return -1;
    }
    
    f = fopen(output_file, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    *out_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    *out_mpg = (uint8_t *)malloc(*out_size);
    if (*out_mpg) {
        fread(*out_mpg, 1, *out_size, f);
    }
    fclose(f);
    
    unlink(output_file);
    
    return (*out_mpg && *out_size > 0) ? 0 : -1;
}

static int extract_video_folder(CapitalArchive *archive, const char *f3_path, int *video_count)
{
    char video_dir[512];
    snprintf(video_dir, sizeof(video_dir), "%s/Data/Video", f3_path);
    
    if (!file_exists(video_dir)) {
        *video_count = 0;
        return 0;
    }
    
    printf("Extracting video...\n");
    
    char find_cmd[1024];
    snprintf(find_cmd, sizeof(find_cmd), "find \"%s\" -type f -name \"*.bik\" 2>/dev/null", video_dir);
    
    FILE *fp = popen(find_cmd, "r");
    if (!fp) return -1;
    
    char line[1024];
    int count = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        
        FILE *bik_f = fopen(line, "rb");
        if (!bik_f) continue;
        
        fseek(bik_f, 0, SEEK_END);
        long size = ftell(bik_f);
        fseek(bik_f, 0, SEEK_SET);
        
        uint8_t *data = (uint8_t *)malloc(size);
        if (data && fread(data, 1, size, bik_f) == (size_t)size) {
            uint8_t *mpg_data = NULL;
            size_t mpg_size = 0;
            
            if (convert_bik_to_mpg_ffmpeg(data, size, &mpg_data, &mpg_size) == 0) {
                char relative_path[512];
                const char *video_ptr = strstr(line, "Video/");
                if (video_ptr) {
                    snprintf(relative_path, sizeof(relative_path), "video/%s", video_ptr + 6);
                    char *ext = strrchr(relative_path, '.');
                    if (ext) strcpy(ext, ".mpg");
                    
                    if (capital_add_video(archive, relative_path, mpg_data, mpg_size, 0, 0) == 0) {
                        printf("  Video: %s\n", relative_path);
                        count++;
                    }
                }
                free(mpg_data);
            }
            free(data);
        }
        fclose(bik_f);
    }
    
    pclose(fp);
    *video_count = count;
    return 0;
}

static int convert_dds_to_png_ffmpeg(const uint8_t *data, size_t data_size,
                                     uint8_t **out_png, size_t *out_size)
{
    char input_file[64];
    char output_file[64];
    char command[1024];
    
    snprintf(input_file, sizeof(input_file), "/tmp/f3tool_img_in_%d.dds", (int)time(NULL));
    snprintf(output_file, sizeof(output_file), "/tmp/f3tool_img_out_%d.png", (int)time(NULL));
    
    FILE *f = fopen(input_file, "wb");
    if (!f) return -1;
    fwrite(data, 1, data_size, f);
    fclose(f);
    
    snprintf(command, sizeof(command),
             "ffmpeg -y -i %s -compression_level 9 -pred median %s 2>/dev/null",
             input_file, output_file);
    
    int ret = system(command);
    
    unlink(input_file);
    
    if (ret != 0) {
        unlink(output_file);
        return -1;
    }
    
    f = fopen(output_file, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    *out_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    *out_png = (uint8_t *)malloc(*out_size);
    if (*out_png) {
        fread(*out_png, 1, *out_size, f);
    }
    fclose(f);
    
    unlink(output_file);
    
    return (*out_png && *out_size > 0) ? 0 : -1;
}

static int build_capital_archive(const char *f3_path, const char *output_path, AppOptions *opts)
{
    CapitalArchive archive;
    char archive_path[512];
    
    snprintf(archive_path, sizeof(archive_path), "%s.capital", output_path);
    
    if (capital_create(archive_path, &archive) != 0) {
        fprintf(stderr, "Failed to create archive: %s\n", archive_path);
        return -1;
    }
    
    if (opts->extract_esm) {
        printf("Adding ESM files...\n");
        const char *esm_files[] = {
            "Fallout3.esm",
            "Anchorage.esm",
            "BrokenSteel.esm",
            "PointLookout.esm",
            "ThePitt.esm",
            "Zeta.esm",
            NULL
        };
        
        for (int i = 0; esm_files[i]; i++) {
            char esm_path[512];
            snprintf(esm_path, sizeof(esm_path), "%s/Data/%s", f3_path, esm_files[i]);
            
            if (!file_exists(esm_path)) {
                continue;
            }
            
            FILE *f = fopen(esm_path, "rb");
            if (!f) continue;
            
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            
            uint8_t *data = (uint8_t *)malloc(size);
            if (data && fread(data, 1, size, f) == (size_t)size) {
                if (capital_add_esm(&archive, esm_files[i], data, size) == 0) {
                    if (opts->verbose) {
                        printf("  Added ESM: %s (%ld bytes)\n", esm_files[i], size);
                    }
                }
                free(data);
            }
            fclose(f);
        }
    }
    
    if (opts->extract_audio) {
        char maintitle_path[512];
        snprintf(maintitle_path, sizeof(maintitle_path), "%s/MainTitle.wav", f3_path);
        
        if (file_exists(maintitle_path)) {
            FILE *f = fopen(maintitle_path, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);
                
                uint8_t *data = (uint8_t *)malloc(size);
                if (data && fread(data, 1, size, f) == (size_t)size) {
                    uint8_t *ogg_data = NULL;
                    size_t ogg_size = 0;
                    
                    if (convert_audio_to_ogg_ffmpeg(data, size, &ogg_data, &ogg_size, "MainTitle.wav") == 0) {
                        if (capital_add_audio(&archive, "MainTitle.ogg", ogg_data, ogg_size, 44100, 2, 16) == 0) {
                            printf("  Added: MainTitle.ogg\n");
                        }
                        free(ogg_data);
                    }
                }
                free(data);
                fclose(f);
            }
        }
    }
    
    int music_count = 0;
    if (opts->extract_audio && opts->limit == 0) {
        extract_music_folder(&archive, f3_path, &music_count);
    }
    
    int video_count = 0;
    if (opts->extract_video && opts->limit == 0) {
        extract_video_folder(&archive, f3_path, &video_count);
    }
    
    int audio_count = 0;
    int art_count = 0;
    int total_audio = 0;
    int total_art = 0;
    
    if (opts->extract_audio || opts->extract_art) {
        char bsa_path[512];
        const char *bsa_files[] = {
            "Fallout - Sound.bsa",
            "Fallout - Voices.bsa",
            "Fallout - MenuVoices.bsa",
            "Fallout - Textures.bsa",
            "Fallout - Misc.bsa",
            NULL
        };
        
        printf("Scanning for files to extract...\n");
        
        for (int i = 0; bsa_files[i]; i++) {
            snprintf(bsa_path, sizeof(bsa_path), "%s/Data/%s", f3_path, bsa_files[i]);
            
            if (!file_exists(bsa_path)) {
                continue;
            }
            
            BSArchive *bsa;
            if (bsa_open(&bsa, bsa_path) != 0) {
                continue;
            }
            
            char **files = NULL;
            int file_count = bsa_list_files(bsa, &files);
            
            for (int j = 0; j < file_count; j++) {
                const char *filename = files[j];
                
                if (opts->extract_audio && is_audio_file(filename)) {
                    total_audio++;
                } else if (opts->extract_art && is_ui_art_file(filename)) {
                    total_art++;
                }
                free(files[j]);
            }
            
            free(files);
            bsa_close(bsa);
        }
        
        if (opts->limit > 0) {
            total_audio = (total_audio < opts->limit) ? total_audio : opts->limit;
            total_art = (total_art < opts->limit) ? total_art : opts->limit;
        }
        
        printf("Found: %d audio files, %d art files\n", total_audio, total_art);
        printf("\nExtracting assets...\n");
        printf("Audio: [%3d/%3d]\n", 0, total_audio);
        printf("Art:   [%3d/%3d]\n", 0, total_art);
        
        int last_audio_pct = 0;
        int last_art_pct = 0;
        
        for (int i = 0; bsa_files[i]; i++) {
            snprintf(bsa_path, sizeof(bsa_path), "%s/Data/%s", f3_path, bsa_files[i]);
            
            if (!file_exists(bsa_path)) {
                continue;
            }
            
            BSArchive *bsa;
            if (bsa_open(&bsa, bsa_path) != 0) {
                continue;
            }
            
            char **files = NULL;
            int file_count = bsa_list_files(bsa, &files);
            
            for (int j = 0; j < file_count; j++) {
                const char *filename = files[j];
                
                if (opts->extract_audio && is_audio_file(filename)) {
                    if (opts->limit > 0 && audio_count >= opts->limit) {
                        free(files[j]);
                        continue;
                    }
                    
                    uint8_t *data;
                    size_t data_size;
                    
                    if (bsa_extract_file(bsa, filename, &data, &data_size) == 0) {
                        uint8_t *ogg_data = NULL;
                        size_t ogg_size = 0;
                        
                        if (convert_audio_to_ogg_ffmpeg(data, data_size, &ogg_data, &ogg_size, filename) == 0) {
                            char ogg_path[512];
                            snprintf(ogg_path, sizeof(ogg_path), "%s.ogg", filename);
                            
                            if (capital_add_audio(&archive, ogg_path, ogg_data, ogg_size, 44100, 1, 16) == 0) {
                                audio_count++;
                            }
                            free(ogg_data);
                        } else {
                            if (capital_add_audio(&archive, filename, data, data_size, 44100, 2, 16) == 0) {
                                audio_count++;
                            }
                        }
                        free(data);
                    }
                    
                    int pct = (total_audio > 0) ? (audio_count * 100 / total_audio) : 100;
                    if (pct != last_audio_pct && pct % 10 == 0) {
                        printf("\rAudio: [%3d/%3d] %3d%%", audio_count, total_audio, pct);
                        fflush(stdout);
                        last_audio_pct = pct;
                    }
                } else if (opts->extract_art && is_ui_art_file(filename)) {
                    if (opts->limit > 0 && art_count >= opts->limit) {
                        free(files[j]);
                        continue;
                    }
                    
                    uint8_t *data;
                    size_t data_size;
                    
                    if (bsa_extract_file(bsa, filename, &data, &data_size) == 0) {
                        if (string_ends_with(filename, ".dds")) {
                            uint8_t *png_data = NULL;
                            size_t png_size = 0;
                            
                            if (convert_dds_to_png_ffmpeg(data, data_size, &png_data, &png_size) == 0) {
                                char png_path[512];
                                snprintf(png_path, sizeof(png_path), "%s.png", filename);
                                
                                if (capital_add_art(&archive, png_path, png_data, png_size, 0, 0, 0) == 0) {
                                    art_count++;
                                }
                                free(png_data);
                            } else if (capital_add_art(&archive, filename, data, data_size, 0, 0, 0) == 0) {
                                art_count++;
                            }
                        } else {
                            if (capital_add_art(&archive, filename, data, data_size, 0, 0, 0) == 0) {
                                art_count++;
                            }
                        }
                        free(data);
                    }
                    
                    int pct = (total_art > 0) ? (art_count * 100 / total_art) : 100;
                    if (pct != last_art_pct && pct % 10 == 0) {
                        printf("\rArt:   [%3d/%3d] %3d%%", art_count, total_art, pct);
                        fflush(stdout);
                        last_art_pct = pct;
                    }
                }
                
                free(files[j]);
            }
            
            free(files);
            bsa_close(bsa);
        }
        
        printf("\n");
    }
    
    printf("Finalizing archive...\n");
    if (capital_finalize(&archive) != 0) {
        fprintf(stderr, "Failed to finalize archive\n");
        capital_close(&archive);
        return -1;
    }
    
    capital_close(&archive);
    
    printf("\n=== Extraction Complete ===\n");
    printf("Archive: %s\n", archive_path);
    printf("Video:   %d files\n", video_count);
    printf("Music:   %d files\n", music_count);
    printf("Audio:   %d files\n", audio_count);
    printf("Art:     %d files\n", art_count);
    
    return 0;
}

int main(int argc, char **argv)
{
    AppOptions opts;
    
    if (parse_args(argc, argv, &opts) != 0) {
        return 1;
    }
    
    printf("Fallout 3 Capital Archive Builder\n");
    printf("=================================\n\n");
    printf("Game path: %s\n", opts.f3_path);
    printf("Output:    %s.capital\n\n", opts.output_path);
    
    if (!file_exists(opts.f3_path)) {
        fprintf(stderr, "Error: Path does not exist: %s\n", opts.f3_path);
        return 1;
    }
    
    if (opts.list_only) {
        printf("Listing all files in Fallout 3 archives...\n\n");
        return list_bsa_files(opts.f3_path, &opts);
    }
    
    create_directory_recursive(opts.output_path);
    
    if (opts.extract_esm || opts.extract_audio || opts.extract_art || opts.extract_video) {
        printf("\n--- Building Capital Archive ---\n");
        build_capital_archive(opts.f3_path, opts.output_path, &opts);
    }
    
    printf("\nArchive creation complete!\n");
    printf("Output: %s.capital\n", opts.output_path);
    
    return 0;
}