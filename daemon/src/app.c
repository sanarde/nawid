/* SPDX-License-Identifier: GPL-3.0-only */
#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>

#include "wayland.h"
#include "x11.h"

static size_t backend_heads(struct nawid_app *app, struct nawid_output **heads)
{
    if (app->wl)
        return nawid_wl_heads(app->wl, heads);
    return nawid_x11_heads(app->x11, heads);
}

static int backend_apply(struct nawid_app *app, const struct nawid_output *entries,
                         size_t count)
{
    if (app->wl)
        return nawid_wl_apply(app->wl, entries, count);
    return nawid_x11_apply(app->x11, entries, count);
}

static void set_active(struct nawid_app *app, const char *name)
{
    if (strcmp(app->active, name) == 0)
        return;
    snprintf(app->active, sizeof(app->active), "%s", name);
    nawid_bus_emit_changed(&app->bus, app->active);
}

static const struct nawid_output *head_find(const struct nawid_output *heads, size_t count,
                                            const char *name)
{
    for (size_t i = 0; i < count; i++) {
        if (strcmp(heads[i].name, name) == 0)
            return &heads[i];
    }
    return NULL;
}

static int apply_profile(struct nawid_app *app, const struct nawid_profile *profile)
{
    struct nawid_output *heads = NULL;
    struct nawid_output *entries;
    size_t count = backend_heads(app, &heads);
    size_t entry_count = 0;
    bool same = true;
    int rc = 0;

    entries = calloc(count ? count : 1, sizeof(*entries));
    if (!heads || !entries) {
        free(heads);
        free(entries);
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        const struct nawid_output *entry = nawid_profile_entry(profile, heads[i].name);

        if (!entry)
            continue;
        entries[entry_count++] = *entry;
        if (!nawid_output_equal(entry, head_find(heads, count, entry->name),
                                app->wl != NULL))
            same = false;
    }
    if (entry_count > 0 && !same)
        rc = backend_apply(app, entries, entry_count);
    if (rc == 0)
        set_active(app, profile->name);
    free(heads);
    free(entries);
    return rc;
}

static int evaluate(struct nawid_app *app)
{
    int rc = 0;

    const struct nawid_profile *profile = NULL;
    struct nawid_output *heads = NULL;
    size_t count;

    if (app->override[0] != '\0') {
        profile = nawid_profiles_find(&app->profiles, app->override);
    } else if (app->settings.profile[0] != '\0') {
        profile = nawid_profiles_find(&app->profiles, app->settings.profile);
    } else if (app->settings.automatic) {
        const char **names;

        count = backend_heads(app, &heads);
        names = calloc(count ? count : 1, sizeof(*names));
        if (names) {
            for (size_t i = 0; i < count; i++)
                names[i] = heads[i].name;
            profile = nawid_profiles_select(&app->profiles, names, count);
            free(names);
        }
        free(heads);
    }
    if (!profile) {
        set_active(app, "");
        return 0;
    }
    rc = apply_profile(app, profile);
    if (rc != 0)
        fprintf(stderr, "nawid: cannot apply the profile \"%s\"\n", profile->name);
    return rc;
}

static int name_compare(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static char *signature_make(struct nawid_app *app)
{
    struct nawid_output *heads = NULL;
    size_t count = backend_heads(app, &heads);
    const char **names = calloc(count ? count : 1, sizeof(*names));
    size_t length = 1;
    char *signature;

    if (!heads || !names) {
        free(heads);
        free(names);
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        names[i] = heads[i].name;
        length += strlen(names[i]) + 1;
    }
    qsort(names, count, sizeof(*names), name_compare);
    signature = calloc(length, 1);
    for (size_t i = 0; signature && i < count; i++) {
        strcat(signature, names[i]);
        strcat(signature, "\n");
    }
    free(heads);
    free(names);
    return signature;
}

static void sync_outputs(struct nawid_app *app)
{
    char *signature = signature_make(app);

    if (!signature || (app->signature && strcmp(signature, app->signature) == 0)) {
        free(signature);
        return;
    }
    if (evaluate(app) == 0) {
        free(app->signature);
        app->signature = signature;
    } else {
        free(signature);
    }
}
