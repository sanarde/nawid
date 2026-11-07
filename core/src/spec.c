/* SPDX-License-Identifier: GPL-3.0-only */
#include "spec.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIZE_LIMIT 65535
#define SCALE_LIMIT 16.0
#define REFRESH_TOLERANCE 10
#define SCALE_TOLERANCE 0.001
#define MODE_TOLERANCE 500

static const char *const transform_names[] = {
    "normal", "90", "180", "270", "flipped", "flipped-90", "flipped-180", "flipped-270",
};

const char *nawid_transform_name(int transform)
{
    if (transform < 0 || transform >= (int)(sizeof(transform_names) / sizeof(*transform_names)))
        return NULL;
    return transform_names[transform];
}

int nawid_transform_parse(const char *text)
{
    for (size_t i = 0; i < sizeof(transform_names) / sizeof(*transform_names); i++) {
        if (strcmp(text, transform_names[i]) == 0)
            return (int)i;
    }
    return -1;
}

bool nawid_profile_name_valid(const char *name)
{
    size_t length = name ? strlen(name) : 0;

    if (length == 0 || length >= NAWID_NAME_MAX)
        return false;
    for (size_t i = 0; i < length; i++) {
        char c = name[i];

        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '_' || c == '-'))
            return false;
    }
    return true;
}

static bool parse_long(const char **cursor, long *value)
{
    char *end;

    errno = 0;
    *value = strtol(*cursor, &end, 10);
    if (end == *cursor || errno != 0)
        return false;
    *cursor = end;
    return true;
}

static bool parse_double(const char **cursor, double *value)
{
    char *end;

    errno = 0;
    *value = strtod(*cursor, &end);
    if (end == *cursor || errno != 0 || !isfinite(*value))
        return false;
    *cursor = end;
    return true;
}

static bool expect(const char **cursor, char c)
{
    if (**cursor != c)
        return false;
    (*cursor)++;
    return true;
}
