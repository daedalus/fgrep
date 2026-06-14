#ifndef FGREP_H
#define FGREP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FGREP_VERSION "0.1.0"
#define FGREP_MAX_LINE_LEN (1 << 20)
#define FGREP_IO_BUF_SIZE  (1 << 16)
#define FGREP_MMAP_THRESHOLD (1 << 20)

typedef struct {
    const char *pattern;
    bool fixed_string;
    bool whole_line;
    bool count_only;
    bool files_with_matches;
    bool files_without_match;
    bool ignore_case;
    bool invert_match;
    bool line_number;
    bool recursive;
    bool color;
    bool multiline;
    int max_count;
} fgrep_options_t;

typedef struct {
    const char *path;
    const char *data;
    size_t line_start;
    size_t line_end;
    size_t line_no;
    size_t match_offset;
    size_t match_len;
} fgrep_match_t;

typedef struct {
    size_t total_matches;
    size_t total_files;
    size_t files_matched;
} fgrep_stats_t;

typedef enum {
    FGREP_OK = 0,
    FGREP_ERR_NOMEM,
    FGREP_ERR_IO,
    FGREP_ERR_PATTERN,
    FGREP_ERR_NO_MATCH,
} fgrep_status_t;

typedef enum {
    FGREP_IO_READ = 0,
    FGREP_IO_MMAP,
} fgrep_io_mode_t;

#endif
