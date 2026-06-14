#ifndef REGEX_ENGINE_H
#define REGEX_ENGINE_H

#include "fgrep.h"
#include <regex.h>
#include <stddef.h>

typedef struct {
    regex_t compiled;
    bool is_regex;
    char *fixed_str;
    size_t fixed_len;
} fgrep_pattern_t;

fgrep_status_t fgrep_pattern_compile(fgrep_pattern_t *pat, const char *pattern, bool fixed_string, bool ignore_case);
void fgrep_pattern_destroy(fgrep_pattern_t *pat);
fgrep_status_t fgrep_pattern_match(const fgrep_pattern_t *pat, const char *data, size_t len, size_t *match_start, size_t *match_len);

#endif
