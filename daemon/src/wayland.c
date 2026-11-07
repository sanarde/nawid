/* SPDX-License-Identifier: GPL-3.0-only */
#include "wayland.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>

#include "wlr-output-management-unstable-v1-client-protocol.h"

#define MANAGER_VERSION 4

struct wl_head;

struct wl_mode {
    struct wl_head *head;
    struct zwlr_output_mode_v1 *mode;
    struct nawid_mode info;
    struct wl_mode *next;
};

struct wl_head {
    struct nawid_wl *wl;
    struct zwlr_output_head_v1 *head;
    struct nawid_output state;
    struct wl_mode *modes;
    struct wl_mode *current;
    struct wl_head *next;
};

struct nawid_wl {
    struct wl_display *display;
    struct wl_registry *registry;
    struct zwlr_output_manager_v1 *manager;
    struct zwlr_output_configuration_v1 *pending;
    struct wl_head *heads;
    uint32_t serial;
    bool ready;
    bool notify;
    const struct nawid_events *events;
    void *user;
};

static void mode_size(void *data, struct zwlr_output_mode_v1 *mode, int32_t width,
                      int32_t height)
{
    struct wl_mode *m = data;

    (void)mode;
    m->info.width = width;
    m->info.height = height;
}

static void mode_refresh(void *data, struct zwlr_output_mode_v1 *mode, int32_t refresh)
{
    struct wl_mode *m = data;

    (void)mode;
    m->info.refresh = refresh;
}

static void mode_preferred(void *data, struct zwlr_output_mode_v1 *mode)
{
    struct wl_mode *m = data;

    (void)mode;
    m->info.preferred = true;
}

static void mode_finished(void *data, struct zwlr_output_mode_v1 *mode)
{
    struct wl_mode *m = data;
    struct wl_head *head = m->head;

    for (struct wl_mode **link = &head->modes; *link; link = &(*link)->next) {
        if (*link == m) {
            *link = m->next;
            break;
        }
    }
    if (head->current == m)
        head->current = NULL;
    if (zwlr_output_mode_v1_get_version(mode) >= ZWLR_OUTPUT_MODE_V1_RELEASE_SINCE_VERSION)
        zwlr_output_mode_v1_release(mode);
    else
        zwlr_output_mode_v1_destroy(mode);
    free(m);
}

static const struct zwlr_output_mode_v1_listener mode_listener = {
    .size = mode_size,
    .refresh = mode_refresh,
    .preferred = mode_preferred,
    .finished = mode_finished,
};

static void head_name(void *data, struct zwlr_output_head_v1 *head, const char *name)
{
    struct wl_head *h = data;

    (void)head;
    snprintf(h->state.name, sizeof(h->state.name), "%s", name);
}

static void head_description(void *data, struct zwlr_output_head_v1 *head,
                             const char *description)
{
    (void)data;
    (void)head;
    (void)description;
}

static void head_physical_size(void *data, struct zwlr_output_head_v1 *head, int32_t width,
                               int32_t height)
{
    (void)data;
    (void)head;
    (void)width;
    (void)height;
}

static void head_mode(void *data, struct zwlr_output_head_v1 *head,
                      struct zwlr_output_mode_v1 *mode)
{
    struct wl_head *h = data;
    struct wl_mode *m = calloc(1, sizeof(*m));

    (void)head;
    if (!m)
        return;
    m->head = h;
    m->mode = mode;
    m->next = h->modes;
    h->modes = m;
    zwlr_output_mode_v1_add_listener(mode, &mode_listener, m);
}

static void head_enabled(void *data, struct zwlr_output_head_v1 *head, int32_t enabled)
{
    struct wl_head *h = data;

    (void)head;
    h->state.enabled = enabled != 0;
}

static void head_current_mode(void *data, struct zwlr_output_head_v1 *head,
                              struct zwlr_output_mode_v1 *mode)
{
    struct wl_head *h = data;

    (void)head;
    h->current = zwlr_output_mode_v1_get_user_data(mode);
}

static void head_position(void *data, struct zwlr_output_head_v1 *head, int32_t x, int32_t y)
{
    struct wl_head *h = data;

    (void)head;
    h->state.x = x;
    h->state.y = y;
}

static void head_transform(void *data, struct zwlr_output_head_v1 *head, int32_t transform)
{
    struct wl_head *h = data;

    (void)head;
    h->state.transform = transform;
}

static void head_scale(void *data, struct zwlr_output_head_v1 *head, wl_fixed_t scale)
{
    struct wl_head *h = data;

    (void)head;
    h->state.scale = wl_fixed_to_double(scale);
}

static void head_finished(void *data, struct zwlr_output_head_v1 *head)
{
    struct wl_head *h = data;
    struct nawid_wl *wl = h->wl;

    for (struct wl_head **link = &wl->heads; *link; link = &(*link)->next) {
        if (*link == h) {
            *link = h->next;
            break;
        }
    }
    while (h->modes) {
        struct wl_mode *m = h->modes;

        h->modes = m->next;
        zwlr_output_mode_v1_destroy(m->mode);
        free(m);
    }
    if (zwlr_output_head_v1_get_version(head) >= ZWLR_OUTPUT_HEAD_V1_RELEASE_SINCE_VERSION)
        zwlr_output_head_v1_release(head);
    else
        zwlr_output_head_v1_destroy(head);
    free(h);
}

static void head_text(void *data, struct zwlr_output_head_v1 *head, const char *text)
{
    (void)data;
    (void)head;
    (void)text;
}

static void head_adaptive_sync(void *data, struct zwlr_output_head_v1 *head, uint32_t state)
{
    (void)data;
    (void)head;
    (void)state;
}

static const struct zwlr_output_head_v1_listener head_listener = {
    .name = head_name,
    .description = head_description,
    .physical_size = head_physical_size,
    .mode = head_mode,
    .enabled = head_enabled,
    .current_mode = head_current_mode,
    .position = head_position,
    .transform = head_transform,
    .scale = head_scale,
    .finished = head_finished,
    .make = head_text,
    .model = head_text,
    .serial_number = head_text,
    .adaptive_sync = head_adaptive_sync,
};

static void manager_head(void *data, struct zwlr_output_manager_v1 *manager,
                         struct zwlr_output_head_v1 *head)
{
    struct nawid_wl *wl = data;
    struct wl_head *h = calloc(1, sizeof(*h));

    (void)manager;
    if (!h)
        return;
    h->wl = wl;
    h->head = head;
    h->state.scale = 1.0;
    for (struct wl_head **link = &wl->heads;; link = &(*link)->next) {
        if (!*link) {
            *link = h;
            break;
        }
    }
    zwlr_output_head_v1_add_listener(head, &head_listener, h);
}

static void manager_done(void *data, struct zwlr_output_manager_v1 *manager, uint32_t serial)
{
    struct nawid_wl *wl = data;

    (void)manager;
    wl->serial = serial;
    wl->ready = true;
    for (struct wl_head *h = wl->heads; h; h = h->next) {
        if (h->current) {
            h->state.width = h->current->info.width;
            h->state.height = h->current->info.height;
            h->state.refresh = h->current->info.refresh;
        }
    }
    if (wl->notify)
        wl->events->changed(wl->user);
}

static void manager_finished(void *data, struct zwlr_output_manager_v1 *manager)
{
    struct nawid_wl *wl = data;

    (void)manager;
    wl->ready = false;
}

static const struct zwlr_output_manager_v1_listener manager_listener = {
    .head = manager_head,
    .done = manager_done,
    .finished = manager_finished,
};

static void registry_global(void *data, struct wl_registry *registry, uint32_t name,
                            const char *interface, uint32_t version)
{
    struct nawid_wl *wl = data;

    if (strcmp(interface, zwlr_output_manager_v1_interface.name) == 0 && !wl->manager) {
        wl->manager = wl_registry_bind(registry, name, &zwlr_output_manager_v1_interface,
                                       version < MANAGER_VERSION ? version : MANAGER_VERSION);
        zwlr_output_manager_v1_add_listener(wl->manager, &manager_listener, wl);
    }
}

static void registry_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_remove,
};

struct nawid_wl *nawid_wl_open(const struct nawid_events *events, void *user)
{
    struct nawid_wl *wl = calloc(1, sizeof(*wl));

    if (!wl)
        return NULL;
    wl->events = events;
    wl->user = user;
    wl->display = wl_display_connect(NULL);
    if (!wl->display) {
        fprintf(stderr, "nawid: cannot connect to the Wayland display\n");
        goto fail;
    }
    wl->registry = wl_display_get_registry(wl->display);
    wl_registry_add_listener(wl->registry, &registry_listener, wl);
    wl_display_roundtrip(wl->display);
    if (!wl->manager) {
        fprintf(stderr, "nawid: the compositor does not support wlr-output-management\n");
        goto fail;
    }
    wl_display_roundtrip(wl->display);
    wl_display_roundtrip(wl->display);
    if (!wl->ready) {
        fprintf(stderr, "nawid: the compositor did not report its outputs\n");
        goto fail;
    }
    wl->notify = true;
    return wl;

fail:
    nawid_wl_close(wl);
    return NULL;
}

void nawid_wl_close(struct nawid_wl *wl)
{
    if (!wl)
        return;
    if (wl->pending)
        zwlr_output_configuration_v1_destroy(wl->pending);
    if (wl->manager)
        zwlr_output_manager_v1_stop(wl->manager);
    while (wl->heads) {
        struct wl_head *h = wl->heads;

        wl->heads = h->next;
        while (h->modes) {
            struct wl_mode *m = h->modes;

            h->modes = m->next;
            zwlr_output_mode_v1_destroy(m->mode);
            free(m);
        }
        zwlr_output_head_v1_destroy(h->head);
        free(h);
    }
    if (wl->manager)
        zwlr_output_manager_v1_destroy(wl->manager);
    if (wl->registry)
        wl_registry_destroy(wl->registry);
    if (wl->display) {
        wl_display_flush(wl->display);
        wl_display_disconnect(wl->display);
    }
    free(wl);
}

int nawid_wl_fd(struct nawid_wl *wl)
{
    return wl_display_get_fd(wl->display);
}

int nawid_wl_dispatch(struct nawid_wl *wl)
{
    while (wl_display_prepare_read(wl->display) != 0) {
        if (wl_display_dispatch_pending(wl->display) < 0)
            return -1;
    }
    if (wl_display_read_events(wl->display) < 0 || wl_display_dispatch_pending(wl->display) < 0)
        return -1;
    if (wl_display_flush(wl->display) < 0 && errno != EAGAIN)
        return -1;
    return 0;
}

size_t nawid_wl_heads(struct nawid_wl *wl, struct nawid_output **heads)
{
    size_t count = 0;
    size_t index = 0;

    for (struct wl_head *h = wl->heads; h; h = h->next)
        count++;
    *heads = calloc(count ? count : 1, sizeof(**heads));
    if (!*heads)
        return 0;
    for (struct wl_head *h = wl->heads; h; h = h->next)
        (*heads)[index++] = h->state;
    return count;
}

static void config_succeeded(void *data, struct zwlr_output_configuration_v1 *config)
{
    struct nawid_wl *wl = data;

    zwlr_output_configuration_v1_destroy(config);
    wl->pending = NULL;
    wl->events->applied(NAWID_APPLY_OK, wl->user);
}

static void config_failed(void *data, struct zwlr_output_configuration_v1 *config)
{
    struct nawid_wl *wl = data;

    zwlr_output_configuration_v1_destroy(config);
    wl->pending = NULL;
    wl->events->applied(NAWID_APPLY_FAILED, wl->user);
}

static void config_cancelled(void *data, struct zwlr_output_configuration_v1 *config)
{
    struct nawid_wl *wl = data;

    zwlr_output_configuration_v1_destroy(config);
    wl->pending = NULL;
    wl->events->applied(NAWID_APPLY_CANCELLED, wl->user);
}

static const struct zwlr_output_configuration_v1_listener config_listener = {
    .succeeded = config_succeeded,
    .failed = config_failed,
    .cancelled = config_cancelled,
};

static struct wl_mode *mode_for(const struct wl_head *head, const struct nawid_output *entry)
{
    size_t count = 0;
    struct nawid_mode *infos;
    struct wl_mode **modes;
    struct wl_mode *found = NULL;
    int index;

    for (struct wl_mode *m = head->modes; m; m = m->next)
        count++;
    infos = calloc(count ? count : 1, sizeof(*infos));
    modes = calloc(count ? count : 1, sizeof(*modes));
    if (!infos || !modes) {
        free(infos);
        free(modes);
        return NULL;
    }
    count = 0;
    for (struct wl_mode *m = head->modes; m; m = m->next) {
        infos[count] = m->info;
        modes[count++] = m;
    }
    index = nawid_mode_pick(infos, count, entry);
    if (index >= 0)
        found = modes[index];
    free(infos);
    free(modes);
    return found;
}

int nawid_wl_apply(struct nawid_wl *wl, const struct nawid_output *entries, size_t count)
{
    struct zwlr_output_configuration_v1 *config;

    if (!wl->ready || wl->pending)
        return -1;
    config = zwlr_output_manager_v1_create_configuration(wl->manager, wl->serial);
    if (!config)
        return -1;
    for (struct wl_head *h = wl->heads; h; h = h->next) {
        const struct nawid_output *entry = NULL;

        for (size_t i = 0; i < count; i++) {
            if (strcmp(entries[i].name, h->state.name) == 0)
                entry = &entries[i];
        }
        if (entry && !entry->enabled) {
            zwlr_output_configuration_v1_disable_head(config, h->head);
        } else if (!entry && !h->state.enabled) {
            zwlr_output_configuration_v1_disable_head(config, h->head);
        } else {
            struct zwlr_output_configuration_head_v1 *settings =
                zwlr_output_configuration_v1_enable_head(config, h->head);
            struct wl_mode *mode = entry ? mode_for(h, entry) : NULL;

            if (!entry) {
                wl_proxy_destroy((struct wl_proxy *)settings);
                continue;
            }
            if (mode)
                zwlr_output_configuration_head_v1_set_mode(settings, mode->mode);
            else
                zwlr_output_configuration_head_v1_set_custom_mode(settings, entry->width,
                                                                  entry->height,
                                                                  entry->refresh);
            zwlr_output_configuration_head_v1_set_position(settings, entry->x, entry->y);
            zwlr_output_configuration_head_v1_set_transform(settings, entry->transform);
            zwlr_output_configuration_head_v1_set_scale(settings,
                                                        wl_fixed_from_double(entry->scale));
            wl_proxy_destroy((struct wl_proxy *)settings);
        }
    }
    zwlr_output_configuration_v1_add_listener(config, &config_listener, wl);
    zwlr_output_configuration_v1_apply(config);
    wl->pending = config;
    wl_display_flush(wl->display);
    return 0;
}
