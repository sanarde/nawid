/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NAWID_SPEC_H
#define NAWID_SPEC_H

#include <stdbool.h>
#include <stddef.h>

#define NAWID_NAME_MAX 64

struct nawid_output {
    char name[NAWID_NAME_MAX];
    bool enabled;
    int x;
    int y;
    int width;
    int height;
    int refresh;
    double scale;
    int transform;
};

struct nawid_mode {
    int width;
    int height;
    int refresh;
    bool preferred;
};

const char *nawid_transform_name(int transform);
int nawid_transform_parse(const char *text);
bool nawid_profile_name_valid(const char *name);
int nawid_item_parse(const char *item, char *profile, size_t profile_size,
                     struct nawid_output *output);
int nawid_item_format(const char *profile, const struct nawid_output *output, char *buffer,
                      size_t size);
int nawid_mode_pick(const struct nawid_mode *modes, size_t count,
                    const struct nawid_output *wanted);
bool nawid_output_equal(const struct nawid_output *a, const struct nawid_output *b,
                        bool with_scale);

#endif
