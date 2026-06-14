#include "fgrep.h"
#include "search.h"
#include "regex_engine.h"
#include "fileutil.h"
#include "threadpool.h"
#include "cpu.h"
#include "simd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [OPTIONS] PATTERN [FILE...]\n"
        "Search for PATTERN in files.\n"
        "\n"
        "Options:\n"
        "  -F, --fixed-string   Pattern is a fixed string\n"
        "  -i, --ignore-case    Ignore case distinctions\n"
        "  -w, --word-regexp     Match whole words only\n"
        "  -x, --line-regexp     Match whole lines only\n"
        "  -c, --count           Print match count per file\n"
        "  -l, --files-with-matches  Print only filenames with matches\n"
        "  -L, --files-without-match Print only filenames without matches\n"
        "  -v, --invert-match    Select non-matching lines\n"
        "  -n, --line-number     Show line numbers\n"
        "  -r, --recursive       Search directories recursively\n"
        "  -C, --color=WHEN      Use color (auto, always, never)\n"
        "  -m, --max-count=NUM   Stop after NUM matches per file\n"
        "  -j, --threads=NUM     Number of threads (default: num CPUs)\n"
        "      --cpu-features    Show CPU features and exit\n"
        "  -h, --help            Show this help\n"
        "  -V, --version         Show version\n",
        prog);
}

static int detect_cpu_count(void) {
    int n = (int)sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? n : 1;
}

int main(int argc, char **argv) {
    fgrep_options_t opts = {
        .color = isatty(STDOUT_FILENO),
    };

    int num_threads = detect_cpu_count();

    static struct option long_opts[] = {
        {"fixed-string",      no_argument,       NULL, 'F'},
        {"ignore-case",       no_argument,       NULL, 'i'},
        {"word-regexp",       no_argument,       NULL, 'w'},
        {"line-regexp",       no_argument,       NULL, 'x'},
        {"count",             no_argument,       NULL, 'c'},
        {"files-with-matches",no_argument,       NULL, 'l'},
        {"files-without-match",no_argument,      NULL, 'L'},
        {"invert-match",      no_argument,       NULL, 'v'},
        {"line-number",       no_argument,       NULL, 'n'},
        {"recursive",         no_argument,       NULL, 'r'},
        {"color",             required_argument, NULL, 'C'},
        {"max-count",         required_argument, NULL, 'm'},
        {"threads",           required_argument, NULL, 'j'},
        {"cpu-features",      no_argument,       NULL, 0x100},
        {"help",              no_argument,       NULL, 'h'},
        {"version",           no_argument,       NULL, 'V'},
        {NULL, 0, NULL, 0},
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "FiwxclLvnrm:C:j:hV", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'F': opts.fixed_string = true; break;
        case 'i': opts.ignore_case = true; break;
        case 'w': opts.whole_line = true; break;
        case 'x': opts.whole_line = true; break;
        case 'c': opts.count_only = true; break;
        case 'l': opts.files_with_matches = true; break;
        case 'L': opts.files_without_match = true; break;
        case 'v': opts.invert_match = true; break;
        case 'n': opts.line_number = true; break;
        case 'r': opts.recursive = true; break;
        case 'C':
            if (strcmp(optarg, "always") == 0) opts.color = true;
            else if (strcmp(optarg, "never") == 0) opts.color = false;
            else if (strcmp(optarg, "auto") == 0) opts.color = isatty(STDOUT_FILENO);
            else { fprintf(stderr, "Invalid color mode: %s\n", optarg); return 1; }
            break;
        case 'm': opts.max_count = atoi(optarg); break;
        case 'j': num_threads = atoi(optarg); break;
        case 0x100: {
            fgrep_cpu_features_t feat;
            cpu_detect(&feat);
            printf("CPU features: %s\n", cpu_features_string(&feat));
            return 0;
        }
        case 'h': usage(argv[0]); return 0;
        case 'V': printf("fgrep %s\n", FGREP_VERSION); return 0;
        default:  usage(argv[0]); return 1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "No pattern specified\n");
        usage(argv[0]);
        return 1;
    }

    opts.pattern = argv[optind++];

    if (!opts.fixed_string && !opts.ignore_case) {
        const char *p = opts.pattern;
        bool is_literal = true;
        for (size_t i = 0; p[i]; i++) {
            if (p[i] == '\\') { is_literal = false; break; }
            if (p[i] == '.' || p[i] == '*' || p[i] == '+' || p[i] == '?' ||
                p[i] == '[' || p[i] == ']' || p[i] == '(' || p[i] == ')' ||
                p[i] == '{' || p[i] == '}' || p[i] == '^' || p[i] == '$' ||
                p[i] == '|') { is_literal = false; break; }
        }
        opts.fixed_string = is_literal;
    }
    opts.fixed_string = opts.fixed_string || !opts.pattern[0];

    fgrep_pattern_t pattern;
    fgrep_status_t st = fgrep_pattern_compile(&pattern, opts.pattern, opts.fixed_string, opts.ignore_case);
    if (st != FGREP_OK) {
        fprintf(stderr, "Invalid pattern: %s\n", opts.pattern);
        return 1;
    }

    fgrep_stats_t stats = {0};
    pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;

    fgrep_search_ctx_t ctx = {
        .opts = &opts,
        .pattern = &pattern,
        .stats = &stats,
        .output = stdout,
        .output_mutex = (opts.color || opts.files_with_matches) ? &output_mutex : NULL,
    };

    fgrep_threadpool_t *pool = NULL;
    if (num_threads > 1 && (optind < argc - 1 || opts.recursive)) {
        st = fgrep_threadpool_create(&pool, num_threads);
        if (st != FGREP_OK) {
            fprintf(stderr, "Failed to create thread pool\n");
            fgrep_pattern_destroy(&pattern);
            return 1;
        }
    }

    if (optind >= argc) {
        fgrep_io_t io;
        st = fgrep_io_open_stdin(&io);
        if (st == FGREP_OK) {
            size_t match_count;
            search_data(io.data, io.size, "<stdin>", &ctx, &match_count);
            if (opts.count_only) fgrep_output_count(stdout, "<stdin>", match_count);
            fgrep_io_close(&io);
        }
    } else {
        for (int i = optind; i < argc; i++) {
            fgrep_filelist_t *files = NULL;
            st = fgrep_filelist_create(&files, argv[i], opts.recursive);
            if (st != FGREP_OK) {
                fprintf(stderr, "Cannot access '%s'\n", argv[i]);
                continue;
            }

            const char *path;
            while ((path = fgrep_filelist_next(files)) != NULL) {
                if (pool) {
                    file_work_t *fw = malloc(sizeof(*fw));
                    if (fw) {
                        fw->path = path;
                        fw->ctx = &ctx;
                        fgrep_threadpool_submit(pool, (fgrep_work_fn)fgrep_search_file_worker, fw);
                    }
                } else {
                    fgrep_search_file(path, &ctx);
                }
            }

            if (pool) fgrep_threadpool_wait(pool);
            fgrep_filelist_destroy(files);
        }
    }

    if (pool) fgrep_threadpool_destroy(pool);
    fgrep_pattern_destroy(&pattern);
    pthread_mutex_destroy(&output_mutex);

    int exit_code = 0;
    if (opts.files_with_matches && stats.files_matched == 0) exit_code = 1;
    else if (!opts.invert_match && stats.total_matches == 0) exit_code = 1;
    else if (opts.invert_match && stats.files_matched > 0) exit_code = 1;

    return exit_code;
}
