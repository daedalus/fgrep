#include "io.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define IO_BUF_SIZE (1 << 16)

fgrep_status_t fgrep_io_open(fgrep_io_t *io, const char *path) {
    __builtin_memset(io, 0, sizeof(*io));
    io->fd = -1;

    int fd = open(path, O_RDONLY);
    if (fd < 0) return FGREP_ERR_IO;

    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return FGREP_ERR_IO; }

    size_t size = (size_t)st.st_size;
    if (size == 0) { close(fd); return FGREP_ERR_IO; }

    void *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) { close(fd); return FGREP_ERR_IO; }
    madvise(map, size, MADV_SEQUENTIAL);

    io->data = map;
    io->size = size;
    io->mode = FGREP_IO_MMAP;
    io->fd = fd;
    io->u.mmap.map_size = size;
    return FGREP_OK;
}

fgrep_status_t fgrep_io_open_stdin(fgrep_io_t *io) {
    __builtin_memset(io, 0, sizeof(*io));
    io->fd = -1;
    size_t cap = IO_BUF_SIZE;
    size_t total = 0;
    char *buf = NULL;
    for (;;) {
        char *newbuf = realloc(buf, cap);
        if (!newbuf) { free(buf); return FGREP_ERR_NOMEM; }
        buf = newbuf;
        ssize_t n = read(STDIN_FILENO, buf + total, cap - total);
        if (n <= 0) break;
        total += (size_t)n;
        if (total == cap) { cap *= 2; if (cap > (1 << 20)) break; }
    }
    io->data = buf;
    io->size = total;
    io->mode = FGREP_IO_READ;
    io->fd = -1;
    io->u.read.buf = buf;
    io->u.read.cap = cap;
    return FGREP_OK;
}

void fgrep_io_close(fgrep_io_t *io) {
    if (io->mode == FGREP_IO_MMAP) {
        if (io->data) munmap((void *)io->data, io->u.mmap.map_size);
        if (io->fd >= 0) close(io->fd);
    } else {
        free(io->u.read.buf);
        if (io->fd >= 0) close(io->fd);
    }
    __builtin_memset(io, 0, sizeof(*io));
    io->fd = -1;
}
