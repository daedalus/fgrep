/*
 * bmh_simd.c - SIMD-accelerated fixed-string search
 *
 * Uses AVX2 SIMD to scan 32 bytes at a time, filtering with first+last
 * byte check, then verifying matches with memcmp.
 */

#include <immintrin.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

size_t bmh_search(const char *text, size_t text_len,
                   const char *pat, size_t pat_len) {
    if (pat_len == 0 || text_len < pat_len) return 0;

    /* Single-byte: SIMD popcount */
    if (pat_len == 1) {
        __m256i ndl = _mm256_set1_epi8(pat[0]);
        size_t count = 0, i = 0;
        while (i + 32 <= text_len) {
            __m256i chunk = _mm256_loadu_si256((const __m256i *)(text + i));
            count += __builtin_popcount((unsigned)_mm256_movemask_epi8(
                _mm256_cmpeq_epi8(chunk, ndl)));
            i += 32;
        }
        for (; i < text_len; i++)
            if (text[i] == pat[0]) count++;
        return count;
    }

    /* Multi-byte: SIMD first+last byte filter + memcmp */
    unsigned char first = (unsigned char)pat[0];
    unsigned char last = (unsigned char)pat[pat_len - 1];
    __m256i ndl_f = _mm256_set1_epi8((char)first);
    __m256i ndl_l = _mm256_set1_epi8((char)last);
    size_t count = 0, i = 0;

    while (i + pat_len <= text_len) {
        __m256i vf = _mm256_loadu_si256((const __m256i *)(text + i));
        int mf = _mm256_movemask_epi8(_mm256_cmpeq_epi8(vf, ndl_f));
        __m256i vl = _mm256_loadu_si256((const __m256i *)(text + i + pat_len - 1));
        int ml = _mm256_movemask_epi8(_mm256_cmpeq_epi8(vl, ndl_l));
        int combined = mf & ml;
        while (combined) {
            int bit = __builtin_ctz(combined);
            size_t pos = i + (size_t)bit;
            if (pos + pat_len <= text_len &&
                memcmp(text + pos + 1, pat + 1, pat_len - 2) == 0)
                count++;
            combined &= combined - 1;
        }
        i += 32;
    }
    return count;
}
