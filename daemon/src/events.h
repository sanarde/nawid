/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NAWID_EVENTS_H
#define NAWID_EVENTS_H

#include <stdbool.h>

struct nawid_events {
    void (*changed)(void *user);
    void (*applied)(bool ok, void *user);
};

#endif
