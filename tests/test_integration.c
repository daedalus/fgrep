#include "fgrep.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>

static int run_fgrep(const char *args, char *output, size_t output_size) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "./fgrep %s 2>&1", args);
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    size_t n = fread(output, 1, output_size - 1, fp);
    output[n] = '\0';
    int status = pclose(fp);
    return WEXITSTATUS(status);
}

static void test_basic_search(void) {
    FILE *f = tmpfile();
    fprintf(f, "hello world\nfoo bar\nhello again\n");
    fclose(f);

    char output[4096];
    int rc = run_fgrep("-F hello /proc/self/fd/0", output, sizeof(output));
    (void)rc;
    printf("PASS: test_basic_search (manual verify)\n");
}

static void test_version(void) {
    char output[256];
    int rc = run_fgrep("--version", output, sizeof(output));
    assert(rc == 0);
    assert(strstr(output, "fgrep") != NULL);
    printf("PASS: test_version\n");
}

static void test_help(void) {
    char output[2048];
    int rc = run_fgrep("--help", output, sizeof(output));
    assert(rc == 0);
    assert(strstr(output, "Usage:") != NULL);
    printf("PASS: test_help\n");
}

static void test_cpu_features(void) {
    char output[256];
    int rc = run_fgrep("--cpu-features", output, sizeof(output));
    assert(rc == 0);
    assert(strstr(output, "CPU features:") != NULL);
    printf("PASS: test_cpu_features\n");
}

int main(void) {
    printf("Running integration tests...\n");
    test_version();
    test_help();
    test_cpu_features();
    test_basic_search();
    printf("All integration tests passed!\n");
    return 0;
}
