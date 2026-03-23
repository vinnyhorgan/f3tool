#include "archive.h"
#include <stdlib.h>
#include <string.h>

int capital_viewer_open(CapitalArchive *archive, const char *path)
{
    memset(archive, 0, sizeof(*archive));
    
    archive->fp = fopen(path, "rb");
    if (!archive->fp) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return -1;
    }
    
    if (fread(&archive->header, sizeof(CapitalHeader), 1, archive->fp) != 1) {
        fprintf(stderr, "Failed to read header\n");
        fclose(archive->fp);
        return -1;
    }
    
    if (archive->header.magic != 0x43415021) {
        fprintf(stderr, "Invalid magic: 0x%X expected 0x%X\n", archive->header.magic, 0x43415021);
        fclose(archive->fp);
        return -1;
    }
    
    archive->files = malloc(sizeof(CapitalFileInfo) * archive->header.entry_count);
    if (!archive->files) {
        fclose(archive->fp);
        return -1;
    }
    
    size_t index_size = 0;
    for (uint32_t i = 0; i < archive->header.entry_count; i++) {
        uint8_t type;
        uint16_t path_len;
        uint32_t data_size, offset;
        
        if (fread(&type, 1, 1, archive->fp) != 1) break;
        if (fread(&path_len, 2, 1, archive->fp) != 1) break;
        if (fread(&data_size, 4, 1, archive->fp) != 1) break;
        if (fread(&offset, 4, 1, archive->fp) != 1) break;
        
        archive->files[i].entry.type = type;
        archive->files[i].entry.path_length = path_len;
        archive->files[i].entry.data_size = data_size;
        archive->files[i].entry.offset = offset;
        
        size_t metadata_size = 0;
        if (type == 1) {
            metadata_size = 4;
            fread(&archive->files[i].entry.metadata.esm.flags, 4, 1, archive->fp);
        } else if (type == 2) {
            metadata_size = 8;
            fread(&archive->files[i].entry.metadata.audio.sample_rate, 8, 1, archive->fp);
        } else if (type == 3) {
            metadata_size = 12;
            fread(&archive->files[i].entry.metadata.art.width, 12, 1, archive->fp);
        } else if (type == 4) {
            metadata_size = 8;
            fread(&archive->files[i].entry.metadata.video.duration_ms, 8, 1, archive->fp);
        }
        
        archive->files[i].path = malloc(path_len + 1);
        if (archive->files[i].path) {
            fread(archive->files[i].path, 1, path_len, archive->fp);
            archive->files[i].path[path_len] = '\0';
        }
        
        index_size += 1 + 2 + 4 + 4 + metadata_size + path_len;
    }
    
    size_t data_start = sizeof(CapitalHeader) + index_size;
    
    archive->data_size = 0;
    for (uint32_t i = 0; i < archive->header.entry_count; i++) {
        archive->data_size += archive->files[i].entry.data_size;
    }
    
    archive->file_data = malloc(archive->data_size);
    if (archive->file_data) {
        fseek(archive->fp, data_start, SEEK_SET);
        fread(archive->file_data, 1, archive->data_size, archive->fp);
        
        size_t cumulative_offset = 0;
        for (uint32_t i = 0; i < archive->header.entry_count; i++) {
            archive->files[i].entry.offset = cumulative_offset;
            cumulative_offset += archive->files[i].entry.data_size;
        }
    }
    
    return 0;
}

void capital_viewer_close(CapitalArchive *archive)
{
    if (!archive) return;
    
    if (archive->fp) {
        fclose(archive->fp);
        archive->fp = NULL;
    }
    
    if (archive->files) {
        for (uint32_t i = 0; i < archive->header.entry_count; i++) {
            free(archive->files[i].path);
        }
        free(archive->files);
        archive->files = NULL;
    }
    
    free(archive->file_data);
    archive->file_data = NULL;
}

int capital_viewer_get_file(CapitalArchive *archive, int index, uint8_t **data, size_t *size)
{
    if (!archive || index < 0 || (uint32_t)index >= archive->header.entry_count) {
        return -1;
    }
    
    if (!archive->file_data) {
        return -1;
    }
    
    CapitalFileInfo *file = &archive->files[index];
    
    if (file->entry.offset >= archive->data_size) {
        return -1;
    }
    
    if (file->entry.offset + file->entry.data_size > archive->data_size) {
        return -1;
    }
    
    *data = archive->file_data + file->entry.offset;
    *size = file->entry.data_size;
    
    return 0;
}

const char *capital_viewer_get_type_name(uint8_t type)
{
    switch (type) {
        case 1: return "ESM";
        case 2: return "Audio";
        case 3: return "Art";
        case 4: return "Video";
        default: return "Unknown";
    }
}

const char *capital_viewer_get_extension(uint8_t type)
{
    switch (type) {
        case 1: return ".esm";
        case 2: return ".ogg";
        case 3: return ".png";
        case 4: return ".mpg";
        default: return ".bin";
    }
}
