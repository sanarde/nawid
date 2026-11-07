/* SPDX-License-Identifier: GPL-3.0-only */
#include <cfgd/cfgd.h>
#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spec.h"

#define BUS_NAME "org.nuros.sanar.Nawi"
#define OBJECT_PATH "/org/nuros/sanar/nawi"
#define INTERFACE "org.nuros.sanar.Nawi"
#define KEY_PROFILES "/org/nuros/sanar/nawi/profiles"

static DBusMessage *call(const char *method, const char *argument)
{
    DBusError error;
    DBusConnection *connection;
    DBusMessage *message;
    DBusMessage *reply;

    dbus_error_init(&error);
    connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (!connection) {
        fprintf(stderr, "nawi: cannot connect to the session bus: %s\n", error.message);
        dbus_error_free(&error);
        return NULL;
    }
    message = dbus_message_new_method_call(BUS_NAME, OBJECT_PATH, INTERFACE, method);
    if (!message)
        return NULL;
    if (argument)
        dbus_message_append_args(message, DBUS_TYPE_STRING, &argument, DBUS_TYPE_INVALID);
    reply = dbus_connection_send_with_reply_and_block(connection, message, 10000, &error);
    dbus_message_unref(message);
    if (!reply) {
        fprintf(stderr, "nawi: nawid: %s\n", error.message);
        dbus_error_free(&error);
    }
    return reply;
}

static int read_outputs(struct nawid_output **outputs, size_t *count)
{
    DBusMessage *reply = call("GetOutputs", NULL);
    DBusMessageIter iter, array, entry;
    size_t capacity = 0;

    *outputs = NULL;
    *count = 0;
    if (!reply)
        return -1;
    if (!dbus_message_iter_init(reply, &iter) ||
        dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
        dbus_message_unref(reply);
        return -1;
    }
    dbus_message_iter_recurse(&iter, &array);
    while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_STRUCT) {
        struct nawid_output *grown;
        struct nawid_output *output;
        const char *name;
        const char *transform;
        dbus_bool_t enabled;
        int32_t values[5];
        double scale;

        if (*count == capacity) {
            capacity = capacity ? capacity * 2 : 4;
            grown = realloc(*outputs, capacity * sizeof(**outputs));
            if (!grown) {
                dbus_message_unref(reply);
                return -1;
            }
            *outputs = grown;
        }
        output = &(*outputs)[(*count)++];
        memset(output, 0, sizeof(*output));
        dbus_message_iter_recurse(&array, &entry);
        dbus_message_iter_get_basic(&entry, &name);
        dbus_message_iter_next(&entry);
        dbus_message_iter_get_basic(&entry, &enabled);
        for (int i = 0; i < 5; i++) {
            dbus_message_iter_next(&entry);
            dbus_message_iter_get_basic(&entry, &values[i]);
        }
        dbus_message_iter_next(&entry);
        dbus_message_iter_get_basic(&entry, &scale);
        dbus_message_iter_next(&entry);
        dbus_message_iter_get_basic(&entry, &transform);

        snprintf(output->name, sizeof(output->name), "%s", name);
        output->enabled = enabled;
        output->x = values[0];
        output->y = values[1];
        output->width = values[2];
        output->height = values[3];
        output->refresh = values[4];
        output->scale = scale;
        output->transform = nawid_transform_parse(transform);
        if (output->transform < 0)
            output->transform = 0;
        dbus_message_iter_next(&array);
    }
    dbus_message_unref(reply);
    return 0;
}

static int command_list(void)
{
    struct nawid_output *outputs;
    size_t count;

    if (read_outputs(&outputs, &count) != 0)
        return 1;
    for (size_t i = 0; i < count; i++) {
        const struct nawid_output *o = &outputs[i];

        if (!o->enabled)
            printf("%s off\n", o->name);
        else
            printf("%s %dx%d@%d.%03d %+d%+d scale %g transform %s\n", o->name, o->width,
                   o->height, o->refresh / 1000, o->refresh % 1000, o->x, o->y, o->scale,
                   nawid_transform_name(o->transform));
    }
    free(outputs);
    return 0;
}

static int command_profiles(void)
{
    DBusMessage *reply = call("GetProfiles", NULL);
    DBusMessage *active = call("GetProfile", NULL);
    DBusMessageIter iter, array;
    const char *current = "";

    if (!reply || !active)
        return 1;
    dbus_message_get_args(active, NULL, DBUS_TYPE_STRING, &current, DBUS_TYPE_INVALID);
    if (dbus_message_iter_init(reply, &iter)) {
        dbus_message_iter_recurse(&iter, &array);
        while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_STRING) {
            const char *name;

            dbus_message_iter_get_basic(&array, &name);
            printf("%s%s\n", name, strcmp(name, current) == 0 ? " *" : "");
            dbus_message_iter_next(&array);
        }
    }
    dbus_message_unref(reply);
    dbus_message_unref(active);
    return 0;
}

static int command_apply(const char *name)
{
    DBusMessage *reply = call("Apply", name);

    if (!reply)
        return 1;
    dbus_message_unref(reply);
    return 0;
}
