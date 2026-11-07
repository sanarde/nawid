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
