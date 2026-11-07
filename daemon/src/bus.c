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

static int watch_update(struct nawid_bus *bus, struct nawid_bus_watch *w, int op)
{
    struct epoll_event ev = {.events = 0, .data.ptr = &w->source};

    if (dbus_watch_get_enabled(w->watch)) {
        unsigned int flags = dbus_watch_get_flags(w->watch);

        if (flags & DBUS_WATCH_READABLE)
            ev.events |= EPOLLIN;
        if (flags & DBUS_WATCH_WRITABLE)
            ev.events |= EPOLLOUT;
    }
    return epoll_ctl(bus->epfd, op, w->fd, &ev);
}

static dbus_bool_t add_watch(DBusWatch *watch, void *data)
{
    struct nawid_bus *bus = data;
    struct nawid_bus_watch *w = calloc(1, sizeof(*w));

    if (!w)
        return FALSE;
    w->source.kind = NAWID_SRC_BUS;
    w->watch = watch;
    w->fd = fcntl(dbus_watch_get_unix_fd(watch), F_DUPFD_CLOEXEC, 0);
    if (w->fd < 0) {
        free(w);
        return FALSE;
    }
    if (watch_update(bus, w, EPOLL_CTL_ADD) != 0) {
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

    epoll_ctl(bus->epfd, EPOLL_CTL_DEL, w->fd, NULL);
    close(w->fd);
    dbus_watch_set_data(watch, NULL, NULL);
    free(w);
}

static void toggle_watch(DBusWatch *watch, void *data)
{
    watch_update(data, dbus_watch_get_data(watch), EPOLL_CTL_MOD);
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
