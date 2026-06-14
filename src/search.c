#include "search.h"
#include "simd.h"
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

fgrep_status_t search_data(const char *data, size_t len, const char *path,
                           fgrep_search_ctx_t *ctx, size_t *match_count) {
    (void)path;
    *match_count = 0;
    simd_memchr_fn memchr_fn = simd_get_memchr();
    const fgrep_options_t *opts = ctx->opts;
    size_t pos = 0;

    while (pos < len) {
        size_t remaining = len - pos;

        size_t line_start = pos;
        void *nl = memchr_fn(data + pos, '\n', remaining);
        size_t line_end = nl ? (size_t)((const char *)nl - data) : len;
        size_t line_len = line_end - line_start;

        size_t match_start, match_len;
        fgrep_status_t st = fgrep_pattern_match(ctx->pattern, data + line_start, line_len, &match_start, &match_len);

        bool matched = (st == FGREP_OK);

        if (opts->invert_match) matched = !matched;

        if (matched) {
            (*match_count)++;

            if (opts->max_count > 0 && *match_count > (size_t)opts->max_count) break;

            if (!opts->count_only && !opts->files_without_match) {
                fgrep_match_t m = {
                    .path = path,
                    .data = data + line_start,
                    .line_start = line_start,
                    .line_end = line_end,
                    .line_no = 0,
                    .match_offset = match_start,
                    .match_len = match_len,
                };

                if (opts->line_number) {
                    size_t line_no = 1;
                    for (size_t i = 0; i < line_start; i++) {
                        if (data[i] == '\n') line_no++;
                    }
                    m.line_no = line_no;
                }

                if (ctx->output_mutex) pthread_mutex_lock(ctx->output_mutex);
                fgrep_output_match(ctx->output, &m, opts);
                if (ctx->output_mutex) pthread_mutex_unlock(ctx->output_mutex);
            }
        }

        pos = (nl ? line_end + 1 : len);
    }

    return FGREP_OK;
}

fgrep_status_t fgrep_search_file(const char *path, fgrep_search_ctx_t *ctx) {
    fgrep_io_t io;
    fgrep_status_t st = fgrep_io_open(&io, path);
    if (st != FGREP_OK) return st;

    size_t match_count = 0;
    st = search_data(io.data, io.size, path, ctx, &match_count);

    if (ctx->opts->count_only) {
        if (ctx->output_mutex) pthread_mutex_lock(ctx->output_mutex);
        fgrep_output_count(ctx->output, path, match_count);
        if (ctx->output_mutex) pthread_mutex_unlock(ctx->output_mutex);
    } else if (ctx->opts->files_with_matches && match_count > 0) {
        if (ctx->output_mutex) pthread_mutex_lock(ctx->output_mutex);
        fgrep_output_file_match(ctx->output, path);
        if (ctx->output_mutex) pthread_mutex_unlock(ctx->output_mutex);
    } else if (ctx->opts->files_without_match && match_count == 0) {
        if (ctx->output_mutex) pthread_mutex_lock(ctx->output_mutex);
        fgrep_output_file_no_match(ctx->output, path);
        if (ctx->output_mutex) pthread_mutex_unlock(ctx->output_mutex);
    }

    if (ctx->stats) {
        __atomic_add_fetch(&ctx->stats->total_matches, match_count, __ATOMIC_RELAXED);
        __atomic_add_fetch(&ctx->stats->total_files, 1, __ATOMIC_RELAXED);
        if (match_count > 0) __atomic_add_fetch(&ctx->stats->files_matched, 1, __ATOMIC_RELAXED);
    }

    fgrep_io_close(&io);
    return st;
}

void *fgrep_search_file_worker(void *arg) {
    file_work_t *fw = arg;
    fgrep_search_file(fw->path, fw->ctx);
    free(fw);
    return NULL;
}
