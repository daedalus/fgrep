#ifndef IO_H
#define IO_H

#include "fgrep.h"
#include <stdio.h>

typedef struct {
    const char *data;
    size_t size;
    fgrep_io_mode_t mode;
    int fd;
    union {
        struct { char *buf; size_t cap; } read;
        struct { size_t map_size; } mmap;
    } u;
} fgrep_io_t;

fgrep_status_t fgrep_io_open(fgrep_io_t *io, const char *path);
fgrep_status_t fgrep_io_open_stdin(fgrep_io_t *io);
void fgrep_io_close(fgrep_io_t *io);

#endif
