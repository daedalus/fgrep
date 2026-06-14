#ifndef SEARCH_H
#define SEARCH_H

#include "fgrep.h"
#include "regex_engine.h"
#include "io.h"
#include "output.h"
#include <stdio.h>
#include <pthread.h>

typedef struct fgrep_search_ctx {
    const fgrep_options_t *opts;
    const fgrep_pattern_t *pattern;
    fgrep_stats_t *stats;
    FILE *output;
    pthread_mutex_t *output_mutex;
} fgrep_search_ctx_t;

typedef struct {
    const char *path;
    fgrep_search_ctx_t *ctx;
} file_work_t;

fgrep_status_t fgrep_search_file(const char *path, fgrep_search_ctx_t *ctx);
fgrep_status_t search_data(const char *data, size_t len, const char *path,
                           fgrep_search_ctx_t *ctx, size_t *match_count);
void *fgrep_search_file_worker(void *arg);

#endif
