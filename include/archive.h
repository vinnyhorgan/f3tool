#ifndef F3_ARCHIVE_H
#define F3_ARCHIVE_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#define CAPITAL_MAGIC   0x43415021  /* "!PAC" reversed = "!PAC"? actually "CAP\0" */
#define CAPITAL_VERSION 1

#define CAPITAL_TYPE_ESM   1
#define CAPITAL_TYPE_AUDIO 2
#define CAPITAL_TYPE_ART  3
#define CAPITAL_TYPE_VIDEO 4

#pragma pack(push, 1)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t entry_count;
    uint32_t flags;
    uint8_t reserved[16];
} CapitalHeader;

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

#pragma pack(pop)

typedef struct {
    FILE *fp;
    CapitalHeader header;
    CapitalEntry *entries;
    char **paths;
    uint8_t *data;
    void *_internal;
} CapitalArchive;

int capital_create(const char *path, CapitalArchive *archive);
int capital_open(CapitalArchive *archive, const char *path, int read_only);
void capital_close(CapitalArchive *archive);

int capital_add_esm(CapitalArchive *archive, const char *esm_path, const uint8_t *data, size_t data_size);
int capital_add_audio(CapitalArchive *archive, const char *original_path, const uint8_t *data, size_t data_size, uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample);
int capital_add_art(CapitalArchive *archive, const char *original_path, const uint8_t *data, size_t data_size, uint32_t width, uint32_t height, uint32_t format);
int capital_add_video(CapitalArchive *archive, const char *original_path, const uint8_t *data, size_t data_size, uint32_t width, uint32_t height);

int capital_finalize(CapitalArchive *archive);

int capital_get_entry_count(CapitalArchive *archive);
int capital_get_entry(CapitalArchive *archive, int index, CapitalEntry *out_entry, char **out_path, uint8_t **out_data);

int capital_extract_all(CapitalArchive *archive, const char *output_dir);

#endif /* F3_ARCHIVE_H */
