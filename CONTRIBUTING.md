# Contributing

- Build and run the tests before sending a change: `meson setup build && meson test -C build`.
- For changes in C code also run the tests with sanitizers: `meson setup build-san -Db_sanitize=address,undefined && meson test -C build-san`.
- The daemon and the command line tool have no automated tests. Check changes to them by hand on Wayland (a compositor with `wlr-output-management`) and on X11, with more than one output when the change touches positions or profile selection.
- Keep each change limited to one area.
- Report bugs with the version, the steps to reproduce, and the observed output.

Source layout:

- `core`: profile entries, their text form, mode choice and profile selection, without any display or bus code.
- `daemon`: `nawid`, the Wayland and X11 backends, settings and the D-Bus service.
- `cli`: the `nawi` tool.
- `data`: cfgd schema, D-Bus service file and autostart entry.
- `protocols`: Wayland protocol XML that the system does not provide.
