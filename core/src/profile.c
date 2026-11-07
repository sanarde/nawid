/* SPDX-License-Identifier: GPL-3.0-only */
#include "profile.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool listed(const char *const *names, size_t count, const char *name)
{
    for (size_t i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0)
            return true;
    }
    return false;
}

static struct nawid_profile *profile_get(struct nawid_profiles *profiles, const char *name)
{
    struct nawid_profile *grown;

    for (size_t i = 0; i < profiles->count; i++) {
        if (strcmp(profiles->items[i].name, name) == 0)
            return &profiles->items[i];
    }
    grown = realloc(profiles->items, (profiles->count + 1) * sizeof(*grown));
    if (!grown)
        return NULL;
    profiles->items = grown;
    grown = &profiles->items[profiles->count++];
    memset(grown, 0, sizeof(*grown));
    snprintf(grown->name, sizeof(grown->name), "%s", name);
    return grown;
}

size_t nawid_profiles_load(struct nawid_profiles *profiles, char *const *items, size_t count)
{
    size_t skipped = 0;

    memset(profiles, 0, sizeof(*profiles));
    for (size_t i = 0; i < count; i++) {
        char name[NAWID_NAME_MAX];
        struct nawid_output output;
        struct nawid_profile *profile;
        struct nawid_output *grown;

        if (nawid_item_parse(items[i], name, sizeof(name), &output) != 0) {
            skipped++;
            continue;
        }
        profile = profile_get(profiles, name);
        if (!profile || nawid_profile_entry(profile, output.name)) {
            skipped++;
            continue;
        }
        grown = realloc(profile->entries, (profile->count + 1) * sizeof(*grown));
        if (!grown) {
            skipped++;
            continue;
        }
        profile->entries = grown;
        profile->entries[profile->count++] = output;
    }
    return skipped;
}

void nawid_profiles_free(struct nawid_profiles *profiles)
{
    for (size_t i = 0; i < profiles->count; i++)
        free(profiles->items[i].entries);
    free(profiles->items);
    memset(profiles, 0, sizeof(*profiles));
}

const struct nawid_profile *nawid_profiles_find(const struct nawid_profiles *profiles,
                                                const char *name)
{
    for (size_t i = 0; i < profiles->count; i++) {
        if (strcmp(profiles->items[i].name, name) == 0)
            return &profiles->items[i];
    }
    return NULL;
}

const struct nawid_output *nawid_profile_entry(const struct nawid_profile *profile,
                                               const char *output)
{
    for (size_t i = 0; i < profile->count; i++) {
        if (strcmp(profile->entries[i].name, output) == 0)
            return &profile->entries[i];
    }
    return NULL;
}

const struct nawid_profile *nawid_profiles_select(const struct nawid_profiles *profiles,
                                                  const char *const *names, size_t count)
{
    const struct nawid_profile *best = NULL;
    size_t best_score = 0;

    for (size_t i = 0; i < profiles->count; i++) {
        const struct nawid_profile *profile = &profiles->items[i];
        size_t score = 0;
        bool matches = true;

        for (size_t j = 0; j < count && matches; j++)
            matches = nawid_profile_entry(profile, names[j]) != NULL;
        for (size_t j = 0; j < profile->count && matches; j++) {
            if (profile->entries[j].enabled) {
                matches = listed(names, count, profile->entries[j].name);
                score++;
            }
        }
        if (matches && count > 0 && (!best || score > best_score)) {
            best = profile;
            best_score = score;
        }
    }
    return best;
}
