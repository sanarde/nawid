/* SPDX-License-Identifier: GPL-3.0-only */
#include "bus.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "app.h"

#define BUS_NAME "org.nuros.sanar.Nawi"
#define OBJECT_PATH "/org/nuros/sanar/nawi"
#define INTERFACE "org.nuros.sanar.Nawi"
#define ERROR_NO_PROFILE "org.nuros.sanar.Nawi.Error.NoProfile"
#define ERROR_REJECTED "org.nuros.sanar.Nawi.Error.Rejected"

static const char introspection_xml[] =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection "
    "1.0//EN\" "
    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\"" INTERFACE "\">\n"
    "    <method name=\"GetOutputs\">\n"
    "      <arg type=\"a(sbiiiiids)\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"GetProfiles\">\n"
    "      <arg type=\"as\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"GetProfile\">\n"
    "      <arg type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"Apply\">\n"
    "      <arg type=\"s\" name=\"profile\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <signal name=\"Changed\">\n"
    "      <arg type=\"s\" name=\"profile\"/>\n"
    "    </signal>\n"
    "  </interface>\n"
    "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
    "    <method name=\"Introspect\">\n"
    "      <arg type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "</node>\n";

static void watch_ready(int fd, uint32_t events, void *user);

static uint32_t watch_events(DBusWatch *watch)
{
    uint32_t events = 0;

    if (dbus_watch_get_enabled(watch)) {
        unsigned int flags = dbus_watch_get_flags(watch);

        if (flags & DBUS_WATCH_READABLE)
            events |= EPOLLIN;
        if (flags & DBUS_WATCH_WRITABLE)
            events |= EPOLLOUT;
    }
    return events;
}

static int watch_arm(struct nawid_bus_watch *w)
{
    uint32_t events = watch_events(w->watch);

    w->bus->loop->unwatch(w->bus->loop, w->fd);
    if (events == 0)
        return 0;
    return w->bus->loop->watch(w->bus->loop, w->fd, events, watch_ready, w);
}

static dbus_bool_t add_watch(DBusWatch *watch, void *data)
{
    struct nawid_bus_watch *w = calloc(1, sizeof(*w));

    if (!w)
        return FALSE;
    w->bus = data;
    w->watch = watch;
    w->fd = fcntl(dbus_watch_get_unix_fd(watch), F_DUPFD_CLOEXEC, 0);
    if (w->fd < 0) {
        free(w);
        return FALSE;
    }
    if (watch_arm(w) != 0) {
        close(w->fd);
        free(w);
        return FALSE;
    }
    dbus_watch_set_data(watch, w, NULL);
    return TRUE;
}

static void remove_watch(DBusWatch *watch, void *data)
{
    struct nawid_bus *bus = data;
    struct nawid_bus_watch *w = dbus_watch_get_data(watch);

    bus->loop->unwatch(bus->loop, w->fd);
    close(w->fd);
    dbus_watch_set_data(watch, NULL, NULL);
    free(w);
}

static void toggle_watch(DBusWatch *watch, void *data)
{
    (void)data;
    watch_arm(dbus_watch_get_data(watch));
}

static void append_outputs(DBusMessage *message, struct nawid_app *app)
{
    DBusMessageIter iter, array, entry;
    struct nawid_output *heads = NULL;
    size_t count = nawid_app_heads(app, &heads);

    dbus_message_iter_init_append(message, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "(sbiiiiids)", &array);
    for (size_t i = 0; i < count; i++) {
        const struct nawid_output *h = &heads[i];
        const char *name = h->name;
        const char *transform = nawid_transform_name(h->transform);
        dbus_bool_t enabled = h->enabled;
        int32_t values[5] = {h->x, h->y, h->width, h->height, h->refresh};

        if (!transform)
            transform = "normal";
        dbus_message_iter_open_container(&array, DBUS_TYPE_STRUCT, NULL, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &name);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_BOOLEAN, &enabled);
        for (int j = 0; j < 5; j++)
            dbus_message_iter_append_basic(&entry, DBUS_TYPE_INT32, &values[j]);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_DOUBLE, &h->scale);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &transform);
        dbus_message_iter_close_container(&array, &entry);
    }
    dbus_message_iter_close_container(&iter, &array);
    free(heads);
}

static void append_profiles(DBusMessage *message, const struct nawid_app *app)
{
    DBusMessageIter iter, array;

    dbus_message_iter_init_append(message, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &array);
    for (size_t i = 0; i < app->profiles.count; i++) {
        const char *name = app->profiles.items[i].name;

        dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &name);
    }
    dbus_message_iter_close_container(&iter, &array);
}

static DBusMessage *apply_reply(DBusMessage *message, struct nawid_app *app)
{
    const char *name = NULL;
    int rc;

    if (!dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID))
        return dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS, "expected a string");
    rc = nawid_app_apply(app, name);
    if (rc == -2)
        return dbus_message_new_error(message, ERROR_NO_PROFILE, "no such profile");
    if (rc != 0)
        return dbus_message_new_error(message, ERROR_REJECTED, "the layout was not applied");
    return dbus_message_new_method_return(message);
}

static DBusHandlerResult handle_message(DBusConnection *connection, DBusMessage *message,
                                        void *data)
{
    struct nawid_bus *bus = data;
    DBusMessage *reply;

    if (dbus_message_is_method_call(message, INTERFACE, "GetOutputs")) {
        reply = dbus_message_new_method_return(message);
        if (reply)
            append_outputs(reply, bus->app);
    } else if (dbus_message_is_method_call(message, INTERFACE, "GetProfiles")) {
        reply = dbus_message_new_method_return(message);
        if (reply)
            append_profiles(reply, bus->app);
    } else if (dbus_message_is_method_call(message, INTERFACE, "GetProfile")) {
        const char *active = bus->app->active;

        reply = dbus_message_new_method_return(message);
        if (reply)
            dbus_message_append_args(reply, DBUS_TYPE_STRING, &active, DBUS_TYPE_INVALID);
    } else if (dbus_message_is_method_call(message, INTERFACE, "Apply")) {
        reply = apply_reply(message, bus->app);
    } else if (dbus_message_is_method_call(message, "org.freedesktop.DBus.Introspectable",
                                           "Introspect")) {
        const char *xml = introspection_xml;

        reply = dbus_message_new_method_return(message);
        if (reply)
            dbus_message_append_args(reply, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID);
    } else {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (!reply)
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    dbus_connection_send(connection, reply, NULL);
    dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static const DBusObjectPathVTable vtable = {
    .message_function = handle_message,
};

int nawid_bus_open(struct nawid_bus *bus, struct nawid_loop *loop, struct nawid_app *app)
{
    DBusError error;

    memset(bus, 0, sizeof(*bus));
    bus->loop = loop;
    bus->app = app;

    dbus_error_init(&error);
    bus->connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
    if (!bus->connection) {
        fprintf(stderr, "nawid: session bus: %s\n", error.message);
        dbus_error_free(&error);
        return -1;
    }
    dbus_connection_set_exit_on_disconnect(bus->connection, FALSE);

    if (dbus_bus_request_name(bus->connection, BUS_NAME, DBUS_NAME_FLAG_DO_NOT_QUEUE,
                              &error) != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        fprintf(stderr, "nawid: cannot own %s%s%s\n", BUS_NAME,
                dbus_error_is_set(&error) ? ": " : "",
                dbus_error_is_set(&error) ? error.message : "");
        dbus_error_free(&error);
        goto fail;
    }
    if (!dbus_connection_register_object_path(bus->connection, OBJECT_PATH, &vtable, bus) ||
        !dbus_connection_set_watch_functions(bus->connection, add_watch, remove_watch,
                                             toggle_watch, bus, NULL)) {
        fprintf(stderr, "nawid: cannot register D-Bus object\n");
        goto fail;
    }
    return 0;

fail:
    dbus_connection_close(bus->connection);
    dbus_connection_unref(bus->connection);
    bus->connection = NULL;
    return -1;
}

void nawid_bus_close(struct nawid_bus *bus)
{
    if (!bus->connection)
        return;
    dbus_connection_close(bus->connection);
    dbus_connection_unref(bus->connection);
    bus->connection = NULL;
}

static void watch_ready(int fd, uint32_t events, void *user)
{
    struct nawid_bus_watch *watch = user;
    struct nawid_bus *bus = watch->bus;
    unsigned int flags = 0;

    (void)fd;
    if (!bus->connection || !dbus_watch_get_enabled(watch->watch))
        return;
    if (events & EPOLLIN)
        flags |= DBUS_WATCH_READABLE;
    if (events & EPOLLOUT)
        flags |= DBUS_WATCH_WRITABLE;
    if (events & EPOLLERR)
        flags |= DBUS_WATCH_ERROR;
    if (events & EPOLLHUP)
        flags |= DBUS_WATCH_HANGUP;

    dbus_watch_handle(watch->watch, flags);
    while (dbus_connection_get_dispatch_status(bus->connection) == DBUS_DISPATCH_DATA_REMAINS)
        dbus_connection_dispatch(bus->connection);

    if (!dbus_connection_get_is_connected(bus->connection)) {
        fprintf(stderr, "nawid: disconnected from session bus\n");
        nawid_bus_close(bus);
    }
}

void nawid_bus_emit_changed(struct nawid_bus *bus, const char *profile)
{
    DBusMessage *signal;

    if (!bus->connection)
        return;
    signal = dbus_message_new_signal(OBJECT_PATH, INTERFACE, "Changed");
    if (!signal)
        return;
    dbus_message_append_args(signal, DBUS_TYPE_STRING, &profile, DBUS_TYPE_INVALID);
    dbus_connection_send(bus->connection, signal, NULL);
    dbus_message_unref(signal);
}
