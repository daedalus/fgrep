#include "output.h"
#include <string.h>
#include <unistd.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_MATCH   "\033[1;31m"
#define COLOR_FILE    "\033[1;35m"
#define COLOR_LINENO  "\033[1;32m"
#define COLOR_SEP     "\033[0;36m"

void fgrep_output_match(FILE *out, const fgrep_match_t *m, const fgrep_options_t *opts) {
    bool use_color = opts->color && isatty(fileno(out));

    if (opts->files_with_matches) {
        if (use_color) fprintf(out, "%s%s%s\n", COLOR_FILE, m->path, COLOR_RESET);
        else           fprintf(out, "%s\n", m->path);
        return;
    }

    if (use_color) fprintf(out, "%s%s%s%s", COLOR_FILE, m->path, COLOR_SEP, ":");

    if (opts->line_number) {
        if (use_color) fprintf(out, "%s%zu%s%s", COLOR_LINENO, m->line_no, COLOR_SEP, ":");
        else           fprintf(out, "%zu:", m->line_no);
    }

    size_t line_len = m->line_end - m->line_start;

    if (use_color) {
        if (m->match_offset > 0) fwrite(m->data, 1, m->match_offset, out);
        fprintf(out, "%s", COLOR_MATCH);
        fwrite(m->data + m->match_offset, 1, m->match_len, out);
        fprintf(out, "%s", COLOR_RESET);
        if (m->match_offset + m->match_len < line_len)
            fwrite(m->data + m->match_offset + m->match_len, 1,
                   line_len - m->match_offset - m->match_len, out);
    } else {
        fwrite(m->data, 1, line_len, out);
    }

    fputc('\n', out);
}

void fgrep_output_count(FILE *out, const char *path, size_t count) {
    fprintf(out, "%s:%zu\n", path, count);
}

void fgrep_output_file_match(FILE *out, const char *path) {
    fprintf(out, "%s\n", path);
}

void fgrep_output_file_no_match(FILE *out, const char *path) {
    fprintf(out, "%s\n", path);
}
