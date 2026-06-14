#include "simd.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void test_memchr_basic(void) {
    simd_memchr_fn fn = simd_get_memchr();
    const char *data = "hello world";
    void *pos = fn(data, 'w', strlen(data));
    assert(pos != NULL);
    assert((const char *)pos == data + 6);
    printf("PASS: test_memchr_basic\n");
}

static void test_memchr_not_found(void) {
    simd_memchr_fn fn = simd_get_memchr();
    const char *data = "hello world";
    void *pos = fn(data, 'z', strlen(data));
    assert(pos == NULL);
    printf("PASS: test_memchr_not_found\n");
}

static void test_memchr_empty(void) {
    simd_memchr_fn fn = simd_get_memchr();
    void *pos = fn("", 'a', 0);
    assert(pos == NULL);
    printf("PASS: test_memchr_empty\n");
}

static void test_memchr_large(void) {
    simd_memchr_fn fn = simd_get_memchr();
    char data[4096];
    memset(data, 'a', sizeof(data));
    data[2048] = 'b';
    void *pos = fn(data, 'b', sizeof(data));
    assert(pos != NULL);
    assert((const char *)pos == data + 2048);
    printf("PASS: test_memchr_large\n");
}

static void test_memchr_first_byte(void) {
    simd_memchr_fn fn = simd_get_memchr();
    const char *data = "target rest";
    void *pos = fn(data, 't', strlen(data));
    assert(pos != NULL);
    assert((const char *)pos == data);
    printf("PASS: test_memchr_first_byte\n");
}

static void test_memchr_last_byte(void) {
    simd_memchr_fn fn = simd_get_memchr();
    const char *data = "rest target";
    void *pos = fn(data, 't', strlen(data));
    assert(pos != NULL);
    assert((const char *)pos == data + 6);
    printf("PASS: test_memchr_last_byte\n");
}

static void test_find_newline(void) {
    const char *data = "line1\nline2\nline3";
    size_t pos = simd_find_newline(data, strlen(data));
    assert(pos == 5);
    printf("PASS: test_find_newline\n");
}

static void test_find_newline_none(void) {
    const char *data = "no newline here";
    size_t pos = simd_find_newline(data, strlen(data));
    assert(pos == strlen(data));
    printf("PASS: test_find_newline_none\n");
}

int main(void) {
    printf("Running SIMD tests...\n");
    test_memchr_basic();
    test_memchr_not_found();
    test_memchr_empty();
    test_memchr_large();
    test_memchr_first_byte();
    test_memchr_last_byte();
    test_find_newline();
    test_find_newline_none();
    printf("All SIMD tests passed!\n");
    return 0;
}
