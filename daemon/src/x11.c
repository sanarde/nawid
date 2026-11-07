/* SPDX-License-Identifier: GPL-3.0-only */
#include "x11.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/randr.h>
#include <xcb/xcb.h>

#define MM_PER_INCH_TIMES_100 2540
#define DPI 96

struct nawid_x11 {
    xcb_connection_t *connection;
    xcb_screen_t *screen;
    uint8_t first_event;
    const struct nawid_events *events;
    void *user;
};

struct x11_head {
    xcb_randr_output_t id;
    xcb_randr_crtc_t crtc;
    xcb_randr_crtc_t wanted_crtc;
    xcb_randr_get_output_info_reply_t *info;
    struct nawid_output state;
    struct nawid_output wanted;
    xcb_randr_mode_t current_mode;
    xcb_randr_mode_t wanted_mode;
    uint16_t current_rotation;
    uint16_t wanted_rotation;
};

struct x11_view {
    xcb_randr_get_screen_resources_current_reply_t *resources;
    struct x11_head *heads;
    size_t count;
};

static int refresh_of(const xcb_randr_mode_info_t *mode)
{
    uint64_t total = (uint64_t)mode->htotal * mode->vtotal;

    if (total == 0)
        return 0;
    if (mode->mode_flags & XCB_RANDR_MODE_FLAG_INTERLACE)
        total /= 2;
    if (mode->mode_flags & XCB_RANDR_MODE_FLAG_DOUBLE_SCAN)
        total *= 2;
    return (int)((uint64_t)mode->dot_clock * 1000 / total);
}

static const xcb_randr_mode_info_t *mode_find(const struct x11_view *view,
                                              xcb_randr_mode_t id)
{
    const xcb_randr_mode_info_t *modes =
        xcb_randr_get_screen_resources_current_modes(view->resources);
    int count = xcb_randr_get_screen_resources_current_modes_length(view->resources);

    for (int i = 0; i < count; i++) {
        if (modes[i].id == id)
            return &modes[i];
    }
    return NULL;
}

static int transform_of(uint16_t rotation)
{
    int turns = 0;
    bool flip = rotation & XCB_RANDR_ROTATION_REFLECT_X;

    if (rotation & XCB_RANDR_ROTATION_ROTATE_90)
        turns = 1;
    else if (rotation & XCB_RANDR_ROTATION_ROTATE_180)
        turns = 2;
    else if (rotation & XCB_RANDR_ROTATION_ROTATE_270)
        turns = 3;
    if (rotation & XCB_RANDR_ROTATION_REFLECT_Y) {
        flip = !flip;
        turns = (turns + 2) % 4;
    }
    return flip ? 4 + turns : turns;
}

static int rotation_of(int transform)
{
    switch (transform) {
    case 0:
        return XCB_RANDR_ROTATION_ROTATE_0;
    case 1:
        return XCB_RANDR_ROTATION_ROTATE_90;
    case 2:
        return XCB_RANDR_ROTATION_ROTATE_180;
    case 3:
        return XCB_RANDR_ROTATION_ROTATE_270;
    default:
        return -1;
    }
}

static void view_free(struct x11_view *view)
{
    for (size_t i = 0; i < view->count; i++)
        free(view->heads[i].info);
    free(view->heads);
    free(view->resources);
    memset(view, 0, sizeof(*view));
}

static int view_load(struct nawid_x11 *x11, struct x11_view *view)
{
    xcb_connection_t *c = x11->connection;
    const xcb_randr_output_t *outputs;
    int total;

    memset(view, 0, sizeof(*view));
    view->resources = xcb_randr_get_screen_resources_current_reply(
        c, xcb_randr_get_screen_resources_current(c, x11->screen->root), NULL);
    if (!view->resources)
        return -1;
    outputs = xcb_randr_get_screen_resources_current_outputs(view->resources);
    total = xcb_randr_get_screen_resources_current_outputs_length(view->resources);
    view->heads = calloc(total ? (size_t)total : 1, sizeof(*view->heads));
    if (!view->heads) {
        view_free(view);
        return -1;
    }

    for (int i = 0; i < total; i++) {
        xcb_randr_get_output_info_reply_t *info = xcb_randr_get_output_info_reply(
            c, xcb_randr_get_output_info(c, outputs[i], view->resources->config_timestamp),
            NULL);
        struct x11_head *head;
        int length;

        if (!info)
            continue;
        if (info->connection != XCB_RANDR_CONNECTION_CONNECTED) {
            free(info);
            continue;
        }
        head = &view->heads[view->count++];
        head->id = outputs[i];
        head->info = info;
        head->crtc = info->crtc;
        head->state.scale = 1.0;
        length = xcb_randr_get_output_info_name_length(info);
        snprintf(head->state.name, sizeof(head->state.name), "%.*s", length,
                 (const char *)xcb_randr_get_output_info_name(info));

        if (info->crtc != XCB_NONE) {
            xcb_randr_get_crtc_info_reply_t *crtc = xcb_randr_get_crtc_info_reply(
                c, xcb_randr_get_crtc_info(c, info->crtc, view->resources->config_timestamp),
                NULL);

            if (crtc) {
                const xcb_randr_mode_info_t *mode = mode_find(view, crtc->mode);

                head->state.enabled = crtc->mode != XCB_NONE;
                head->state.x = crtc->x;
                head->state.y = crtc->y;
                head->state.transform = transform_of(crtc->rotation);
                head->current_mode = crtc->mode;
                head->current_rotation = crtc->rotation;
                if (mode) {
                    head->state.width = mode->width;
                    head->state.height = mode->height;
                    head->state.refresh = refresh_of(mode);
                }
                free(crtc);
            }
        }
    }
    return 0;
}

struct nawid_x11 *nawid_x11_open(const struct nawid_events *events, void *user)
{
    struct nawid_x11 *x11 = calloc(1, sizeof(*x11));
    const xcb_query_extension_reply_t *extension;
    xcb_randr_query_version_reply_t *version;
    xcb_screen_iterator_t it;
    int screen_number = 0;

    if (!x11)
        return NULL;
    x11->events = events;
    x11->user = user;
    x11->connection = xcb_connect(NULL, &screen_number);
    if (xcb_connection_has_error(x11->connection)) {
        fprintf(stderr, "nawid: cannot connect to the X server\n");
        goto fail;
    }
    it = xcb_setup_roots_iterator(xcb_get_setup(x11->connection));
    for (int i = 0; i < screen_number && it.rem; i++)
        xcb_screen_next(&it);
    x11->screen = it.data;

    extension = xcb_get_extension_data(x11->connection, &xcb_randr_id);
    version = extension && extension->present
                  ? xcb_randr_query_version_reply(
                        x11->connection, xcb_randr_query_version(x11->connection, 1, 2), NULL)
                  : NULL;
    if (!version || version->major_version < 1 ||
        (version->major_version == 1 && version->minor_version < 2)) {
        free(version);
        fprintf(stderr, "nawid: the X server does not support RandR 1.2\n");
        goto fail;
    }
    free(version);
    x11->first_event = extension->first_event;
    xcb_randr_select_input(x11->connection, x11->screen->root,
                           XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE |
                               XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE |
                               XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE);
    xcb_flush(x11->connection);
    return x11;

fail:
    nawid_x11_close(x11);
    return NULL;
}

void nawid_x11_close(struct nawid_x11 *x11)
{
    if (!x11)
        return;
    if (x11->connection)
        xcb_disconnect(x11->connection);
    free(x11);
}

int nawid_x11_fd(struct nawid_x11 *x11)
{
    return xcb_get_file_descriptor(x11->connection);
}

int nawid_x11_dispatch(struct nawid_x11 *x11)
{
    xcb_generic_event_t *event;
    bool changed = false;

    while ((event = xcb_poll_for_event(x11->connection))) {
        uint8_t type = event->response_type & 0x7f;

        if (type == x11->first_event + XCB_RANDR_SCREEN_CHANGE_NOTIFY ||
            type == x11->first_event + XCB_RANDR_NOTIFY)
            changed = true;
        free(event);
    }
    if (xcb_connection_has_error(x11->connection))
        return -1;
    if (changed)
        x11->events->changed(x11->user);
    return 0;
}

size_t nawid_x11_heads(struct nawid_x11 *x11, struct nawid_output **heads)
{
    struct x11_view view;
    size_t count;

    *heads = NULL;
    if (view_load(x11, &view) != 0)
        return 0;
    count = view.count;
    *heads = calloc(count ? count : 1, sizeof(**heads));
    if (!*heads) {
        view_free(&view);
        return 0;
    }
    for (size_t i = 0; i < count; i++)
        (*heads)[i] = view.heads[i].state;
    view_free(&view);
    return count;
}

static int mode_for(const struct x11_view *view, const struct x11_head *head,
                    const struct nawid_output *entry, xcb_randr_mode_t *id)
{
    int total = xcb_randr_get_output_info_modes_length(head->info);
    const xcb_randr_mode_t *ids = xcb_randr_get_output_info_modes(head->info);
    struct nawid_mode *infos = calloc(total ? (size_t)total : 1, sizeof(*infos));
    int index;

    if (!infos)
        return -1;
    for (int i = 0; i < total; i++) {
        const xcb_randr_mode_info_t *mode = mode_find(view, ids[i]);

        if (mode) {
            infos[i].width = mode->width;
            infos[i].height = mode->height;
            infos[i].refresh = refresh_of(mode);
            infos[i].preferred = i < head->info->num_preferred;
        }
    }
    index = nawid_mode_pick(infos, (size_t)total, entry);
    free(infos);
    if (index < 0)
        return -1;
    *id = ids[index];
    return 0;
}

static bool crtc_allowed(const struct x11_head *head, xcb_randr_crtc_t crtc)
{
    const xcb_randr_crtc_t *crtcs = xcb_randr_get_output_info_crtcs(head->info);
    int total = xcb_randr_get_output_info_crtcs_length(head->info);

    for (int i = 0; i < total; i++) {
        if (crtcs[i] == crtc)
            return true;
    }
    return false;
}

static bool crtc_taken(const struct x11_view *view, xcb_randr_crtc_t crtc)
{
    for (size_t i = 0; i < view->count; i++) {
        if (view->heads[i].wanted_crtc == crtc)
            return true;
    }
    return false;
}

static bool rotation_supported(struct nawid_x11 *x11, const struct x11_view *view,
                               xcb_randr_crtc_t crtc, uint16_t rotation)
{
    xcb_randr_get_crtc_info_reply_t *info = xcb_randr_get_crtc_info_reply(
        x11->connection,
        xcb_randr_get_crtc_info(x11->connection, crtc, view->resources->config_timestamp),
        NULL);
    bool supported = info && (info->rotations & rotation);

    free(info);
    return supported;
}

static int plan(struct nawid_x11 *x11, struct x11_view *view,
                const struct nawid_output *entries, size_t count)
{
    const xcb_randr_crtc_t *crtcs =
        xcb_randr_get_screen_resources_current_crtcs(view->resources);
    int crtc_total = xcb_randr_get_screen_resources_current_crtcs_length(view->resources);

    for (size_t i = 0; i < view->count; i++) {
        struct x11_head *head = &view->heads[i];
        const struct nawid_output *entry = NULL;

        for (size_t j = 0; j < count; j++) {
            if (strcmp(entries[j].name, head->state.name) == 0)
                entry = &entries[j];
        }
        if (!entry) {
            head->wanted = head->state;
            head->wanted_mode = head->current_mode;
            head->wanted_rotation = head->current_rotation;
            head->wanted_crtc = head->state.enabled ? head->crtc : XCB_NONE;
            continue;
        }
        head->wanted = *entry;
        if (!entry->enabled)
            continue;
        if (rotation_of(entry->transform) < 0 ||
            mode_for(view, head, entry, &head->wanted_mode) != 0)
            return -1;
        head->wanted_rotation = (uint16_t)rotation_of(entry->transform);
        if (head->crtc != XCB_NONE && crtc_allowed(head, head->crtc))
            head->wanted_crtc = head->crtc;
    }
    for (size_t i = 0; i < view->count; i++) {
        struct x11_head *head = &view->heads[i];

        if (!head->wanted.enabled || head->wanted_crtc != XCB_NONE)
            continue;
        for (int j = 0; j < crtc_total; j++) {
            if (crtc_allowed(head, crtcs[j]) && !crtc_taken(view, crtcs[j])) {
                head->wanted_crtc = crtcs[j];
                break;
            }
        }
        if (head->wanted_crtc == XCB_NONE)
            return -1;
    }
    for (size_t i = 0; i < view->count; i++) {
        const struct x11_head *head = &view->heads[i];

        if (head->wanted.enabled &&
            !rotation_supported(x11, view, head->wanted_crtc, head->wanted_rotation))
            return -1;
    }
    return 0;
}
