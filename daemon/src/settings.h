/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NAWID_SETTINGS_H
#define NAWID_SETTINGS_H

#include <cfgd/cfgd.h>
#include <stdbool.h>
#include <stddef.h>

#define NAWID_SETTINGS_PREFIX "/org/nuros/sanar/nawi/"

struct nawid_settings {
    char *profile;
    bool automatic;
    char **items;
    size_t items_count;
};

void nawid_settings_load(cfgd_t *cfgd, struct nawid_settings *settings);
void nawid_settings_free(struct nawid_settings *settings);

#endif
