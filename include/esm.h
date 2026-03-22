#ifndef F3_ESM_H
#define F3_ESM_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#define ESM_MAGIC         0x34534554  /* "TES4" in little-endian */

#define ESM_FLAG_MASTER       (1 << 0)
#define ESM_FLAG_ESM          (1 << 1)

#define ESM_GROUP_TYPE_TOP        0
#define ESM_GROUP_TYPE_WORLD      1
#define ESM_GROUP_TYPE_INTERN    2
#define ESM_GROUP_TYPE_CELLS     3
#define ESM_GROUP_TYPE_CELL_CHILD 4
#define ESM_GROUP_TYPE_TOP_CHILD  5

#pragma pack(push, 1)

typedef struct {
    char type[4];
    uint32_t data_size;
    uint32_t flags;
    uint32_t form_id;
    uint32_t revision;
    uint16_t version;
    uint16_t unknown;
} ESMRecordHeader;

typedef struct {
    char type[4];
    uint16_t size;
} ESMSubrecordHeader;

typedef struct {
    uint32_t size;
    uint32_t version;
    uint32_t record_count;
    uint32_t next_object_id;
} ESMHeaderData;

#pragma pack(pop)

typedef struct {
    char name[4];
    uint32_t size;
    uint32_t label;
    uint32_t group_type;
    uint32_t timestamp;
    uint32_t unknown;
} ESMGroupHeader;

typedef struct {
    char *record_type;
    uint32_t form_id;
    uint32_t flags;
    uint8_t *data;
    size_t data_size;
} ESMRecord;

typedef struct {
    FILE *fp;
    char *filename;
    uint32_t record_count;
    uint32_t next_object_id;
    float version;
} ESMFile;

int esm_open(ESMFile *esm, const char *path);
void esm_close(ESMFile *esm);

int esm_read_records(ESMFile *esm, void (*callback)(ESMRecord *record, void *user_data), void *user_data);

int esm_parse_record(ESMFile *esm, ESMRecord *out_record);

char *esm_read_string(FILE *fp, uint16_t *out_size);

#endif /* F3_ESM_H */
