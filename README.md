# Sanar Nawid

> [!NOTE]
> **Repository Mirrors**
>
> - **git.nuros.org** ([sanarde/nawid](https://git.nuros.org/sanarde/nawid)): primary, self-hosted Forgejo instance, accounts restricted to the core team.
> - **GitHub** ([sanarde/nawid](https://github.com/sanarde/nawid)): mirror for external contributors. Issues and Pull Requests opened here are welcome and are reviewed and processed by the core team.

## About

Monitor configuration daemon of SanarDE for Wayland and X11. It applies saved layouts, called profiles, when the set of connected monitors changes and reads its settings from cfgd.

## Dependencies

- C17 compiler
- Meson (>= 1.0) and Ninja
- libdbus-1
- libcfgd, the client library of [cfgd](https://git.nuros.org/sanarde/cfgd)
- wayland-client and wayland-scanner
- libxcb with the RandR extension

## Build

```sh
meson setup build
meson compile -C build
meson test -C build
meson install -C build
```

The install step puts `nawid` and `nawi` in `bin`, the cfgd schema in `share/cfgd/schemas`, the D-Bus service file in `share/dbus-1/services` and the autostart entry in `<sysconfdir>/xdg/autostart`.

## Running

`nawid` selects Wayland when `WAYLAND_DISPLAY` is set and X11 otherwise. Use `nawid -b wayland` or `nawid -b x11` to choose. Only one instance can run per session.

- Wayland: the compositor must support `wlr-output-management`, for example labwc, sway or river. GNOME Shell and KWin do not.
- X11: RandR 1.2 or newer. The scale of an output is ignored, only the transforms `normal`, `90`, `180` and `270` are supported, and an output that is cloned onto another one is not restored after a layout change. A layout that the server rejects leaves the previous one in place.

A profile is applied when `nawid` starts, when the set of connected outputs changes, when the settings change and when `Apply` is called. A change made by another tool to the same outputs is not reverted.

## Settings

Settings are keys of the cfgd schema `org.nuros.sanar.nawi`, path `/org/nuros/sanar/nawi/`. Changes apply without a restart.

- `auto`: select a profile by the connected outputs. Default `true`.
- `profile`: name of a profile that is always applied. Empty selects by the connected outputs.
- `profiles`: list of entries, one per output of a profile.

An entry is `PROFILE/OUTPUT=WIDTHxHEIGHT[@HZ],X,Y,SCALE,TRANSFORM` or `PROFILE/OUTPUT=off`. `OUTPUT` is the name of the compositor or of RandR, for example `DP-1`. `PROFILE` has letters, digits, `.`, `_` and `-`. `TRANSFORM` is `normal`, `90`, `180`, `270`, `flipped`, `flipped-90`, `flipped-180` or `flipped-270`. Without `@HZ` the preferred mode of that size is used.

```sh
cfgd set /org/nuros/sanar/nawi/profiles 'desk/eDP-1=off;desk/DP-1=2560x1440@143.912,0,0,1,normal'
```

With `auto`, a profile is selected when it has an entry for every connected output and every output it enables is connected. The one that enables the most outputs wins, and the first one in the list wins a tie. Entries for outputs that are not connected are ignored.

## Command line

```
nawi list
nawi profiles
nawi apply [PROFILE]
nawi save PROFILE
nawi delete PROFILE
```

`list` prints the current layout. `profiles` lists the profiles and marks the applied one with `*`. `apply PROFILE` applies a profile until the settings change, `apply` without a name returns to the selection by `auto` and `profile`. `save` stores the current layout as a profile, replacing a profile of the same name, and `delete` removes one. `save` and `delete` write `profiles` through cfgd.

## D-Bus interface

Session bus, name `org.nuros.sanar.Nawi`, object `/org/nuros/sanar/nawi`, interface `org.nuros.sanar.Nawi`.

- `GetOutputs() -> a(sbiiiiids)`: name, enabled, x, y, width, height, refresh in mHz, scale, transform.
- `GetProfiles() -> as`
- `GetProfile() -> s`: applied profile, empty when none.
- `Apply(s profile)`: empty selects by `auto` and `profile`. Errors `org.nuros.sanar.Nawi.Error.NoProfile` and `org.nuros.sanar.Nawi.Error.Rejected`.
- Signal `Changed(s profile)`.

