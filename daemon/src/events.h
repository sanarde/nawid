/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NAWID_EVENTS_H
#define NAWID_EVENTS_H

#include <stdbool.h>

enum nawid_apply_result {
    NAWID_APPLY_OK,
    NAWID_APPLY_FAILED,
    NAWID_APPLY_CANCELLED,
};

struct nawid_events {
    void (*changed)(void *user);
    void (*applied)(enum nawid_apply_result result, void *user);
};

#endif
