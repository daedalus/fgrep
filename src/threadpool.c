#include "threadpool.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct work_item {
    fgrep_work_fn fn;
    void *arg;
    struct work_item *next;
} work_item_t;

typedef struct {
    fgrep_threadpool_t *pool;
} thread_arg_t;

static work_item_t *work_head = NULL;
static work_item_t *work_tail = NULL;
static pthread_mutex_t work_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *worker_thread(void *arg) {
    thread_arg_t *ta = arg;
    fgrep_threadpool_t *pool = ta->pool;
    free(ta);

    for (;;) {
        pthread_mutex_lock(&work_mutex);
        while (!work_head && !pool->shutdown) {
            pthread_cond_wait(&pool->cond, &work_mutex);
        }

        if (pool->shutdown && !work_head) {
            pthread_mutex_unlock(&work_mutex);
            break;
        }

        work_item_t *item = work_head;
        if (item) {
            work_head = item->next;
            if (!work_head) work_tail = NULL;
        }
        pthread_mutex_unlock(&work_mutex);

        if (item) {
            item->fn(item->arg);
            free(item);
        }
    }

    return NULL;
}

fgrep_status_t fgrep_threadpool_create(fgrep_threadpool_t **out, int num_threads) {
    fgrep_threadpool_t *pool = calloc(1, sizeof(*pool));
    if (!pool) return FGREP_ERR_NOMEM;

    pool->num_threads = num_threads;
    pool->threads = calloc((size_t)num_threads, sizeof(pthread_t));
    if (!pool->threads) { free(pool); return FGREP_ERR_NOMEM; }

    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond, NULL);

    for (int i = 0; i < num_threads; i++) {
        thread_arg_t *ta = malloc(sizeof(*ta));
        if (!ta) { fgrep_threadpool_destroy(pool); return FGREP_ERR_NOMEM; }
        ta->pool = pool;
        if (pthread_create(&pool->threads[i], NULL, worker_thread, ta) != 0) {
            free(ta);
            fgrep_threadpool_destroy(pool);
            return FGREP_ERR_IO;
        }
        pool->active++;
    }

    *out = pool;
    return FGREP_OK;
}

void fgrep_threadpool_destroy(fgrep_threadpool_t *pool) {
    if (!pool) return;

    pthread_mutex_lock(&work_mutex);
    pool->shutdown = true;
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&work_mutex);

    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    work_item_t *item = work_head;
    while (item) {
        work_item_t *next = item->next;
        free(item);
        item = next;
    }
    work_head = work_tail = NULL;

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    free(pool->threads);
    free(pool);
}

fgrep_status_t fgrep_threadpool_submit(fgrep_threadpool_t *pool, fgrep_work_fn fn, void *arg) {
    work_item_t *item = malloc(sizeof(*item));
    if (!item) return FGREP_ERR_NOMEM;

    item->fn = fn;
    item->arg = arg;
    item->next = NULL;

    pthread_mutex_lock(&work_mutex);
    if (work_tail) work_tail->next = item;
    else work_head = item;
    work_tail = item;
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&work_mutex);

    return FGREP_OK;
}

void fgrep_threadpool_wait(fgrep_threadpool_t *pool) {
    (void)pool;
    for (;;) {
        pthread_mutex_lock(&work_mutex);
        bool done = (work_head == NULL);
        pthread_mutex_unlock(&work_mutex);
        if (done) break;
        usleep(100);
    }
}
