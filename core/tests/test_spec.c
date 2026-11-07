/* SPDX-License-Identifier: GPL-3.0-only */
#include "check.h"
#include "spec.h"

static int parse(const char *item, char *profile, struct nawid_output *output)
{
    return nawid_item_parse(item, profile, NAWID_NAME_MAX, output);
}

int main(void)
{
    char profile[NAWID_NAME_MAX];
    char text[128];
    struct nawid_output o;
    struct nawid_output p;

    CHECK(parse("desk/DP-1=2560x1440@143.912,1920,-100,1.5,90", profile, &o) == 0);
    CHECK_STR(profile, "desk");
    CHECK_STR(o.name, "DP-1");
    CHECK(o.enabled && o.width == 2560 && o.height == 1440 && o.refresh == 143912);
    CHECK(o.x == 1920 && o.y == -100 && o.scale == 1.5 && o.transform == 1);

    CHECK(parse("desk/eDP-1=1920x1080,0,0,1,normal", profile, &o) == 0);
    CHECK(o.refresh == 0 && o.transform == 0);

    CHECK(parse("desk/HDMI-A-1=off", profile, &o) == 0);
    CHECK(!o.enabled);
    CHECK_STR(o.name, "HDMI-A-1");

    CHECK(parse("desk", profile, &o) != 0);
    CHECK(parse("desk/DP-1", profile, &o) != 0);
    CHECK(parse("/DP-1=off", profile, &o) != 0);
    CHECK(parse("desk/=off", profile, &o) != 0);
    CHECK(parse("de sk/DP-1=off", profile, &o) != 0);
    CHECK(parse("desk/DP-1=on", profile, &o) != 0);
    CHECK(parse("desk/DP-1=0x1080,0,0,1,normal", profile, &o) != 0);
    CHECK(parse("desk/DP-1=1920x1080,0,0,0,normal", profile, &o) != 0);
    CHECK(parse("desk/DP-1=1920x1080,0,0,1,upside", profile, &o) != 0);
    CHECK(parse("desk/DP-1=1920x1080,0,0,1", profile, &o) != 0);
    CHECK(parse("desk/DP-1=1920x1080@0,0,0,1,normal", profile, &o) != 0);
    CHECK(parse("desk/DP-1=1920x1080,a,0,1,normal", profile, &o) != 0);
    CHECK(parse("desk/DP-1=1920x1080,0,0,nan,normal", profile, &o) != 0);

    CHECK(parse("desk/DP-1=2560x1440@143.912,1920,-100,1.5,90", profile, &o) == 0);
    CHECK(nawid_item_format(profile, &o, text, sizeof(text)) == 0);
    CHECK_STR(text, "desk/DP-1=2560x1440@143.912,1920,-100,1.5,90");
    CHECK(nawid_item_format(profile, &o, text, 10) != 0);
    strcpy(p.name, "a=b");
    p.enabled = false;
    CHECK(nawid_item_format(profile, &p, text, sizeof(text)) != 0);
    strcpy(p.name, "a/b");
    CHECK(nawid_item_format(profile, &p, text, sizeof(text)) != 0);
    CHECK(parse("desk/HDMI-A-1=off", profile, &o) == 0);
    CHECK(nawid_item_format(profile, &o, text, sizeof(text)) == 0);
    CHECK_STR(text, "desk/HDMI-A-1=off");

    CHECK_STR(nawid_transform_name(7), "flipped-270");
    CHECK(nawid_transform_name(8) == NULL);
    CHECK(nawid_transform_parse("flipped-90") == 5);
    CHECK(nawid_transform_parse("") == -1);

    CHECK(nawid_profile_name_valid("work-2.1_a"));
    CHECK(!nawid_profile_name_valid(""));
    CHECK(!nawid_profile_name_valid("a/b"));
    CHECK(!nawid_profile_name_valid("a=b"));
    CHECK(!nawid_profile_name_valid(NULL));

    CHECK(parse("a/DP-1=1920x1080@60.000,0,0,1,normal", profile, &o) == 0);
    p = o;
    p.refresh = 60005;
    CHECK(nawid_output_equal(&o, &p, true));
    p.refresh = 59940;
    CHECK(!nawid_output_equal(&o, &p, true));
    p = o;
    p.refresh = 0;
    CHECK(nawid_output_equal(&o, &p, true));
    p.scale = 2.0;
    CHECK(!nawid_output_equal(&o, &p, true));
    CHECK(nawid_output_equal(&o, &p, false));
    p = o;
    p.x = 1;
    CHECK(!nawid_output_equal(&o, &p, true));
    p = o;
    p.transform = 2;
    CHECK(!nawid_output_equal(&o, &p, true));
    p = o;
    p.enabled = false;
    CHECK(!nawid_output_equal(&o, &p, true));
    o.enabled = false;
    p.width = 1;
    CHECK(nawid_output_equal(&o, &p, true));

    {
        struct nawid_mode modes[] = {
            {1920, 1080, 60000, false}, {1920, 1080, 144000, false}, {1920, 1080, 59940, false},
            {1280, 720, 60000, true},   {2560, 1440, 60000, false}, {2560, 1440, 75000, true},
        };
        struct nawid_output want = {.width = 1920, .height = 1080};

        CHECK(nawid_mode_pick(modes, 6, &want) == 1);
        want.refresh = 59900;
        CHECK(nawid_mode_pick(modes, 6, &want) == 2);
        want.refresh = 60000;
        CHECK(nawid_mode_pick(modes, 6, &want) == 0);
        want.refresh = 30000;
        CHECK(nawid_mode_pick(modes, 6, &want) == -1);
        want.width = 2560;
        want.height = 1440;
        want.refresh = 0;
        CHECK(nawid_mode_pick(modes, 6, &want) == 5);
        want.width = 800;
        want.height = 600;
        CHECK(nawid_mode_pick(modes, 6, &want) == -1);
        CHECK(nawid_mode_pick(modes, 0, &want) == -1);
    }

    return check_failures != 0;
}
