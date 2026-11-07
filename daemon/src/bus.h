/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NAWID_BUS_H
#define NAWID_BUS_H

#include <dbus/dbus.h>
#include <stdint.h>

#include "loop.h"

struct nawid_app;

struct nawid_bus_watch {
    struct nawid_bus *bus;
    DBusWatch *watch;
    int fd;
};

struct nawid_bus {
    DBusConnection *connection;
    struct nawid_loop *loop;
    struct nawid_app *app;
};

int nawid_bus_open(struct nawid_bus *bus, struct nawid_loop *loop, struct nawid_app *app);
void nawid_bus_close(struct nawid_bus *bus);
void nawid_bus_emit_changed(struct nawid_bus *bus, const char *profile);

#endif
