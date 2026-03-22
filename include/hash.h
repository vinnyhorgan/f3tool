#ifndef F3_HASH_H
#define F3_HASH_H

#include <stdint.h>

uint64_t bsa_hash_filename(const char *filename);
uint64_t bsa_hash_filename_with_extension(const char *filename);

#endif /* F3_HASH_H */
