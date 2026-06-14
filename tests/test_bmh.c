#include "bmh_simd.h"
#include <stdio.h>
#include <string.h>

int main() {
    const char *text = "hello world ERROR hello ERROR again ERROR";
    size_t text_len = strlen(text);

    /* Test 1: Find "ERROR" */
    size_t count = bmh_search(text, text_len, "ERROR", 5);
    printf("Test 1: Found %zu matches (expected 3)\n", count);

    /* Test 2: Find "hello" */
    count = bmh_search(text, text_len, "hello", 5);
    printf("Test 2: Found %zu matches (expected 2)\n", count);

    /* Test 3: Find single char */
    count = bmh_search(text, text_len, "R", 1);
    printf("Test 3: Found %zu matches (expected 9)\n", count);

    /* Test 4: No match */
    count = bmh_search(text, text_len, "xyz", 3);
    printf("Test 4: Found %zu matches (expected 0)\n", count);

    /* Test 5: Pattern longer than text */
    count = bmh_search(text, text_len, "hello world foo bar", 20);
    printf("Test 5: Found %zu matches (expected 0)\n", count);

    return 0;
}
