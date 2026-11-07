/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "profile.h"

int main(void)
{
    char *items[] = {
        "solo/eDP-1=1920x1080,0,0,1,normal",
        "desk/eDP-1=off",
        "desk/DP-1=2560x1440@60,0,0,1,normal",
        "dual/eDP-1=1920x1080,0,0,1,normal",
        "dual/DP-1=2560x1440,1920,0,1,normal",
        "broken/DP-1=nonsense",
        "dual/DP-1=1x1,0,0,1,normal",
        "no-slash",
    };
    const char *laptop[] = {"eDP-1"};
    const char *both[] = {"eDP-1", "DP-1"};
    const char *dock[] = {"DP-1"};
    const char *other[] = {"HDMI-A-1"};
    const char *three[] = {"eDP-1", "DP-1", "HDMI-A-1"};
    struct nawid_profiles profiles;
    const struct nawid_profile *found;

    CHECK(nawid_profiles_load(&profiles, items, 8) == 3);
    CHECK(profiles.count == 3);
    CHECK_STR(profiles.items[0].name, "solo");
    CHECK_STR(profiles.items[1].name, "desk");
    CHECK_STR(profiles.items[2].name, "dual");
    CHECK(profiles.items[1].count == 2);
    CHECK(profiles.items[2].count == 2);
    CHECK(profiles.items[2].entries[1].x == 1920);

    CHECK(nawid_profiles_find(&profiles, "desk") == &profiles.items[1]);
    CHECK(nawid_profiles_find(&profiles, "none") == NULL);
    CHECK(nawid_profile_entry(&profiles.items[1], "eDP-1") != NULL);
    CHECK(nawid_profile_entry(&profiles.items[1], "DP-2") == NULL);

    found = nawid_profiles_select(&profiles, laptop, 1);
    CHECK(found && strcmp(found->name, "solo") == 0);
    found = nawid_profiles_select(&profiles, both, 2);
    CHECK(found && strcmp(found->name, "dual") == 0);
    found = nawid_profiles_select(&profiles, dock, 1);
    CHECK(found && strcmp(found->name, "desk") == 0);
    found = nawid_profiles_select(&profiles, other, 1);
    CHECK(found == NULL);
    found = nawid_profiles_select(&profiles, three, 3);
    CHECK(found == NULL);
    found = nawid_profiles_select(&profiles, both, 0);
    CHECK(found == NULL);

    nawid_profiles_free(&profiles);
    CHECK(profiles.count == 0);

    {
        char *tied[] = {
            "first/eDP-1=1920x1080,0,0,1,normal",
            "second/eDP-1=1920x1080,0,0,2,normal",
        };

        CHECK(nawid_profiles_load(&profiles, tied, 2) == 0);
        found = nawid_profiles_select(&profiles, laptop, 1);
        CHECK(found && strcmp(found->name, "second") == 0);
        nawid_profiles_free(&profiles);
    }

    CHECK(nawid_profiles_load(&profiles, items, 0) == 0);
    CHECK(nawid_profiles_select(&profiles, laptop, 1) == NULL);
    nawid_profiles_free(&profiles);

    return check_failures != 0;
}
