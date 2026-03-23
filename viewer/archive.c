#include "archive.h"
#include <stdlib.h>
#include <string.h>

int capital_viewer_open(CapitalArchive *archive, const char *path)
{
    memset(archive, 0, sizeof(*archive));
    
    archive->fp = fopen(path, "rb");
    if (!archive->fp) {
        return -1;
    }
    
    if (fread(&archive->header, sizeof(CapitalHeader), 1, archive->fp) != 1) {
        fclose(archive->fp);
        return -1;
    }
    
    if (archive->header.magic != 0x43415021) {
        fclose(archive->fp);
        return -1;
    }
    
    archive->files = malloc(sizeof(CapitalFileInfo) * archive->header.entry_count);
    if (!archive->files) {
        fclose(archive->fp);
        return -1;
    }
    
    for (uint32_t i = 0; i < archive->header.entry_count; i++) {
        uint8_t type;
        uint16_t path_len;
        uint32_t data_size, offset;
        uint32_t meta[3];
        
        if (fread(&type, 1, 1, archive->fp) != 1) break;
        if (fread(&path_len, 2, 1, archive->fp) != 1) break;
        if (fread(&data_size, 4, 1, archive->fp) != 1) break;
        if (fread(&offset, 4, 1, archive->fp) != 1) break;
        if (fread(meta, 12, 1, archive->fp) != 1) break;
        
        archive->files[i].entry.type = type;
        archive->files[i].entry.path_length = path_len;
        archive->files[i].entry.data_size = data_size;
        archive->files[i].entry.offset = offset;
        
        if (type == 1) {
            archive->files[i].entry.metadata.esm.flags = meta[0];
        } else if (type == 2) {
            archive->files[i].entry.metadata.audio.sample_rate = meta[0];
            archive->files[i].entry.metadata.audio.channels = (uint16_t)meta[1];
            archive->files[i].entry.metadata.audio.bits_per_sample = (uint16_t)(meta[1] >> 16);
        } else if (type == 3) {
            archive->files[i].entry.metadata.art.width = meta[0];
            archive->files[i].entry.metadata.art.height = meta[1];
            archive->files[i].entry.metadata.art.format = meta[2];
        } else if (type == 4) {
            archive->files[i].entry.metadata.video.duration_ms = meta[0];
            archive->files[i].entry.metadata.video.reserved = meta[1];
        }
        
        archive->files[i].path = malloc(path_len + 1);
        if (archive->files[i].path) {
            if (fread(archive->files[i].path, 1, path_len, archive->fp) != path_len) {
                free(archive->files[i].path);
                archive->files[i].path = NULL;
                break;
            }
            archive->files[i].path[path_len] = '\0';
        }
    }
    
    archive->file_data = NULL;
    archive->data_size = 0;
    
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
    
    CapitalFileInfo *file = &archive->files[index];
    
    uint8_t *buffer = malloc(file->entry.data_size);
    if (!buffer) return -1;
    
    if (fseek(archive->fp, file->entry.offset, SEEK_SET) != 0) {
        free(buffer);
        return -1;
    }
    
    if (fread(buffer, 1, file->entry.data_size, archive->fp) != file->entry.data_size) {
        free(buffer);
        return -1;
    }
    
    *data = buffer;
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
