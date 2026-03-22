#ifndef F3_UTIL_H
#define F3_UTIL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

bool file_exists(const char *path);
char *path_join(const char *dir, const char *filename);
char *path_dirname(const char *path);
char *path_basename(const char *path);

int create_directory_recursive(const char *path);

char *str_to_lower(const char *str);

int string_ends_with(const char *str, const char *suffix);

void hexdump(const uint8_t *data, size_t size);

#endif /* F3_UTIL_H */
