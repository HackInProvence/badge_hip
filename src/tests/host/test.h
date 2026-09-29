/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Minimal test framework for the host tests (see run_tests.py) */

#ifndef HOST_TEST_H
#define HOST_TEST_H

#include <stdio.h>

static int test_failures = 0, test_passes = 0;

#define CHECK(cond) do { \
    if (cond) { ++test_passes; } \
    else { ++test_failures; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define CHECK_EQ(a, b) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (_a == _b) { ++test_passes; } \
    else { ++test_failures; printf("FAIL %s:%d: %s == %s (%lld != %lld)\n", __FILE__, __LINE__, #a, #b, _a, _b); } \
} while (0)

#define CHECK_STR(a, b) do { \
    const char *_a = (a), *_b = (b); \
    if (! strcmp(_a, _b)) { ++test_passes; } \
    else { ++test_failures; printf("FAIL %s:%d: \"%s\" == \"%s\"\n", __FILE__, __LINE__, _a, _b); } \
} while (0)

#define TEST_END() do { \
    printf("%d checks, %d failed\n", test_passes + test_failures, test_failures); \
    return test_failures ? 1 : 0; \
} while (0)

#endif
