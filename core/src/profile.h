/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NAWID_PROFILE_H
#define NAWID_PROFILE_H

#include <stddef.h>

#include "spec.h"

struct nawid_profile {
    char name[NAWID_NAME_MAX];
    struct nawid_output *entries;
    size_t count;
};

struct nawid_profiles {
    struct nawid_profile *items;
    size_t count;
};

size_t nawid_profiles_load(struct nawid_profiles *profiles, char *const *items, size_t count);
void nawid_profiles_free(struct nawid_profiles *profiles);
const struct nawid_profile *nawid_profiles_find(const struct nawid_profiles *profiles,
                                                const char *name);
const struct nawid_output *nawid_profile_entry(const struct nawid_profile *profile,
                                               const char *output);
const struct nawid_profile *nawid_profiles_select(const struct nawid_profiles *profiles,
                                                  const char *const *names, size_t count);

#endif
