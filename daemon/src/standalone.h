/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NAWID_STANDALONE_H
#define NAWID_STANDALONE_H

#include <stdbool.h>

#include "loop.h"

struct standalone_watch;

struct nawid_standalone {
    struct nawid_loop loop;
    int epfd;
    struct standalone_watch *watches;
};

int nawid_standalone_open(struct nawid_standalone *standalone);
void nawid_standalone_close(struct nawid_standalone *standalone);
int nawid_standalone_run(struct nawid_standalone *standalone, bool (*done)(void *user),
                         void *user);

#endif
