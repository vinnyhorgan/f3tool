#ifndef CAPITAL_VIEWER_ARCHIVE_H
#define CAPITAL_VIEWER_ARCHIVE_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

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
} CapitalEntry;

typedef struct {
    char *path;
    CapitalEntry entry;
} CapitalFileInfo;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t entry_count;
    uint32_t flags;
    uint8_t reserved[16];
} CapitalHeader;

typedef struct {
    FILE *fp;
    CapitalHeader header;
    CapitalFileInfo *files;
    uint8_t *file_data;
    size_t data_size;
} CapitalArchive;

int capital_viewer_open(CapitalArchive *archive, const char *path);
void capital_viewer_close(CapitalArchive *archive);
int capital_viewer_get_file(CapitalArchive *archive, int index, uint8_t **data, size_t *size);
const char *capital_viewer_get_type_name(uint8_t type);
const char *capital_viewer_get_extension(uint8_t type);

#endif
