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

static int parse_spec(const char *spec, struct nawid_output *output)
{
    const char *cursor = spec;
    long width, height, x, y;
    double refresh = 0.0;
    double scale;
    int transform;

    if (strcmp(spec, "off") == 0) {
        output->enabled = false;
        return 0;
    }
    if (!parse_long(&cursor, &width) || !expect(&cursor, 'x') || !parse_long(&cursor, &height))
        return -1;
    if (expect(&cursor, '@') && (!parse_double(&cursor, &refresh) || refresh <= 0.0 ||
                                 refresh > SIZE_LIMIT))
        return -1;
    if (!expect(&cursor, ',') || !parse_long(&cursor, &x) || !expect(&cursor, ',') ||
        !parse_long(&cursor, &y) || !expect(&cursor, ',') || !parse_double(&cursor, &scale) ||
        !expect(&cursor, ','))
        return -1;
    transform = nawid_transform_parse(cursor);
    if (transform < 0 || width <= 0 || width > SIZE_LIMIT || height <= 0 ||
        height > SIZE_LIMIT || x < -SIZE_LIMIT || x > SIZE_LIMIT || y < -SIZE_LIMIT ||
        y > SIZE_LIMIT || scale <= 0.0 || scale > SCALE_LIMIT)
        return -1;

    output->enabled = true;
    output->width = (int)width;
    output->height = (int)height;
    output->refresh = (int)lround(refresh * 1000.0);
    output->x = (int)x;
    output->y = (int)y;
    output->scale = scale;
    output->transform = transform;
    return 0;
}

int nawid_item_parse(const char *item, char *profile, size_t profile_size,
                     struct nawid_output *output)
{
    const char *slash = strchr(item, '/');
    const char *equals;
    size_t profile_length;
    size_t name_length;
    char name[NAWID_NAME_MAX];

    if (!slash)
        return -1;
    equals = strchr(slash + 1, '=');
    if (!equals)
        return -1;
    profile_length = (size_t)(slash - item);
    name_length = (size_t)(equals - slash - 1);
    if (profile_length == 0 || profile_length >= profile_size || name_length == 0 ||
        name_length >= sizeof(name) || memchr(slash + 1, '/', name_length))
        return -1;

    memcpy(profile, item, profile_length);
    profile[profile_length] = '\0';
    if (!nawid_profile_name_valid(profile))
        return -1;
    memcpy(name, slash + 1, name_length);
    name[name_length] = '\0';

    memset(output, 0, sizeof(*output));
    memcpy(output->name, name, name_length + 1);
    output->scale = 1.0;
    return parse_spec(equals + 1, output);
}

int nawid_item_format(const char *profile, const struct nawid_output *output, char *buffer,
                      size_t size)
{
    int written;

    if (!output->enabled) {
        written = snprintf(buffer, size, "%s/%s=off", profile, output->name);
    } else if (output->refresh > 0) {
        written = snprintf(buffer, size, "%s/%s=%dx%d@%d.%03d,%d,%d,%g,%s", profile,
                           output->name, output->width, output->height, output->refresh / 1000,
                           output->refresh % 1000, output->x, output->y, output->scale,
                           nawid_transform_name(output->transform));
    } else {
        written = snprintf(buffer, size, "%s/%s=%dx%d,%d,%d,%g,%s", profile, output->name,
                           output->width, output->height, output->x, output->y, output->scale,
                           nawid_transform_name(output->transform));
    }
    return written < 0 || (size_t)written >= size ? -1 : 0;
}

bool nawid_output_equal(const struct nawid_output *a, const struct nawid_output *b,
                        bool with_scale)
{
    if (a->enabled != b->enabled)
        return false;
    if (!a->enabled)
        return true;
    if (a->x != b->x || a->y != b->y || a->width != b->width || a->height != b->height ||
        a->transform != b->transform)
        return false;
    if (a->refresh > 0 && b->refresh > 0 && abs(a->refresh - b->refresh) > REFRESH_TOLERANCE)
        return false;
    return !with_scale || fabs(a->scale - b->scale) <= SCALE_TOLERANCE;
}

int nawid_mode_pick(const struct nawid_mode *modes, size_t count,
                    const struct nawid_output *wanted)
{
    int best = -1;
    int best_distance = 0;

    for (size_t i = 0; i < count; i++) {
        int distance;

        if (modes[i].width != wanted->width || modes[i].height != wanted->height)
            continue;
        if (wanted->refresh > 0) {
            distance = abs(modes[i].refresh - wanted->refresh);
            if (distance > MODE_TOLERANCE)
                continue;
        } else if (modes[i].preferred) {
            return (int)i;
        } else {
            distance = -modes[i].refresh;
        }
        if (best < 0 || distance < best_distance) {
            best = (int)i;
            best_distance = distance;
        }
    }
    return best;
}
