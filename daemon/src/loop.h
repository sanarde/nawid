/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NAWID_LOOP_H
#define NAWID_LOOP_H

#include <cfgd/cfgd.h>
#include <stdint.h>

typedef void (*nawid_watch_fn)(int fd, uint32_t events, void *user);

struct nawid_loop {
    cfgd_t *cfgd;
    int (*watch)(struct nawid_loop *loop, int fd, uint32_t events, nawid_watch_fn fn,
                 void *user);
    void (*unwatch)(struct nawid_loop *loop, int fd);
    int (*subscribe)(struct nawid_loop *loop, const char *prefix, cfgd_changed_fn fn,
                     void *user);
    void (*unsubscribe)(struct nawid_loop *loop, int id);
};

#endif
