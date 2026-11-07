/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NAWID_X11_H
#define NAWID_X11_H

#include <stddef.h>

#include "events.h"
#include "spec.h"

struct nawid_x11;

struct nawid_x11 *nawid_x11_open(const struct nawid_events *events, void *user);
void nawid_x11_close(struct nawid_x11 *x11);
int nawid_x11_fd(struct nawid_x11 *x11);
int nawid_x11_dispatch(struct nawid_x11 *x11);
size_t nawid_x11_heads(struct nawid_x11 *x11, struct nawid_output **heads);
int nawid_x11_apply(struct nawid_x11 *x11, const struct nawid_output *entries, size_t count);

#endif
