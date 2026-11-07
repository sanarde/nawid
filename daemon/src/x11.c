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
