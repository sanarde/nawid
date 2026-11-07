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
    bool reading;
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
