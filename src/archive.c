#include "../include/archive.h"
#include "../include/util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(push, 1)

typedef struct {
    uint8_t type;
    uint16_t path_length;
    uint32_t data_size;
    uint32_t offset;
    union {
        struct {
            uint32_t flags;
        } esm;
        struct {
            uint32_t sample_rate;
            uint16_t channels;
            uint16_t bits_per_sample;
        } audio;
        struct {
            uint32_t width;
            uint32_t height;
            uint32_t format;
        } art;
        struct {
            uint32_t duration_ms;
            uint32_t reserved;
        } video;
    } metadata;
} CapitalEntryInternal;

#pragma pack(pop)

struct CapitalArchiveInternal {
    FILE *fp;
    CapitalHeader header;
    CapitalEntryInternal *entries;
    char **paths;
    uint8_t **datas;
    size_t entry_count;
    size_t entry_capacity;
    size_t data_start_offset;
};

static size_t calculate_header_size(CapitalArchive *archive)
{
    (void)archive;
    return sizeof(CapitalHeader);
}

static size_t calculate_index_size(CapitalArchive *archive)
{
    struct CapitalArchiveInternal *ai = (struct CapitalArchiveInternal *)archive->_internal;
    size_t size = 0;
    for (size_t i = 0; i < ai->entry_count; i++) {
        size += sizeof(CapitalEntryInternal);
        size += ai->paths[i] ? strlen(ai->paths[i]) : 0;
    }
    return size;
}

int capital_create(const char *path, CapitalArchive *archive)
{
    memset(archive, 0, sizeof(*archive));
    
    struct CapitalArchiveInternal *ai = (struct CapitalArchiveInternal *)calloc(1, sizeof(struct CapitalArchiveInternal));
    if (!ai) return -1;
    
    archive->_internal = ai;
    
    ai->fp = fopen(path, "wb");
    if (!ai->fp) {
        free(ai);
        return -1;
    }

    ai->header.magic = CAPITAL_MAGIC;
    ai->header.version = CAPITAL_VERSION;
    ai->header.entry_count = 0;
    ai->header.flags = 0;
    memset(ai->header.reserved, 0, sizeof(ai->header.reserved));

    if (fwrite(&ai->header, sizeof(CapitalHeader), 1, ai->fp) != 1) {
        fclose(ai->fp);
        free(ai);
        return -1;
    }

    ai->entries = NULL;
    ai->paths = NULL;
    ai->datas = NULL;
    ai->entry_count = 0;
    ai->entry_capacity = 0;
    
    ai->data_start_offset = sizeof(CapitalHeader);
    
    return 0;
}

int capital_open(CapitalArchive *archive, const char *path, int read_only)
{
    memset(archive, 0, sizeof(*archive));
    
    struct CapitalArchiveInternal *ai = (struct CapitalArchiveInternal *)calloc(1, sizeof(struct CapitalArchiveInternal));
    if (!ai) return -1;
    
    archive->_internal = ai;
    
    ai->fp = fopen(path, read_only ? "rb" : "r+b");
    if (!ai->fp) {
        free(ai);
        return -1;
    }

    if (fread(&ai->header, sizeof(CapitalHeader), 1, ai->fp) != 1) {
        fclose(ai->fp);
        free(ai);
        return -1;
    }

    if (ai->header.magic != CAPITAL_MAGIC) {
        fprintf(stderr, "Not a CAP file: magic 0x%X, expected 0x%X\n",
                ai->header.magic, CAPITAL_MAGIC);
        fclose(ai->fp);
        free(ai);
        return -1;
    }

    ai->entry_count = ai->header.entry_count;
    ai->entry_capacity = ai->entry_count;
    
    ai->entries = (CapitalEntryInternal *)malloc(sizeof(CapitalEntryInternal) * ai->entry_count);
    ai->paths = (char **)malloc(sizeof(char *) * ai->entry_count);
    ai->datas = (uint8_t **)malloc(sizeof(uint8_t *) * ai->entry_count);

    if (!ai->entries || !ai->paths || !ai->datas) {
        free(ai->entries);
        free(ai->paths);
        free(ai->datas);
        fclose(ai->fp);
        free(ai);
        return -1;
    }

    size_t offset = sizeof(CapitalHeader);
    for (size_t i = 0; i < ai->entry_count; i++) {
        if (fread(&ai->entries[i], sizeof(CapitalEntryInternal), 1, ai->fp) != 1) {
            for (size_t j = 0; j < i; j++) {
                free(ai->paths[j]);
                free(ai->datas[j]);
            }
            free(ai->entries);
            free(ai->paths);
            free(ai->datas);
            fclose(ai->fp);
            free(ai);
            return -1;
        }
        
        ai->paths[i] = (char *)malloc(ai->entries[i].path_length + 1);
        ai->datas[i] = NULL;
        
        if (!ai->paths[i]) {
            for (size_t j = 0; j <= i; j++) {
                free(ai->paths[j]);
            }
            free(ai->entries);
            free(ai->paths);
            free(ai->datas);
            fclose(ai->fp);
            free(ai);
            return -1;
        }

        if (fread(ai->paths[i], 1, ai->entries[i].path_length, ai->fp) != (size_t)ai->entries[i].path_length) {
            for (size_t j = 0; j <= i; j++) {
                free(ai->paths[j]);
            }
            free(ai->entries);
            free(ai->paths);
            free(ai->datas);
            fclose(ai->fp);
            free(ai);
            return -1;
        }
        ai->paths[i][ai->entries[i].path_length] = '\0';
        
        offset += sizeof(CapitalEntryInternal) + ai->entries[i].path_length;
    }
    
    ai->data_start_offset = offset;

    return 0;
}

void capital_close(CapitalArchive *archive)
{
    if (!archive || !archive->_internal) return;
    
    struct CapitalArchiveInternal *ai = (struct CapitalArchiveInternal *)archive->_internal;

    if (ai->fp) {
        fclose(ai->fp);
        ai->fp = NULL;
    }

    if (ai->entries) {
        free(ai->entries);
        ai->entries = NULL;
    }

    if (ai->paths) {
        for (size_t i = 0; i < ai->entry_count; i++) {
            if (ai->paths[i]) {
                free(ai->paths[i]);
            }
        }
        free(ai->paths);
        ai->paths = NULL;
    }
    
    if (ai->datas) {
        for (size_t i = 0; i < ai->entry_count; i++) {
            if (ai->datas[i]) {
                free(ai->datas[i]);
            }
        }
        free(ai->datas);
        ai->datas = NULL;
    }
    
    free(ai);
    archive->_internal = NULL;
}

static int ensure_capacity(CapitalArchive *archive)
{
    struct CapitalArchiveInternal *ai = (struct CapitalArchiveInternal *)archive->_internal;
    
    if (ai->entry_count < ai->entry_capacity) return 0;
    
    size_t new_cap = ai->entry_capacity == 0 ? 16 : ai->entry_capacity * 2;
    
    CapitalEntryInternal *new_entries = (CapitalEntryInternal *)realloc(ai->entries, sizeof(CapitalEntryInternal) * new_cap);
    char **new_paths = (char **)realloc(ai->paths, sizeof(char *) * new_cap);
    uint8_t **new_datas = (uint8_t **)realloc(ai->datas, sizeof(uint8_t *) * new_cap);
    
    if (!new_entries || !new_paths || !new_datas) {
        free(new_entries);
        free(new_paths);
        free(new_datas);
        return -1;
    }
    
    ai->entries = new_entries;
    ai->paths = new_paths;
    ai->datas = new_datas;
    ai->entry_capacity = new_cap;
    
    return 0;
}

int capital_add_esm(CapitalArchive *archive, const char *esm_path, const uint8_t *data, size_t data_size)
{
    struct CapitalArchiveInternal *ai = (struct CapitalArchiveInternal *)archive->_internal;
    
    if (ensure_capacity(archive) != 0) return -1;
    
    size_t idx = ai->entry_count;
    
    ai->entries[idx].type = CAPITAL_TYPE_ESM;
    ai->entries[idx].path_length = (uint16_t)strlen(esm_path);
    ai->entries[idx].data_size = (uint32_t)data_size;
    ai->entries[idx].offset = 0;
    ai->entries[idx].metadata.esm.flags = 0;
    
    ai->paths[idx] = strdup(esm_path);
    ai->datas[idx] = (uint8_t *)malloc(data_size);
    
    if (!ai->paths[idx] || !ai->datas[idx]) {
        free(ai->paths[idx]);
        free(ai->datas[idx]);
        return -1;
    }
    
    memcpy(ai->datas[idx], data, data_size);
    ai->entry_count++;
    
    return 0;
}

int capital_add_audio(CapitalArchive *archive, const char *original_path, const uint8_t *data, 
                      size_t data_size, uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample)
{
    struct CapitalArchiveInternal *ai = (struct CapitalArchiveInternal *)archive->_internal;
    
    if (ensure_capacity(archive) != 0) return -1;
    
    size_t idx = ai->entry_count;
    
    ai->entries[idx].type = CAPITAL_TYPE_AUDIO;
    ai->entries[idx].path_length = (uint16_t)strlen(original_path);
    ai->entries[idx].data_size = (uint32_t)data_size;
    ai->entries[idx].offset = 0;
    ai->entries[idx].metadata.audio.sample_rate = sample_rate;
    ai->entries[idx].metadata.audio.channels = channels;
    ai->entries[idx].metadata.audio.bits_per_sample = bits_per_sample;
    
    ai->paths[idx] = strdup(original_path);
    ai->datas[idx] = (uint8_t *)malloc(data_size);
    
    if (!ai->paths[idx] || !ai->datas[idx]) {
        free(ai->paths[idx]);
        free(ai->datas[idx]);
        return -1;
    }
    
    memcpy(ai->datas[idx], data, data_size);
    ai->entry_count++;
    
    return 0;
}

int capital_add_art(CapitalArchive *archive, const char *original_path, const uint8_t *data,
                    size_t data_size, uint32_t width, uint32_t height, uint32_t format)
{
    struct CapitalArchiveInternal *ai = (struct CapitalArchiveInternal *)archive->_internal;
    
    if (ensure_capacity(archive) != 0) return -1;
    
    size_t idx = ai->entry_count;
    
    ai->entries[idx].type = CAPITAL_TYPE_ART;
    ai->entries[idx].path_length = (uint16_t)strlen(original_path);
    ai->entries[idx].data_size = (uint32_t)data_size;
    ai->entries[idx].offset = 0;
    ai->entries[idx].metadata.art.width = width;
    ai->entries[idx].metadata.art.height = height;
    ai->entries[idx].metadata.art.format = format;
    
    ai->paths[idx] = strdup(original_path);
    ai->datas[idx] = (uint8_t *)malloc(data_size);
    
    if (!ai->paths[idx] || !ai->datas[idx]) {
        free(ai->paths[idx]);
        free(ai->datas[idx]);
        return -1;
    }
    
    memcpy(ai->datas[idx], data, data_size);
    ai->entry_count++;
    
    return 0;
}

int capital_add_video(CapitalArchive *archive, const char *original_path, const uint8_t *data,
                     size_t data_size, uint32_t width, uint32_t height)
{
    struct CapitalArchiveInternal *ai = (struct CapitalArchiveInternal *)archive->_internal;
    
    if (ensure_capacity(archive) != 0) return -1;
    
    size_t idx = ai->entry_count;
    
    ai->entries[idx].type = CAPITAL_TYPE_VIDEO;
    ai->entries[idx].path_length = (uint16_t)strlen(original_path);
    ai->entries[idx].data_size = (uint32_t)data_size;
    ai->entries[idx].offset = 0;
    ai->entries[idx].metadata.video.duration_ms = width;
    ai->entries[idx].metadata.video.reserved = height;
    
    ai->paths[idx] = strdup(original_path);
    ai->datas[idx] = (uint8_t *)malloc(data_size);
    
    if (!ai->paths[idx] || !ai->datas[idx]) {
        free(ai->paths[idx]);
        free(ai->datas[idx]);
        return -1;
    }
    
    memcpy(ai->datas[idx], data, data_size);
    ai->entry_count++;
    
    return 0;
}

int capital_finalize(CapitalArchive *archive)
{
    struct CapitalArchiveInternal *ai = (struct CapitalArchiveInternal *)archive->_internal;
    
    size_t index_size = calculate_index_size(archive);
    size_t current_offset = sizeof(CapitalHeader) + index_size;
    
    for (size_t i = 0; i < ai->entry_count; i++) {
        ai->entries[i].offset = (uint32_t)current_offset;
        current_offset += ai->entries[i].data_size;
    }
    
    rewind(ai->fp);
    ai->header.entry_count = (uint32_t)ai->entry_count;
    
    if (fwrite(&ai->header, sizeof(CapitalHeader), 1, ai->fp) != 1) {
        return -1;
    }
    
    for (size_t i = 0; i < ai->entry_count; i++) {
        if (fwrite(&ai->entries[i], sizeof(CapitalEntryInternal), 1, ai->fp) != 1) {
            return -1;
        }
        if (fwrite(ai->paths[i], 1, ai->entries[i].path_length, ai->fp) != (size_t)ai->entries[i].path_length) {
            return -1;
        }
    }
    
    for (size_t i = 0; i < ai->entry_count; i++) {
        if (fwrite(ai->datas[i], 1, ai->entries[i].data_size, ai->fp) != ai->entries[i].data_size) {
            return -1;
        }
    }
    
    fflush(ai->fp);
    
    return 0;
}

int capital_get_entry_count(CapitalArchive *archive)
{
    if (!archive || !archive->_internal) return 0;
    struct CapitalArchiveInternal *ai = (struct CapitalArchiveInternal *)archive->_internal;
    return (int)ai->entry_count;
}

int capital_get_entry(CapitalArchive *archive, int index, CapitalEntry *out_entry, 
                      char **out_path, uint8_t **out_data)
{
    if (!archive || !archive->_internal) return -1;
    struct CapitalArchiveInternal *ai = (struct CapitalArchiveInternal *)archive->_internal;
    
    if (index < 0 || (size_t)index >= ai->entry_count) {
        return -1;
    }
    
    if (out_entry) {
        memset(out_entry, 0, sizeof(CapitalEntry));
        out_entry->type = ai->entries[index].type;
        out_entry->path_length = ai->entries[index].path_length;
        out_entry->data_size = ai->entries[index].data_size;
        out_entry->offset = ai->entries[index].offset;
        memcpy(&out_entry->metadata, &ai->entries[index].metadata, sizeof(out_entry->metadata));
    }

    if (out_path) {
        *out_path = ai->paths[index];
    }

    if (out_data) {
        *out_data = ai->datas[index];
    }

    return 0;
}

int capital_extract_all(CapitalArchive *archive, const char *output_dir)
{
    (void)archive;
    (void)output_dir;
    return -1;
}