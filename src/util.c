#include "../include/util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

bool file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

char *path_join(const char *dir, const char *filename)
{
    size_t dir_len = strlen(dir);
    size_t filename_len = strlen(filename);
    char *result = (char *)malloc(dir_len + filename_len + 2);
    
    if (!result) return NULL;
    
    memcpy(result, dir, dir_len);
    
    if (dir_len > 0 && dir[dir_len - 1] != '/' && dir[dir_len - 1] != '\\') {
        result[dir_len++] = '/';
    }
    
    memcpy(result + dir_len, filename, filename_len + 1);
    
    return result;
}

char *path_dirname(const char *path)
{
    const char *last_sep = strrchr(path, '/');
    const char *lastbackslash = strrchr(path, '\\');
    
    const char *last = last_sep;
    if (!last || (lastbackslash && lastbackslash > last)) {
        last = lastbackslash;
    }
    
    if (!last) {
        char *result = strdup(".");
        return result;
    }
    
    size_t len = last - path;
    char *result = (char *)malloc(len + 1);
    if (!result) return NULL;
    
    memcpy(result, path, len);
    result[len] = '\0';
    
    return result;
}

char *path_basename(const char *path)
{
    const char *last_sep = strrchr(path, '/');
    const char *lastbackslash = strrchr(path, '\\');
    
    const char *last = last_sep;
    if (!last || (lastbackslash && lastbackslash > last)) {
        last = lastbackslash;
    }
    
    if (!last) {
        return strdup(path);
    }
    
    return strdup(last + 1);
}

int create_directory_recursive(const char *path)
{
    char *path_copy = strdup(path);
    if (!path_copy) return -1;
    
    for (char *p = path_copy; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char saved = *p;
            *p = '\0';
            
            if (path_copy[0] && !file_exists(path_copy)) {
                if (mkdir(path_copy, 0755) != 0 && !file_exists(path_copy)) {
                    free(path_copy);
                    return -1;
                }
            }
            
            *p = saved;
        }
    }
    
    if (!file_exists(path_copy)) {
        if (mkdir(path_copy, 0755) != 0 && !file_exists(path_copy)) {
            free(path_copy);
            return -1;
        }
    }
    
    free(path_copy);
    return 0;
}

char *str_to_lower(const char *str)
{
    char *result = strdup(str);
    if (!result) return NULL;
    
    for (char *p = result; *p; p++) {
        *p = (char)tolower(*p);
    }
    
    return result;
}

int string_ends_with(const char *str, const char *suffix)
{
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    
    if (suffix_len > str_len) return 0;
    
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

void hexdump(const uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; i += 16) {
        printf("%08zx: ", i);
        
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                printf("%02x ", data[i + j]);
            } else {
                printf("   ");
            }
            
            if (j == 7) printf(" ");
        }
        
        printf(" |");
        for (size_t j = 0; j < 16 && i + j < size; j++) {
            uint8_t c = data[i + j];
            printf("%c", isprint(c) ? c : '.');
        }
        printf("|\n");
    }
}
