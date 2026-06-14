#ifndef BMH_SIMD_H
#define BMH_SIMD_H

#include <stddef.h>

size_t bmh_search(const char *text, size_t text_len,
                   const char *pat, size_t pat_len);

#endif
