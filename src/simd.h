#ifndef SIMD_H
#define SIMD_H

#include <stddef.h>
#include <stdint.h>

typedef void *(*simd_memchr_fn)(const void *s, int c, size_t n);

simd_memchr_fn simd_get_memchr(void);
size_t simd_find_newline(const char *data, size_t len);

#endif
