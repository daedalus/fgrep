#include "regex_engine.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void test_fixed_string_match(void) {
    fgrep_pattern_t pat;
    fgrep_status_t st = fgrep_pattern_compile(&pat, "hello", true, false);
    assert(st == FGREP_OK);

    size_t start, len;
    st = fgrep_pattern_match(&pat, "say hello world", 15, &start, &len);
    assert(st == FGREP_OK);
    assert(start == 4);
    assert(len == 5);

    fgrep_pattern_destroy(&pat);
    printf("PASS: test_fixed_string_match\n");
}

static void test_fixed_string_no_match(void) {
    fgrep_pattern_t pat;
    fgrep_status_t st = fgrep_pattern_compile(&pat, "xyz", true, false);
    assert(st == FGREP_OK);

    size_t start, len;
    st = fgrep_pattern_match(&pat, "hello world", 11, &start, &len);
    assert(st == FGREP_ERR_NO_MATCH);

    fgrep_pattern_destroy(&pat);
    printf("PASS: test_fixed_string_no_match\n");
}

static void test_regex_simple(void) {
    fgrep_pattern_t pat;
    fgrep_status_t st = fgrep_pattern_compile(&pat, "[0-9]+", false, false);
    assert(st == FGREP_OK);

    size_t start, len;
    st = fgrep_pattern_match(&pat, "abc123def", 9, &start, &len);
    assert(st == FGREP_OK);
    assert(start == 3);
    assert(len == 3);

    fgrep_pattern_destroy(&pat);
    printf("PASS: test_regex_simple\n");
}

static void test_regex_no_match(void) {
    fgrep_pattern_t pat;
    fgrep_status_t st = fgrep_pattern_compile(&pat, "[0-9]+", false, false);
    assert(st == FGREP_OK);

    size_t start, len;
    st = fgrep_pattern_match(&pat, "hello world", 11, &start, &len);
    assert(st == FGREP_ERR_NO_MATCH);

    fgrep_pattern_destroy(&pat);
    printf("PASS: test_regex_no_match\n");
}

static void test_regex_case_insensitive(void) {
    fgrep_pattern_t pat;
    fgrep_status_t st = fgrep_pattern_compile(&pat, "hello", false, true);
    assert(st == FGREP_OK);

    size_t start, len;
    st = fgrep_pattern_match(&pat, "HELLO world", 11, &start, &len);
    assert(st == FGREP_OK);
    assert(start == 0);
    assert(len == 5);

    fgrep_pattern_destroy(&pat);
    printf("PASS: test_regex_case_insensitive\n");
}

static void test_regex_invalid(void) {
    fgrep_pattern_t pat;
    fgrep_status_t st = fgrep_pattern_compile(&pat, "[invalid", false, false);
    assert(st == FGREP_ERR_PATTERN);
    printf("PASS: test_regex_invalid\n");
}

static void test_fixed_empty(void) {
    fgrep_pattern_t pat;
    fgrep_status_t st = fgrep_pattern_compile(&pat, "", true, false);
    assert(st == FGREP_OK);

    size_t start, len;
    st = fgrep_pattern_match(&pat, "hello", 5, &start, &len);
    assert(st == FGREP_ERR_NO_MATCH);

    fgrep_pattern_destroy(&pat);
    printf("PASS: test_fixed_empty\n");
}

int main(void) {
    printf("Running regex engine tests...\n");
    test_fixed_string_match();
    test_fixed_string_no_match();
    test_regex_simple();
    test_regex_no_match();
    test_regex_case_insensitive();
    test_regex_invalid();
    test_fixed_empty();
    printf("All regex engine tests passed!\n");
    return 0;
}
