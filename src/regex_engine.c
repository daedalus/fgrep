#include "regex_engine.h"
#include "simd.h"
#include <stdlib.h>
#include <string.h>

#define STACK_BUF_SIZE 1024

fgrep_status_t fgrep_pattern_compile(fgrep_pattern_t *pat, const char *pattern, bool fixed_string, bool ignore_case) {
    __builtin_memset(pat, 0, sizeof(*pat));

    if (fixed_string) {
        pat->is_regex = false;
        pat->fixed_len = strlen(pattern);
        if (pat->fixed_len > 0) {
            pat->fixed_str = strdup(pattern);
            if (!pat->fixed_str) return FGREP_ERR_NOMEM;
        }
        return FGREP_OK;
    }

    int flags = REG_EXTENDED | REG_NEWLINE;
    if (ignore_case) flags |= REG_ICASE;

    int rc = regcomp(&pat->compiled, pattern, flags);
    if (rc != 0) return FGREP_ERR_PATTERN;

    pat->is_regex = true;
    return FGREP_OK;
}

void fgrep_pattern_destroy(fgrep_pattern_t *pat) {
    if (pat->is_regex) regfree(&pat->compiled);
    else free(pat->fixed_str);
}

static bool fixed_string_insensitive_match(const char *data, size_t len, const fgrep_pattern_t *pat, size_t *pos) {
    if (pat->fixed_len == 0) return false;
    if (pat->fixed_len > len) return false;

    simd_memchr_fn memchr_fn = simd_get_memchr();
    size_t needle_char = (unsigned char)pat->fixed_str[0];

    for (size_t i = 0; i + pat->fixed_len <= len; i++) {
        void *found = memchr_fn(data + i, (int)needle_char, len - i);
        if (!found) return false;
        i = (size_t)((const char *)found - data);

        bool match = true;
        for (size_t j = 1; j < pat->fixed_len; j++) {
            if ((unsigned char)data[i + j] != (unsigned char)pat->fixed_str[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            *pos = i;
            return true;
        }
    }
    return false;
}

fgrep_status_t fgrep_pattern_match(const fgrep_pattern_t *pat, const char *data, size_t len, size_t *match_start, size_t *match_len) {
    if (!pat->is_regex) {
        if (pat->fixed_len == 0) return FGREP_ERR_NO_MATCH;
        if (pat->fixed_len > len) return FGREP_ERR_NO_MATCH;

        size_t pos;
        if (fixed_string_insensitive_match(data, len, pat, &pos)) {
            *match_start = pos;
            *match_len = pat->fixed_len;
            return FGREP_OK;
        }
        return FGREP_ERR_NO_MATCH;
    }

    regmatch_t rm[1];

    char stack_buf[STACK_BUF_SIZE];
    char *tmp;
    bool on_stack = (len < STACK_BUF_SIZE);
    if (on_stack) {
        tmp = stack_buf;
    } else {
        tmp = malloc(len + 1);
        if (!tmp) return FGREP_ERR_NOMEM;
    }

    memcpy(tmp, data, len);
    tmp[len] = '\0';

    int rc = regexec(&pat->compiled, tmp, 1, rm, 0);

    if (!on_stack) free(tmp);

    if (rc != 0) return FGREP_ERR_NO_MATCH;

    *match_start = (size_t)rm[0].rm_so;
    *match_len = (size_t)(rm[0].rm_eo - rm[0].rm_so);
    return FGREP_OK;
}
