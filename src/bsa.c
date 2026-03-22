#include "../include/bsa.h"
#include "../include/hash.h"
#include "../include/util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <zlib.h>

struct BSArchiveInternal {
    FILE *fp;
    FileHeader header;
    FolderRecord *folders;
    uint8_t *fileRecords;
    char *fileNames;
    size_t fileRecordsSize;
};

static uint32_t calculate_baseline_offset(FileHeader *header)
{
    return sizeof(FileHeader) + 
           sizeof(FolderRecord) * header->folderCount +
           header->totalFileNameLength;
}

int bsa_open(BSArchive **out_archive, const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open: %s\n", path);
        return -1;
    }

    FileHeader header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    if (header.fileId != 0x00415342) {
        fprintf(stderr, "Not a BSA file\n");
        fclose(fp);
        return -1;
    }

    if (header.version != 0x68 && header.version != 0x67) {
        fprintf(stderr, "Unsupported BSA version: 0x%X\n", header.version);
        fclose(fp);
        return -1;
    }

    BSArchive *archive = (BSArchive *)calloc(1, sizeof(BSArchive));
    if (!archive) {
        fclose(fp);
        return -1;
    }

    archive->fp = fp;
    archive->header = header;

    size_t folderRecordsOffset = sizeof(FileHeader);
    archive->folders = (FolderRecord *)malloc(sizeof(FolderRecord) * header.folderCount);
    if (!archive->folders) {
        fclose(fp);
        free(archive);
        return -1;
    }

    fseek(fp, folderRecordsOffset, SEEK_SET);
    if (fread(archive->folders, sizeof(FolderRecord), header.folderCount, fp) != (size_t)header.folderCount) {
        free(archive->folders);
        fclose(fp);
        free(archive);
        return -1;
    }

    uint32_t baselineOffset = calculate_baseline_offset(&header);
    size_t fileRecordsAndNamesSize = header.totalFileNameLength + 16 * header.fileCount + header.folderCount;
    archive->fileRecords = (uint8_t *)malloc(fileRecordsAndNamesSize);
    if (!archive->fileRecords) {
        free(archive->folders);
        fclose(fp);
        free(archive);
        return -1;
    }

    if (fread(archive->fileRecords, 1, fileRecordsAndNamesSize, fp) != fileRecordsAndNamesSize) {
        free(archive->fileRecords);
        free(archive->folders);
        fclose(fp);
        free(archive);
        return -1;
    }
    archive->fileRecordsSize = fileRecordsAndNamesSize;

    archive->fileNames = (char *)malloc(header.totalFileNameLength + 1);
    if (!archive->fileNames) {
        free(archive->fileRecords);
        free(archive->folders);
        free(archive);
        fclose(fp);
        return -1;
    }

    memcpy(archive->fileNames, archive->fileRecords + header.totalFileNameLength + header.folderCount, header.totalFileNameLength);
    archive->fileNames[header.totalFileNameLength] = '\0';

    *out_archive = archive;
    return 0;
}

void bsa_close(BSArchive *archive)
{
    if (!archive) return;

    if (archive->fp) {
        fclose(archive->fp);
    }
    if (archive->folders) {
        free(archive->folders);
    }
    if (archive->fileRecords) {
        free(archive->fileRecords);
    }
    if (archive->fileNames) {
        free(archive->fileNames);
    }
    free(archive);
}

int bsa_get_file_count(BSArchive *archive)
{
    return archive->header.fileCount;
}

int bsa_list_files(BSArchive *archive, char ***out_files)
{
    char **files = (char **)malloc(sizeof(char *) * archive->header.fileCount);
    if (!files) return -1;

    uint32_t baselineOffset = calculate_baseline_offset(&archive->header);
    char *fileNamesPtr = archive->fileNames;
    int fileIndex = 0;

    for (uint32_t i = 0; i < archive->header.folderCount; i++) {
        FolderRecord *folder = &archive->folders[i];
        
        if (folder->offset < baselineOffset) {
            continue;
        }

        uint32_t folderDataOffset = folder->offset - baselineOffset;
        
        if (folderDataOffset >= archive->fileRecordsSize) {
            continue;
        }
        
        uint8_t *folderData = archive->fileRecords + folderDataOffset;

        uint32_t nameLen = folderData[0];
        
        char folderName[256] = {0};
        if (nameLen > 0 && nameLen < sizeof(folderName)) {
            memcpy(folderName, folderData + 1, nameLen);
            folderName[nameLen] = '\0';
        }

        uint8_t *fileRecordsStart = folderData + 1 + nameLen;

        for (uint32_t j = 0; j < folder->count; j++) {
            FileRecord *rec = (FileRecord *)(fileRecordsStart + j * sizeof(FileRecord));
            
            char fullPath[512];
            if (folderName[0]) {
                snprintf(fullPath, sizeof(fullPath), "%s\\%s", folderName, fileNamesPtr);
            } else {
                snprintf(fullPath, sizeof(fullPath), "%s", fileNamesPtr);
            }
            
            files[fileIndex] = strdup(fullPath);
            fileNamesPtr += strlen(fileNamesPtr) + 1;
            fileIndex++;
        }
    }

    *out_files = files;
    return fileIndex;
}

int bsa_extract_file(BSArchive *archive, const char *filename, uint8_t **out_data, size_t *out_size)
{
    char filenameLower[512];
    strncpy(filenameLower, filename, sizeof(filenameLower) - 1);
    filenameLower[sizeof(filenameLower) - 1] = '\0';
    for (char *p = filenameLower; *p; p++) {
        *p = (char)tolower(*p);
    }

    uint32_t baselineOffset = calculate_baseline_offset(&archive->header);
    char *fileNamesPtr = archive->fileNames;

    for (uint32_t i = 0; i < archive->header.folderCount; i++) {
        FolderRecord *folder = &archive->folders[i];
        
        if (folder->offset < baselineOffset) {
            continue;
        }

        uint32_t folderDataOffset = folder->offset - baselineOffset;
        if (folderDataOffset >= archive->fileRecordsSize) {
            continue;
        }
        
        uint8_t *folderData = archive->fileRecords + folderDataOffset;

        uint32_t nameLen = folderData[0];
        char folderName[256] = {0};
        if (nameLen > 0 && nameLen < sizeof(folderName)) {
            memcpy(folderName, folderData + 1, nameLen);
            folderName[nameLen] = '\0';
        }

        uint8_t *fileRecordsStart = folderData + 1 + nameLen;

        for (uint32_t j = 0; j < folder->count; j++) {
            FileRecord *rec = (FileRecord *)(fileRecordsStart + j * sizeof(FileRecord));
            
            char fullPath[512];
            if (folderName[0]) {
                snprintf(fullPath, sizeof(fullPath), "%s\\%s", folderName, fileNamesPtr);
            } else {
                snprintf(fullPath, sizeof(fullPath), "%s", fileNamesPtr);
            }
            
            for (char *p = fullPath; *p; p++) {
                *p = (char)tolower(*p);
            }
            
            if (strcmp(fullPath, filenameLower) == 0) {
                int isCompressed = (archive->header.archiveFlags & 0x04) != 0;
                int fileCompressed = (rec->size & 0x40000000) != 0;
                
                uint32_t dataSize = rec->size & 0x3FFFFFFF;
                
                uint8_t *data = (uint8_t *)malloc(dataSize);
                if (!data) return -1;

                fseek(archive->fp, rec->offset, SEEK_SET);
                if (fread(data, 1, dataSize, archive->fp) != dataSize) {
                    free(data);
                    return -1;
                }

                if (isCompressed != fileCompressed) {
                    if (fileCompressed) {
                        uint32_t uncompressedSize = *(uint32_t *)data;
                        uint8_t *uncompressed = (uint8_t *)malloc(uncompressedSize);
                        if (!uncompressed) {
                            free(data);
                            return -1;
                        }

                        size_t destSize = uncompressedSize;
                        int ret = bsa_decompress_data(data + 4, dataSize - 4, uncompressed, &destSize);
                        free(data);

                        if (ret != 0 || destSize != uncompressedSize) {
                            free(uncompressed);
                            return -1;
                        }

                        *out_data = uncompressed;
                        *out_size = uncompressedSize;
                    } else {
                        free(data);
                        *out_data = NULL;
                        *out_size = 0;
                    }
                } else {
                    *out_data = data;
                    *out_size = dataSize;
                }

                return 0;
            }
            
            fileNamesPtr += strlen(fileNamesPtr) + 1;
        }
    }

    return -1;
}

int bsa_extract_file_to_path(BSArchive *archive, const char *filename, const char *output_path)
{
    uint8_t *data;
    size_t size;
    
    if (bsa_extract_file(archive, filename, &data, &size) != 0) {
        return -1;
    }

    FILE *out = fopen(output_path, "wb");
    if (!out) {
        free(data);
        return -1;
    }

    size_t written = fwrite(data, 1, size, out);
    fclose(out);
    free(data);

    return (written == size) ? 0 : -1;
}

int bsa_decompress_data(const uint8_t *compressed, size_t compressed_size, uint8_t *decompressed, size_t *decompressed_size)
{
    z_stream stream;
    int ret;

    memset(&stream, 0, sizeof(stream));
    stream.next_in = (Bytef *)compressed;
    stream.avail_in = compressed_size;
    stream.next_out = decompressed;
    stream.avail_out = *decompressed_size;

    ret = inflateInit(&stream);
    if (ret != Z_OK) {
        return -1;
    }

    ret = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);

    if (ret == Z_STREAM_END) {
        *decompressed_size = stream.total_out;
        return 0;
    }

    return -1;
}
