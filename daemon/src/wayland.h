/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NAWID_WAYLAND_H
#define NAWID_WAYLAND_H

#include <stddef.h>

#include "events.h"
#include "spec.h"

struct nawid_wl;

struct nawid_wl *nawid_wl_open(const struct nawid_events *events, void *user);
void nawid_wl_close(struct nawid_wl *wl);
int nawid_wl_fd(struct nawid_wl *wl);
int nawid_wl_dispatch(struct nawid_wl *wl);
size_t nawid_wl_heads(struct nawid_wl *wl, struct nawid_output **heads);
int nawid_wl_apply(struct nawid_wl *wl, const struct nawid_output *entries, size_t count);

#endif
