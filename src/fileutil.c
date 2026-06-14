#include "fileutil.h"
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <limits.h>

struct fgrep_file_entry {
    char *path;
    struct fgrep_file_entry *next;
};

struct fgrep_filelist {
    struct fgrep_file_entry *head;
    struct fgrep_file_entry *tail;
    struct fgrep_file_entry *current;
    size_t count;
};

static fgrep_status_t add_file(fgrep_filelist_t *list, const char *path) {
    struct fgrep_file_entry *e = malloc(sizeof(*e));
    if (!e) return FGREP_ERR_NOMEM;
    e->path = strdup(path);
    if (!e->path) { free(e); return FGREP_ERR_NOMEM; }
    e->next = NULL;
    if (list->tail) list->tail->next = e;
    else list->head = e;
    list->tail = e;
    list->count++;
    return FGREP_OK;
}

static fgrep_status_t walk_dir(fgrep_filelist_t *list, const char *dirpath, bool recursive) {
    DIR *d = opendir(dirpath);
    if (!d) return FGREP_ERR_IO;

    struct dirent *ent;
    fgrep_status_t st = FGREP_OK;

    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dirpath, ent->d_name);

        struct stat sb;
        if (stat(path, &sb) < 0) continue;

        if (S_ISDIR(sb.st_mode) && recursive) {
            st = walk_dir(list, path, true);
            if (st != FGREP_OK) break;
        } else if (S_ISREG(sb.st_mode)) {
            st = add_file(list, path);
            if (st != FGREP_OK) break;
        }
    }

    closedir(d);
    return st;
}

fgrep_status_t fgrep_filelist_create(fgrep_filelist_t **out, const char *path, bool recursive) {
    fgrep_filelist_t *list = calloc(1, sizeof(*list));
    if (!list) return FGREP_ERR_NOMEM;

    struct stat sb;
    if (stat(path, &sb) < 0) { free(list); return FGREP_ERR_IO; }

    fgrep_status_t st;
    if (S_ISDIR(sb.st_mode)) {
        st = walk_dir(list, path, recursive);
    } else {
        st = add_file(list, path);
    }

    if (st != FGREP_OK) {
        fgrep_filelist_destroy(list);
        return st;
    }

    list->current = list->head;
    *out = list;
    return FGREP_OK;
}

void fgrep_filelist_destroy(fgrep_filelist_t *list) {
    if (!list) return;
    struct fgrep_file_entry *e = list->head;
    while (e) {
        struct fgrep_file_entry *next = e->next;
        free(e->path);
        free(e);
        e = next;
    }
    free(list);
}

size_t fgrep_filelist_count(const fgrep_filelist_t *list) {
    return list->count;
}

const char *fgrep_filelist_next(fgrep_filelist_t *list) {
    if (!list->current) return NULL;
    const char *path = list->current->path;
    list->current = list->current->next;
    return path;
}
