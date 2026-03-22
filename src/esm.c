#include "../include/esm.h"
#include "../include/util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int esm_open(ESMFile *esm, const char *path)
{
    memset(esm, 0, sizeof(*esm));

    esm->fp = fopen(path, "rb");
    if (!esm->fp) {
        return -1;
    }

    uint32_t magic;
    if (fread(&magic, 4, 1, esm->fp) != 1) {
        fclose(esm->fp);
        return -1;
    }

    if (magic != ESM_MAGIC) {
        fprintf(stderr, "Not an ESM file: magic 0x%X, expected 0x%X\n",
                magic, ESM_MAGIC);
        fclose(esm->fp);
        return -1;
    }

    ESMRecordHeader header;
    if (fread(&header, sizeof(header), 1, esm->fp) != 1) {
        fclose(esm->fp);
        return -1;
    }

    char type[5] = {0};
    memcpy(type, header.type, 4);
    fprintf(stderr, "ESM type: %s, data_size: %u, flags: 0x%X, form_id: 0x%X\n",
            type, header.data_size, header.flags, header.form_id);

    while (fread(type, 4, 1, esm->fp) == 1) {
        if (strncmp(type, "HEDR", 4) == 0) {
            uint16_t sub_size;
            if (fread(&sub_size, 2, 1, esm->fp) != 1) break;
            
            ESMHeaderData hedr;
            if (fread(&hedr, sizeof(hedr), 1, esm->fp) != 1) break;
            
            esm->version = hedr.version;
            esm->record_count = hedr.record_count;
            esm->next_object_id = hedr.next_object_id;
            
            fprintf(stderr, "HEDR: version=%f, records=%u, next_id=%u\n",
                    hedr.version, hedr.record_count, hedr.next_object_id);
            break;
        } else {
            uint16_t sub_size;
            if (fread(&sub_size, 2, 1, esm->fp) != 1) break;
            if (fseek(esm->fp, sub_size, SEEK_CUR) != 0) break;
        }
    }

    esm->filename = strdup(path);
    return 0;
}

void esm_close(ESMFile *esm)
{
    if (!esm) return;

    if (esm->fp) {
        fclose(esm->fp);
        esm->fp = NULL;
    }

    if (esm->filename) {
        free(esm->filename);
        esm->filename = NULL;
    }
}

int esm_read_records(ESMFile *esm, void (*callback)(ESMRecord *record, void *user_data), void *user_data)
{
    ESMRecord record;
    char type[5] = {0};

    while (fread(type, 4, 1, esm->fp) == 1) {
        if (strncmp(type, "GRUP", 4) == 0) {
            ESMGroupHeader grup;
            if (fread(&grup.size, 4, 1, esm->fp) != 1) break;
            if (fread(&grup.label, 4, 1, esm->fp) != 1) break;
            if (fread(&grup.group_type, 4, 1, esm->fp) != 1) break;
            if (fread(&grup.timestamp, 4, 1, esm->fp) != 1) break;
            if (fread(&grup.unknown, 4, 1, esm->fp) != 1) break;

            fprintf(stderr, "GRUP: label=%.4s, type=%u, size=%u\n",
                    grup.label, grup.group_type, grup.size);

            if (fseek(esm->fp, grup.size - 24, SEEK_CUR) != 0) break;
            continue;
        }

        ESMRecordHeader rec;
        memcpy(rec.type, type, 4);
        
        if (fread(&rec.data_size, 4, 1, esm->fp) != 1) break;
        if (fread(&rec.flags, 4, 1, esm->fp) != 1) break;
        if (fread(&rec.form_id, 4, 1, esm->fp) != 1) break;
        if (fread(&rec.revision, 4, 1, esm->fp) != 1) break;
        if (fread(&rec.version, 2, 1, esm->fp) != 1) break;
        if (fread(&rec.unknown, 2, 1, esm->fp) != 1) break;

        record.record_type = strdup(rec.type);
        record.form_id = rec.form_id;
        record.flags = rec.flags;
        record.data_size = rec.data_size;
        record.data = (uint8_t *)malloc(rec.data_size);

        if (!record.data) {
            free(record.record_type);
            return -1;
        }

        if (fread(record.data, 1, rec.data_size, esm->fp) != rec.data_size) {
            free(record.data);
            free(record.record_type);
            return -1;
        }

        if (callback) {
            callback(&record, user_data);
        }

        free(record.data);
        free(record.record_type);

        if (feof(esm->fp)) break;
    }

    return 0;
}

char *esm_read_string(FILE *fp, uint16_t *out_size)
{
    uint16_t size;
    if (fread(&size, 2, 1, fp) != 1) {
        return NULL;
    }

    if (out_size) {
        *out_size = size;
    }

    char *str = (char *)malloc(size + 1);
    if (!str) return NULL;

    if (fread(str, 1, size, fp) != size) {
        free(str);
        return NULL;
    }

    str[size] = '\0';
    return str;
}
