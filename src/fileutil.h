#ifndef FILEUTIL_H
#define FILEUTIL_H

#include "fgrep.h"

typedef struct fgrep_filelist fgrep_filelist_t;

fgrep_status_t fgrep_filelist_create(fgrep_filelist_t **list, const char *path, bool recursive);
void fgrep_filelist_destroy(fgrep_filelist_t *list);
size_t fgrep_filelist_count(const fgrep_filelist_t *list);
const char *fgrep_filelist_next(fgrep_filelist_t *list);

#endif
