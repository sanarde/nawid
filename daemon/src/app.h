/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NAWID_APP_H
#define NAWID_APP_H

#include <cfgd/cfgd.h>
#include <stdbool.h>

#include "bus.h"
#include "events.h"
#include "loop.h"
#include "profile.h"
#include "settings.h"
#include "spec.h"

enum nawid_backend {
    NAWID_BACKEND_AUTO,
    NAWID_BACKEND_WAYLAND,
    NAWID_BACKEND_X11,
};

struct nawid_wl;
struct nawid_x11;

struct nawid_app {
    int epfd;
    cfgd_t *cfgd;
    enum nawid_backend backend;
    struct nawid_wl *wl;
    struct nawid_x11 *x11;
    struct nawid_settings settings;
    struct nawid_profiles profiles;
    struct nawid_bus bus;
    struct nawid_source config_source;
    struct nawid_source display_source;
    char active[NAWID_NAME_MAX];
    char override[NAWID_NAME_MAX];
    char *signature;
    struct nawid_events events;
};

int nawid_app_open(struct nawid_app *app, int epfd, enum nawid_backend backend);
void nawid_app_close(struct nawid_app *app);
int nawid_app_prepare(struct nawid_app *app);
int nawid_app_finish(struct nawid_app *app, bool display_ready);
void nawid_app_config_event(struct nawid_app *app);
size_t nawid_app_heads(struct nawid_app *app, struct nawid_output **heads);
int nawid_app_apply(struct nawid_app *app, const char *name);

#endif
