#ifndef F3_BSA_H
#define F3_BSA_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#define BSA_MAGIC           0x00415342
#define BSA_VERSION_FO3     0x68

#define BSA_FLAG_FOLDER_NAMES    (1 << 0)
#define BSA_FLAG_FILE_NAMES      (1 << 1)
#define BSA_FLAG_COMPRESSED     (1 << 2)

#define BSA_FILE_COMPRESSED  (1 << 30)
#define BSA_FILE_SIZE_MASK   0x3FFFFFFF

#pragma pack(push, 1)
typedef struct {
    uint32_t fileId;
    uint32_t version;
    uint32_t offset;
    uint32_t archiveFlags;
    uint32_t folderCount;
    uint32_t fileCount;
    uint32_t totalFolderNameLength;
    uint32_t totalFileNameLength;
    uint32_t fileFlags;
} FileHeader;

typedef struct {
    uint64_t nameHash;
    uint32_t count;
    uint32_t offset;
} FolderRecord;

typedef struct {
    uint64_t nameHash;
    uint32_t size;
    uint32_t offset;
} FileRecord;
#pragma pack(pop)

typedef struct BSArchiveInternal BSArchive;

int bsa_open(BSArchive **out_archive, const char *path);
void bsa_close(BSArchive *archive);

int bsa_get_file_count(BSArchive *archive);
int bsa_list_files(BSArchive *archive, char ***out_files);

int bsa_extract_file(BSArchive *archive, const char *filename, uint8_t **out_data, size_t *out_size);
int bsa_extract_file_to_path(BSArchive *archive, const char *filename, const char *output_path);

int bsa_decompress_data(const uint8_t *compressed, size_t compressed_size, uint8_t *decompressed, size_t *decompressed_size);

#endif
