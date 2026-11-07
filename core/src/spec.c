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
