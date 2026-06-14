#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "fgrep.h"
#include <pthread.h>

typedef void (*fgrep_work_fn)(void *arg);

typedef struct {
    pthread_t *threads;
    int num_threads;
    int active;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool shutdown;
} fgrep_threadpool_t;

fgrep_status_t fgrep_threadpool_create(fgrep_threadpool_t **pool, int num_threads);
void fgrep_threadpool_destroy(fgrep_threadpool_t *pool);
fgrep_status_t fgrep_threadpool_submit(fgrep_threadpool_t *pool, fgrep_work_fn fn, void *arg);
void fgrep_threadpool_wait(fgrep_threadpool_t *pool);

#endif
