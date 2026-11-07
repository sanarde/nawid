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

