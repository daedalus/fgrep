/*
 * kwset_engine.c - Self-contained Boyer-Moore search engine
 * 
 * Reimplemented from GNU grep's kwset.c (bmexec_trans + bm_delta2_search).
 * Single compilation unit for maximum compiler optimization.
 *
 * Key insights from grep's code:
 * 1. 12x unrolled skip loop with branch hints
 * 2. delta2 shift table computed from trie failure function
 * 3. memchr fallback for slow advance regions
 * 4. No line parsing during search - only after match found
 */

#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>

#define NCHAR 256

typedef struct {
    char *target;
    int mind;
    unsigned char delta[NCHAR];
    int *shift;
    char gc1;
    char gc2;
} kwset_engine_t;

/* Build delta1 (skip) table */
static void build_delta1(kwset_engine_t *ks, const char *pat, int m) {
    for (int i = 0; i < NCHAR; i++) ks->delta[i] = (unsigned char)m;
    for (int i = 0; i < m - 1; i++)
        ks->delta[(unsigned char)pat[i]] = (unsigned char)(m - 1 - i);
}

/* Compute delta2 shift table using KMP failure function */
static void build_delta2(kwset_engine_t *ks, const char *pat, int m) {
    ks->shift = malloc(sizeof(int) * (m - 1));
    
    /* Compute KMP failure function */
    int *fail = malloc(sizeof(int) * (m + 1));
    fail[0] = -1;
    int k = -1;
    for (int i = 1; i <= m; i++) {
        while (k >= 0 && pat[k] != pat[i - 1])
            k = fail[k];
        k++;
        fail[i] = k;
    }
    
    /* Compute good suffix shifts */
    for (int i = 0; i < m - 1; i++) {
        int s = i + 1; /* matched suffix length */
        
        /* Find rightmost occurrence of pat[m-s..m-1] in pat[0..m-2] */
        ks->shift[i] = m;
        for (int j = m - s - 1; j >= 0; j--) {
            int ok = 1;
            for (int q = 0; q < s; q++) {
                if (pat[j + q] != pat[m - s + q]) { ok = 0; break; }
            }
            if (ok) { ks->shift[i] = m - s - j; break; }
        }
        
        /* If not found, use longest prefix that's also suffix */
        if (ks->shift[i] == m) {
            for (int p = fail[m]; p > 0; p = fail[p]) {
                if (p <= s) { ks->shift[i] = m - p; break; }
            }
        }
    }
    
    free(fail);
}

/* Initialize the search engine */
void kwset_engine_init(kwset_engine_t *ks, const char *pat, int m) {
    ks->mind = m;
    ks->target = malloc(m);
    memcpy(ks->target, pat, m);
    build_delta1(ks, pat, m);
    build_delta2(ks, pat, m);
    ks->gc1 = pat[m - 1];
    ks->gc2 = (m >= 2) ? pat[m - 2] : 0;
}

void kwset_engine_free(kwset_engine_t *ks) {
    free(ks->target);
    free(ks->shift);
}

/* bm_delta2_search: exact reimplementation from grep's kwset.c */
static inline bool bm_delta2_search(kwset_engine_t *ks, const char **tpp,
                              const char *ep, const char *sp) {
    const char *tp = *tpp;
    int len = ks->mind;
    int d = len, skip = 0;
    
    while (1) {
        int i = 2;
        if (tp[-2] == ks->gc2) {
            while (++i <= d)
                if (tp[-i] != sp[-i]) break;
            if (i > d) {
                for (i = d + skip + 1; i <= len; ++i)
                    if (tp[-i] != sp[-i]) break;
                if (i > len) {
                    *tpp = tp - len;
                    return true;
                }
            }
        }
        d = ks->shift[i - 2];
        tp += d;
        if (tp > ep) break;
        if (tp[-1] != ks->gc1) {
            tp += ks->delta[(unsigned char)tp[-1]];
            break;
        }
        skip = i - 1;
    }
    *tpp = tp;
    return false;
}

/* bmexec: exact reimplementation from grep's kwset.c bmexec_trans */
ptrdiff_t kwset_engine_search(kwset_engine_t *ks, const char *text, int size) {
    int len = ks->mind;
    if (len == 0) return 0;
    if (len > size) return -1;
    if (len == 1) {
        const char *tp = memchr(text, ks->gc1, (size_t)size);
        return tp ? tp - text : -1;
    }
    
    const unsigned char *d1 = ks->delta;
    const char *sp = ks->target + len;
    const char *tp = text + len;
    const char *ep = text + size;
    char gc1 = ks->gc1;
    char gc2 = ks->gc2;
    
    if ((size_t)len * 12 < (size_t)size) {
        for (ep = text + size - 11 * len; tp <= ep;) {
            const char *tp0 = tp;
            int d;
            /* 12x unrolled skip - grep's exact inner loop */
            d = d1[(unsigned char)tp[-1]]; tp += d;
            d = d1[(unsigned char)tp[-1]]; tp += d;
            if (d != 0) {
                d = d1[(unsigned char)tp[-1]]; tp += d;
                d = d1[(unsigned char)tp[-1]]; tp += d;
                d = d1[(unsigned char)tp[-1]]; tp += d;
                if (d != 0) {
                    d = d1[(unsigned char)tp[-1]]; tp += d;
                    d = d1[(unsigned char)tp[-1]]; tp += d;
                    d = d1[(unsigned char)tp[-1]]; tp += d;
                    if (d != 0) {
                        d = d1[(unsigned char)tp[-1]]; tp += d;
                        d = d1[(unsigned char)tp[-1]]; tp += d;
                        if (128 <= tp - tp0) continue;
                        tp--;
                        tp = memchr(tp, gc1, (size_t)(ep + 11 * len - tp));
                        if (!tp) return -1;
                        tp++;
                        if (tp + 11 * len > ep + 11 * len) break;
                    }
                }
            }
            if (bm_delta2_search(ks, &tp, ep + 11 * len, sp))
                return tp - text;
        }
    }
    
    ep = text + size;
    int d = d1[(unsigned char)tp[-1]];
    while (d <= (int)(ep - tp)) {
        d = d1[(unsigned char)((tp += d)[-1])];
        if (d != 0) continue;
        if (bm_delta2_search(ks, &tp, ep, sp))
            return tp - text;
    }
    
    return -1;
}
