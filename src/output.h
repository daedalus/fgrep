#ifndef OUTPUT_H
#define OUTPUT_H

#include "fgrep.h"
#include <stdio.h>

void fgrep_output_match(FILE *out, const fgrep_match_t *match, const fgrep_options_t *opts);
void fgrep_output_count(FILE *out, const char *path, size_t count);
void fgrep_output_file_match(FILE *out, const char *path);
void fgrep_output_file_no_match(FILE *out, const char *path);

#endif
