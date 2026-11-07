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

static void settings_reload(struct nawid_app *app)
{
    size_t skipped;

    app->override[0] = '\0';
    nawid_settings_load(app->cfgd, &app->settings);
    nawid_profiles_free(&app->profiles);
    skipped = nawid_profiles_load(&app->profiles, app->settings.items,
                                  app->settings.items_count);
    if (skipped > 0)
        fprintf(stderr, "nawid: %zu invalid or duplicate profile entries ignored\n", skipped);
}

static void settings_changed(const char *key, void *user)
{
    (void)key;
    settings_reload(user);
    evaluate(user);
}

static void backend_changed(void *user)
{
    sync_outputs(user);
}

static void backend_applied(enum nawid_apply_result result, void *user)
{
    struct nawid_app *app = user;

    if (result == NAWID_APPLY_FAILED) {
        fprintf(stderr, "nawid: the compositor did not accept the layout\n");
        return;
    }
    if (result == NAWID_APPLY_CANCELLED) {
        free(app->signature);
        app->signature = NULL;
    }
    sync_outputs(app);
}

static int add_source(int epfd, int fd, struct nawid_source *source)
{
    struct epoll_event ev = {.events = EPOLLIN, .data.ptr = source};

    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

int nawid_app_open(struct nawid_app *app, int epfd, enum nawid_backend backend)
{
    int display_fd;

    memset(app, 0, sizeof(*app));
    app->epfd = epfd;
    app->config_source.kind = NAWID_SRC_CONFIG;
    app->display_source.kind = NAWID_SRC_DISPLAY;
    app->events.changed = backend_changed;
    app->events.applied = backend_applied;
    app->bus.connection = NULL;

    app->cfgd = cfgd_open();
    if (!app->cfgd) {
        fprintf(stderr, "nawid: cannot open the settings\n");
        return -1;
    }
    settings_reload(app);
    if (add_source(epfd, cfgd_notify_fd(app->cfgd), &app->config_source) != 0 ||
        cfgd_subscribe(app->cfgd, NAWID_SETTINGS_PREFIX, settings_changed, app) < 0) {
        fprintf(stderr, "nawid: cannot watch the settings\n");
        return -1;
    }

    if (backend == NAWID_BACKEND_AUTO)
        backend = getenv("WAYLAND_DISPLAY") ? NAWID_BACKEND_WAYLAND : NAWID_BACKEND_X11;
    app->backend = backend;
    if (backend == NAWID_BACKEND_WAYLAND) {
        app->wl = nawid_wl_open(&app->events, app);
        if (!app->wl)
            return -1;
        display_fd = nawid_wl_fd(app->wl);
    } else {
        app->x11 = nawid_x11_open(&app->events, app);
        if (!app->x11)
            return -1;
        display_fd = nawid_x11_fd(app->x11);
    }
    if (add_source(epfd, display_fd, &app->display_source) != 0) {
        fprintf(stderr, "nawid: cannot watch the display connection\n");
        return -1;
    }
    if (nawid_bus_open(&app->bus, epfd, app) != 0)
        return -1;

    app->signature = signature_make(app);
    evaluate(app);
    return 0;
}

void nawid_app_close(struct nawid_app *app)
{
    nawid_bus_close(&app->bus);
    nawid_wl_close(app->wl);
    nawid_x11_close(app->x11);
    nawid_profiles_free(&app->profiles);
    nawid_settings_free(&app->settings);
    free(app->signature);
    cfgd_close(app->cfgd);
    memset(app, 0, sizeof(*app));
}

int nawid_app_prepare(struct nawid_app *app)
{
    return app->wl ? nawid_wl_prepare(app->wl) : nawid_x11_dispatch(app->x11);
}

int nawid_app_finish(struct nawid_app *app, bool display_ready)
{
    if (app->wl)
        return nawid_wl_finish(app->wl, display_ready);
    return display_ready ? nawid_x11_dispatch(app->x11) : 0;
}

void nawid_app_config_event(struct nawid_app *app)
{
    cfgd_dispatch(app->cfgd);
}

size_t nawid_app_heads(struct nawid_app *app, struct nawid_output **heads)
{
    return backend_heads(app, heads);
}

int nawid_app_apply(struct nawid_app *app, const char *name)
{
    const struct nawid_profile *profile = NULL;

    if (name[0] != '\0') {
        profile = nawid_profiles_find(&app->profiles, name);
        if (!profile)
            return -2;
        snprintf(app->override, sizeof(app->override), "%s", name);
        return apply_profile(app, profile);
    }
    app->override[0] = '\0';
    return evaluate(app);
}
