/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NAWID_TEST_CHECK_H
#define NAWID_TEST_CHECK_H

#include <stdio.h>
#include <string.h>

static int check_failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
                    #cond);                                                  \
            check_failures++;                                                \
        }                                                                    \
    } while (0)

#define CHECK_STR(actual, expected)                                          \
    do {                                                                     \
        const char *a_ = (actual), *e_ = (expected);                         \
        if (!a_ || strcmp(a_, e_) != 0) {                                    \
            fprintf(stderr, "%s:%d: %s = \"%s\", expected \"%s\"\n",         \
                    __FILE__, __LINE__, #actual, a_ ? a_ : "(null)", e_);    \
            check_failures++;                                                \
        }                                                                    \
    } while (0)

#endif
