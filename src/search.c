#include "search.h"
#include "simd.h"
#include <emmintrin.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#define OUTPUT_BUF_SIZE (1 << 16)

static inline void *fgrep_memchr(const void *s, int c, size_t n) {
    const unsigned char *p = s;
    __m128i needle = _mm_set1_epi8((char)c);
    size_t i = 0;
    for (; i + 16 <= n; i += 16) {
        __m128i chunk = _mm_loadu_si128((const __m128i *)(p + i));
        int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, needle));
        if (mask) return (void *)(p + i + __builtin_ctz(mask));
    }
    for (; i < n; i++) if (p[i] == (unsigned char)c) return (void *)(p + i);
    return NULL;
}

static inline size_t find_line_start(const char *d, size_t p) {
    while (p > 0 && d[p - 1] != '\n') p--;
    return p;
}

static inline size_t find_line_end(const char *d, size_t n, size_t p) {
    while (p < n && d[p] != '\n') p++;
    return p;
}

static inline size_t count_lines_to(const char *d, size_t p) {
    size_t n = 1;
    for (size_t i = 0; i < p; i++) if (d[i] == '\n') n++;
    return n;
}

__attribute__((noinline, cold))
static fgrep_status_t search_regex_impl(const char *data, size_t len, const char *path,
                                         fgrep_search_ctx_t *ctx, size_t *match_count_out) {
    const fgrep_options_t *opts = ctx->opts;
    const fgrep_pattern_t *pat = ctx->pattern;
    FILE *out = ctx->output;
    pthread_mutex_t *mtx = ctx->output_mutex;
    size_t pos = 0, count = 0, line_no = 1;

    while (pos < len) {
        size_t nl = simd_find_newline(data + pos, len - pos);
        size_t line_len = (nl < len - pos) ? nl : len - pos;
        size_t line_end = pos + line_len;

        size_t ms = 0, ml = 0;
        bool matched = (fgrep_pattern_match(pat, data + pos, line_len, &ms, &ml) == FGREP_OK);
        if (opts->invert_match) matched = !matched;

        if (matched) {
            count++;
            if (opts->max_count > 0 && count > (size_t)opts->max_count) break;
            if (!opts->count_only && !opts->files_without_match) {
                fgrep_match_t m = {
                    .path = path, .data = data + pos,
                    .line_start = pos, .line_end = line_end,
                    .line_no = opts->line_number ? line_no : 0,
                    .match_offset = ms, .match_len = ml,
                };
                if (mtx) pthread_mutex_lock(mtx);
                fgrep_output_match(out, &m, opts);
                if (mtx) pthread_mutex_unlock(mtx);
            }
        }
        if (nl < len - pos) { pos = line_end + 1; line_no++; }
        else break;
    }

    if (opts->count_only) {
        if (mtx) pthread_mutex_lock(mtx);
        fgrep_output_count(out, path, count);
        if (mtx) pthread_mutex_unlock(mtx);
    } else if (opts->files_with_matches && count > 0) {
        if (mtx) pthread_mutex_lock(mtx);
        fgrep_output_file_match(out, path);
        if (mtx) pthread_mutex_unlock(mtx);
    } else if (opts->files_without_match && count == 0) {
        if (mtx) pthread_mutex_lock(mtx);
        fgrep_output_file_no_match(out, path);
        if (mtx) pthread_mutex_unlock(mtx);
    }

    if (ctx->stats) {
        __atomic_add_fetch(&ctx->stats->total_matches, count, __ATOMIC_RELAXED);
        __atomic_add_fetch(&ctx->stats->total_files, 1, __ATOMIC_RELAXED);
        if (count > 0) __atomic_add_fetch(&ctx->stats->files_matched, 1, __ATOMIC_RELAXED);
    }
    if (match_count_out) *match_count_out = count;
    return FGREP_OK;
}

fgrep_status_t search_data(const char *data, size_t len, const char *path,
                           fgrep_search_ctx_t *ctx, size_t *match_count_out) {
    const fgrep_options_t *opts = ctx->opts;
    const fgrep_pattern_t *pat = ctx->pattern;

    if (pat->is_regex || opts->ignore_case || opts->invert_match) {
        return search_regex_impl(data, len, path, ctx, match_count_out);
    }

    const char *needle = pat->fixed_str;
    size_t nlen = pat->fixed_len;
    if (nlen == 0) { if (match_count_out) *match_count_out = 0; return FGREP_OK; }

    FILE *out = ctx->output;
    pthread_mutex_t *mtx = ctx->output_mutex;
    size_t pos = 0, count = 0;

    if (opts->count_only || opts->files_with_matches || opts->files_without_match) {
        if (nlen == 1) {
            unsigned char c = (unsigned char)needle[0];
            __m128i ndl = _mm_set1_epi8((char)c);
            while (pos + 1 <= len) {
                size_t rem = len - pos;
                const unsigned char *p = data + pos;
                size_t i = 0;
                void *f = NULL;
                for (; i + 16 <= rem; i += 16) {
                    __m128i chunk = _mm_loadu_si128((const __m128i *)(p + i));
                    int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, ndl));
                    if (mask) { f = (void *)(p + i + __builtin_ctz(mask)); break; }
                }
                if (!f) for (; i < rem; i++) if (p[i] == c) { f = (void *)(p + i); break; }
                if (!f) break;
                pos = (size_t)((const char *)f - data);
                count++;
                pos++;
            }
        } else {
            unsigned char first = (unsigned char)needle[0];
            unsigned char last = (unsigned char)needle[nlen - 1];
            __m128i ndl_f = _mm_set1_epi8((char)first);
            __m128i ndl_l = _mm_set1_epi8((char)last);
            while (pos + nlen <= len) {
                size_t rem = len - pos - nlen + 1;
                const unsigned char *p = data + pos;
                size_t i = 0;
                void *f = NULL;
                for (; i + 16 <= rem; i += 16) {
                    __m128i cf = _mm_loadu_si128((const __m128i *)(p + i));
                    int mf = _mm_movemask_epi8(_mm_cmpeq_epi8(cf, ndl_f));
                    __m128i cl = _mm_loadu_si128((const __m128i *)(p + i + nlen - 1));
                    int ml = _mm_movemask_epi8(_mm_cmpeq_epi8(cl, ndl_l));
                    int c = mf & ml;
                    if (c) { f = (void *)(p + i + __builtin_ctz(c)); break; }
                }
                if (!f) for (; i < rem; i++) if (p[i] == first && p[i + nlen - 1] == last) { f = (void *)(p + i); break; }
                if (!f) break;
                pos = (size_t)((const char *)f - data);
                if (memcmp(data + pos + 1, needle + 1, nlen - 2) == 0) count++;
                pos++;
            }
        }
    } else {
        const bool use_color = opts->color && isatty(fileno(out));
        const bool show_lineno = opts->line_number;
        const size_t plen = path ? strlen(path) : 0;
        const size_t max_count = (size_t)opts->max_count;
        size_t line_no = 1;
        char outbuf[OUTPUT_BUF_SIZE];
        size_t outpos = 0;

        if (nlen == 1) {
            unsigned char c = (unsigned char)needle[0];
            __m128i ndl = _mm_set1_epi8((char)c);
            while (pos + 1 <= len) {
                size_t rem = len - pos;
                const unsigned char *p = data + pos;
                size_t i = 0;
                void *f = NULL;
                for (; i + 16 <= rem; i += 16) {
                    __m128i chunk = _mm_loadu_si128((const __m128i *)(p + i));
                    int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, ndl));
                    if (mask) { f = (void *)(p + i + __builtin_ctz(mask)); break; }
                }
                if (!f) for (; i < rem; i++) if (p[i] == c) { f = (void *)(p + i); break; }
                if (!f) break;
                pos = (size_t)((const char *)f - data);

                count++;
                if (max_count > 0 && count > max_count) break;
                size_t ls = find_line_start(data, pos);
                size_t le = find_line_end(data, len, pos + nlen);
                if (show_lineno) line_no = count_lines_to(data, ls);

                size_t needed = plen + (show_lineno ? 24 : 0) + (le - ls) + 2;
                if (outpos + needed > OUTPUT_BUF_SIZE) { fwrite(outbuf, 1, outpos, out); outpos = 0; }

                if (path) {
                    if (use_color) {
                        memcpy(outbuf + outpos, "\033[1;35m", 7); outpos += 7;
                        memcpy(outbuf + outpos, path, plen); outpos += plen;
                        memcpy(outbuf + outpos, "\033[0;36m:", 8); outpos += 8;
                    } else {
                        memcpy(outbuf + outpos, path, plen); outpos += plen;
                        outbuf[outpos++] = ':';
                    }
                }
                if (show_lineno) {
                    char lbuf[24];
                    int n = snprintf(lbuf, sizeof(lbuf), "%zu:", line_no);
                    if (use_color) {
                        memcpy(outbuf + outpos, "\033[1;32m", 7); outpos += 7;
                        memcpy(outbuf + outpos, lbuf, (size_t)n); outpos += (size_t)n;
                        memcpy(outbuf + outpos, "\033[0;36m", 7); outpos += 7;
                    } else {
                        memcpy(outbuf + outpos, lbuf, (size_t)n); outpos += (size_t)n;
                    }
                }

                size_t match_off = (size_t)(pos - ls);
                if (use_color) {
                    if (match_off > 0) { memcpy(outbuf + outpos, data + ls, match_off); outpos += match_off; }
                    memcpy(outbuf + outpos, "\033[1;31m", 7); outpos += 7;
                    outbuf[outpos++] = data[pos];
                    memcpy(outbuf + outpos, "\033[0m", 4); outpos += 4;
                    size_t after = (size_t)(le - pos - 1);
                    if (after > 0) { memcpy(outbuf + outpos, data + pos + 1, after); outpos += after; }
                } else {
                    memcpy(outbuf + outpos, data + ls, (size_t)(le - ls)); outpos += (size_t)(le - ls);
                }
                outbuf[outpos++] = '\n';
                pos++;
            }
        } else {
            unsigned char first = (unsigned char)needle[0];
            unsigned char last = (unsigned char)needle[nlen - 1];
            __m128i ndl_f = _mm_set1_epi8((char)first);
            __m128i ndl_l = _mm_set1_epi8((char)last);
            while (pos + nlen <= len) {
                size_t rem = len - pos - nlen + 1;
                const unsigned char *p = data + pos;
                size_t i = 0;
                void *f = NULL;
                for (; i + 16 <= rem; i += 16) {
                    __m128i cf = _mm_loadu_si128((const __m128i *)(p + i));
                    int mf = _mm_movemask_epi8(_mm_cmpeq_epi8(cf, ndl_f));
                    __m128i cl = _mm_loadu_si128((const __m128i *)(p + i + nlen - 1));
                    int ml = _mm_movemask_epi8(_mm_cmpeq_epi8(cl, ndl_l));
                    int c = mf & ml;
                    if (c) { f = (void *)(p + i + __builtin_ctz(c)); break; }
                }
                if (!f) for (; i < rem; i++) if (p[i] == first && p[i + nlen - 1] == last) { f = (void *)(p + i); break; }
                if (!f) break;
                pos = (size_t)((const char *)f - data);
                if (memcmp(data + pos + 1, needle + 1, nlen - 2) != 0) { pos++; continue; }

                count++;
                if (max_count > 0 && count > max_count) break;
                size_t ls = find_line_start(data, pos);
                size_t le = find_line_end(data, len, pos + nlen);
                if (show_lineno) line_no = count_lines_to(data, ls);

                size_t needed = plen + (show_lineno ? 24 : 0) + (le - ls) + 2;
                if (outpos + needed > OUTPUT_BUF_SIZE) { fwrite(outbuf, 1, outpos, out); outpos = 0; }

                if (path) {
                    if (use_color) {
                        memcpy(outbuf + outpos, "\033[1;35m", 7); outpos += 7;
                        memcpy(outbuf + outpos, path, plen); outpos += plen;
                        memcpy(outbuf + outpos, "\033[0;36m:", 8); outpos += 8;
                    } else {
                        memcpy(outbuf + outpos, path, plen); outpos += plen;
                        outbuf[outpos++] = ':';
                    }
                }
                if (show_lineno) {
                    char lbuf[24];
                    int n = snprintf(lbuf, sizeof(lbuf), "%zu:", line_no);
                    if (use_color) {
                        memcpy(outbuf + outpos, "\033[1;32m", 7); outpos += 7;
                        memcpy(outbuf + outpos, lbuf, (size_t)n); outpos += (size_t)n;
                        memcpy(outbuf + outpos, "\033[0;36m", 7); outpos += 7;
                    } else {
                        memcpy(outbuf + outpos, lbuf, (size_t)n); outpos += (size_t)n;
                    }
                }

                size_t match_off = (size_t)(pos - ls);
                if (use_color) {
                    if (match_off > 0) { memcpy(outbuf + outpos, data + ls, match_off); outpos += match_off; }
                    memcpy(outbuf + outpos, "\033[1;31m", 7); outpos += 7;
                    memcpy(outbuf + outpos, data + pos, nlen); outpos += nlen;
                    memcpy(outbuf + outpos, "\033[0m", 4); outpos += 4;
                    size_t after = (size_t)(le - pos - nlen);
                    if (after > 0) { memcpy(outbuf + outpos, data + pos + nlen, after); outpos += after; }
                } else {
                    memcpy(outbuf + outpos, data + ls, (size_t)(le - ls)); outpos += (size_t)(le - ls);
                }
                outbuf[outpos++] = '\n';
                pos += nlen;
            }
        }

        if (outpos > 0) fwrite(outbuf, 1, outpos, out);
    }

    if (opts->count_only) {
        if (mtx) pthread_mutex_lock(mtx);
        fgrep_output_count(out, path, count);
        if (mtx) pthread_mutex_unlock(mtx);
    } else if (opts->files_with_matches && count > 0) {
        if (mtx) pthread_mutex_lock(mtx);
        fgrep_output_file_match(out, path);
        if (mtx) pthread_mutex_unlock(mtx);
    } else if (opts->files_without_match && count == 0) {
        if (mtx) pthread_mutex_lock(mtx);
        fgrep_output_file_no_match(out, path);
        if (mtx) pthread_mutex_unlock(mtx);
    }

    if (ctx->stats) {
        __atomic_add_fetch(&ctx->stats->total_matches, count, __ATOMIC_RELAXED);
        __atomic_add_fetch(&ctx->stats->total_files, 1, __ATOMIC_RELAXED);
        if (count > 0) __atomic_add_fetch(&ctx->stats->files_matched, 1, __ATOMIC_RELAXED);
    }
    if (match_count_out) *match_count_out = count;
    return FGREP_OK;
}

fgrep_status_t fgrep_search_file(const char *path, fgrep_search_ctx_t *ctx) {
    fgrep_io_t io;
    fgrep_status_t st = fgrep_io_open(&io, path);
    if (st != FGREP_OK) return st;
    st = search_data(io.data, io.size, path, ctx, NULL);
    fgrep_io_close(&io);
    return st;
}

void *fgrep_search_file_worker(void *arg) {
    file_work_t *fw = arg;
    fgrep_search_file(fw->path, fw->ctx);
    free(fw);
    return NULL;
}
