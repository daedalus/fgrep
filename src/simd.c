#include "simd.h"
#include <string.h>
#include <emmintrin.h>

#ifdef __AVX2__
#include <immintrin.h>
#endif

#ifdef __AVX512F__
#include <immintrin.h>
#endif

static void __attribute__((unused)) *scalar_memchr(const void *s, int c, size_t n) {
    const unsigned char *p = s;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == (unsigned char)c) return (void *)(p + i);
    }
    return NULL;
}

#ifdef __SSE2__
static void *sse2_memchr(const void *s, int c, size_t n) {
    const unsigned char *p = s;
    __m128i needle = _mm_set1_epi8((char)c);
    size_t i = 0;

    for (; i + 16 <= n; i += 16) {
        __m128i chunk = _mm_loadu_si128((const __m128i *)(p + i));
        __m128i cmp = _mm_cmpeq_epi8(chunk, needle);
        int mask = _mm_movemask_epi8(cmp);
        if (mask != 0) {
            return (void *)(p + i + __builtin_ctz(mask));
        }
    }

    for (; i < n; i++) {
        if (p[i] == (unsigned char)c) return (void *)(p + i);
    }
    return NULL;
}
#endif

#ifdef __AVX2__
static void *avx2_memchr(const void *s, int c, size_t n) {
    const unsigned char *p = s;
    __m256i needle = _mm256_set1_epi8((char)c);
    size_t i = 0;

    for (; i + 32 <= n; i += 32) {
        __m256i chunk = _mm256_loadu_si256((const __m256i *)(p + i));
        __m256i cmp = _mm256_cmpeq_epi8(chunk, needle);
        uint32_t mask = _mm256_movemask_epi8(cmp);
        if (mask != 0) {
            return (void *)(p + i + __builtin_ctz(mask));
        }
    }

    for (; i + 16 <= n; i += 16) {
        __m128i chunk = _mm_loadu_si128((const __m128i *)(p + i));
        __m128i cmp = _mm_cmpeq_epi8(chunk, _mm256_castsi256_si128(needle));
        int mask = _mm_movemask_epi8(cmp);
        if (mask != 0) {
            return (void *)(p + i + __builtin_ctz(mask));
        }
    }

    for (; i < n; i++) {
        if (p[i] == (unsigned char)c) return (void *)(p + i);
    }
    return NULL;
}
#endif

#ifdef __AVX512F__
static void *avx512_memchr(const void *s, int c, size_t n) {
    const unsigned char *p = s;
    __m512i needle = _mm512_set1_epi8((char)c);
    size_t i = 0;

    for (; i + 64 <= n; i += 64) {
        __m512i chunk = _mm512_loadu_si512((const __m512i *)(p + i));
        __mmask64 mask = _mm512_cmpeq_epu8_mask(chunk, needle);
        if (mask != 0) {
            return (void *)(p + i + __builtin_ctzll(mask));
        }
    }

    for (; i + 32 <= n; i += 32) {
        __m256i chunk = _mm256_loadu_si256((const __m256i *)(p + i));
        __m256i cmp = _mm256_cmpeq_epi8(chunk, _mm512_castsi512_si256(needle));
        uint32_t mask = _mm256_movemask_epi8(cmp);
        if (mask != 0) {
            return (void *)(p + i + __builtin_ctz(mask));
        }
    }

    for (; i < n; i++) {
        if (p[i] == (unsigned char)c) return (void *)(p + i);
    }
    return NULL;
}
#endif

simd_memchr_fn simd_get_memchr(void) {
#ifdef __AVX512F__
    return avx512_memchr;
#elif defined(__AVX2__)
    return avx2_memchr;
#elif defined(__SSE2__)
    return sse2_memchr;
#else
    return scalar_memchr;
#endif
}

size_t simd_find_newline(const char *data, size_t len) {
    simd_memchr_fn fn = simd_get_memchr();
    void *pos = fn(data, '\n', len);
    return pos ? (size_t)((const char *)pos - data) : len;
}
