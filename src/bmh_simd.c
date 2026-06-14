/*
 * bmh_simd.c - Boyer-Moore-Horspool search with SIMD acceleration
 *
 * Key insight from grep: BMH skip table advances past non-matching regions.
 * SIMD scans 32 bytes at a time to find candidate positions.
 * Combined approach: SIMD finds candidates, BMH skip advances efficiently.
 */

#include <immintrin.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

size_t bmh_search(const char *text, size_t text_len,
                   const char *pat, size_t pat_len) {
    if (pat_len == 0 || text_len < pat_len) return 0;

    /* Build skip table */
    size_t skip[256];
    for (int j = 0; j < 256; j++) skip[j] = pat_len;
    for (size_t j = 0; j < pat_len - 1; j++)
        skip[(unsigned char)pat[j]] = pat_len - 1 - j;

    /* For single-byte patterns, use SIMD */
    if (pat_len == 1) {
        __m256i ndl = _mm256_set1_epi8(pat[0]);
        size_t count = 0, i = 0;
        while (i + 32 <= text_len) {
            __m256i chunk = _mm256_loadu_si256((const __m256i *)(text + i));
            int mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, ndl));
            count += __builtin_popcount((unsigned)mask);
            i += 32;
        }
        for (; i < text_len; i++)
            if (text[i] == pat[0]) count++;
        return count;
    }

    /* For multi-byte: SIMD first+last byte filter + BMH skip */
    unsigned char first = (unsigned char)pat[0];
    unsigned char last = (unsigned char)pat[pat_len - 1];
    __m256i ndl_first = _mm256_set1_epi8((char)first);
    __m256i ndl_last = _mm256_set1_epi8((char)last);
    size_t count = 0, i = 0;

    while (i + pat_len <= text_len) {
        /* SIMD: find positions where first byte matches in 32-byte window */
        __m256i vec = _mm256_loadu_si256((const __m256i *)(text + i));
        int mf = _mm256_movemask_epi8(_mm256_cmpeq_epi8(vec, ndl_first));

        /* For each candidate, check last byte with SIMD */
        __m256i vec_l = _mm256_loadu_si256((const __m256i *)(text + i + pat_len - 1));
        int ml = _mm256_movemask_epi8(_mm256_cmpeq_epi8(vec_l, ndl_last));
        int combined = mf & ml;

        while (combined) {
            int bit = __builtin_ctz(combined);
            size_t pos = i + (size_t)bit;
            if (pos + pat_len <= text_len && memcmp(text + pos, pat, pat_len) == 0)
                count++;
            combined &= combined - 1;
        }

        /* Advance: max of SIMD window and BMH skip */
        size_t bmh_skip = skip[(unsigned char)text[i + pat_len - 1]];
        i += (32 > bmh_skip) ? 32 : bmh_skip;
    }
    return count;
}
