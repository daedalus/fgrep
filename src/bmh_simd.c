/*
 * bmh_simd.c - Fixed-string search engine
 *
 * Uses glibc's SIMD-accelerated memchr to find candidate positions,
 * then verifies with memcmp. This matches grep's approach of using
 * memchr for candidate finding.
 */

#include <immintrin.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

size_t bmh_search(const char *text, size_t text_len,
                   const char *pat, size_t pat_len) {
    if (pat_len == 0 || text_len < pat_len) return 0;

    /* Single-byte: use memchr in a loop */
    if (pat_len == 1) {
        unsigned char c = (unsigned char)pat[0];
        size_t count = 0;
        const char *p = text;
        const char *end = text + text_len;
        while ((p = memchr(p, c, (size_t)(end - p))) != NULL) {
            count++;
            p++;
        }
        return count;
    }

    /* Multi-byte: memchr for last byte, verify backward */
    unsigned char last = (unsigned char)pat[pat_len - 1];
    size_t count = 0;
    const char *p = text + pat_len - 1;
    const char *end = text + text_len;

    while (p < end) {
        const char *found = memchr(p, (unsigned char)last, (size_t)(end - p));
        if (!found) break;
        const char *start = found - pat_len + 1;
        if (start >= text && start[0] == pat[0] &&
            memcmp(start + 1, pat + 1, pat_len - 2) == 0)
            count++;
        p = found + 1;
    }
    return count;
}
