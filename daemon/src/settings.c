/* SPDX-License-Identifier: GPL-3.0-only */
#include "settings.h"

#include <stdlib.h>
#include <string.h>

#define KEY_PROFILE NAWID_SETTINGS_PREFIX "profile"
#define KEY_AUTO NAWID_SETTINGS_PREFIX "auto"
#define KEY_PROFILES NAWID_SETTINGS_PREFIX "profiles"

void nawid_settings_load(cfgd_t *cfgd, struct nawid_settings *settings)
{
    char *profile;
    bool flag;

    nawid_settings_free(settings);
    settings->profile = strdup("");
    settings->automatic = true;

    profile = cfgd_get_string(cfgd, KEY_PROFILE);
    if (profile) {
        free(settings->profile);
        settings->profile = profile;
    }
    if (cfgd_get_bool(cfgd, KEY_AUTO, &flag))
        settings->automatic = flag;
    settings->items = cfgd_get_strv(cfgd, KEY_PROFILES, &settings->items_count);
}

void nawid_settings_free(struct nawid_settings *settings)
{
    free(settings->profile);
    if (settings->items)
        cfgd_strv_free(settings->items, settings->items_count);
    memset(settings, 0, sizeof(*settings));
}
