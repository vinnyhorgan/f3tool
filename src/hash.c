#include "../include/hash.h"
#include <string.h>
#include <ctype.h>

static uint32_t hash_rot(uint32_t x, int k)
{
    return (x << k) | (x >> (32 - k));
}

uint64_t bsa_hash_filename(const char *filename)
{
    const char *path_sep = strrchr(filename, '\\');
    const char *name_start = path_sep ? path_sep + 1 : filename;
    size_t name_len = strlen(name_start);
    
    if (path_sep) {
        size_t path_len = path_sep - filename;
        for (size_t i = 0; i < path_len; i++) {
            uint32_t x = ((uint32_t)tolower(filename[i])) | 
                         ((uint32_t)(tolower(filename[i + 1])) << 8) |
                         ((uint32_t)tolower(filename[i + 2]) << 16) |
                         ((uint32_t)tolower(filename[i + 3]) << 24);
            x *= 0x4B1F36A9;
            x ^= hash_rot(x, 15) * 0x103C3B27;
            x ^= hash_rot(x, 15);
        }
    }
    
    uint32_t hash1 = 0x4B1F36A9;
    uint32_t hash2 = 0x103C3B27;
    
    for (size_t i = 0; i < name_len; i++) {
        uint32_t x = ((uint32_t)tolower(name_start[i]));
        hash1 = (hash1 * 0x4B1F36A9) ^ (x ^ hash_rot(x, 15) * 0x103C3B27);
        hash2 = hash_rot(hash2 * x, 17);
    }
    
    return ((uint64_t)hash1 << 32) | hash2;
}

uint64_t bsa_hash_filename_with_extension(const char *filename)
{
    char filename_lower[512];
    char *ext = strrchr(filename, '.');
    size_t filename_len;
    
    if (ext) {
        filename_len = ext - filename;
        memcpy(filename_lower, filename, filename_len);
        filename_lower[filename_len] = '\0';
        strcpy(filename_lower + filename_len, ext);
    } else {
        strncpy(filename_lower, filename, sizeof(filename_lower) - 1);
        filename_lower[sizeof(filename_lower) - 1] = '\0';
        filename_len = strlen(filename_lower);
    }
    
    for (char *p = filename_lower; *p; p++) {
        *p = (char)tolower(*p);
    }
    
    return bsa_hash_filename(filename_lower);
}
