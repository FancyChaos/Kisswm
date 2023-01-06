# Kisswm

**Kisswm** is a simple tiling window manager inspired by [DWM](https://dwm.suckless.org/).

## Features

- Basic ICCM/EWMH compliance
- Built-in statusbar
  - Displays the root window name (can be used with dwmblocks)
- Common navigation functionalities
- Multiple tiling layouts switchable per tag
  - Stack
  - Master stack
  - Side by side
- Basic multi-monitor support
- Window borders and custom border colors

## Configuration

The configuration is solely done inside *config.h*, which is a copy
of the default *config.h* file from the *src/* directory.

> make config

- Copies the default *config.h* file into the root directory
for customization.

## Installation

Use *gmake* on OpenBSD to compile kisswm.

> make

- Build kisswm using customized *config.h* file

> doas make install

- Install kisswm to  *$(DESTDIR)\$(PREFIX)/bin*
(most likely */usr/local/bin/kisswm*)

## Keybindings

The default **modkey** is bind to **Mod4Mask**,
which is the *super key*/*Windows key* on the keyboard.

| Shortcut                      | Action                               |
|-------------------------------|--------------------------------------|
| `MOD` + `Return`              | Spawn *term* (default **st**)        |
| `MOD` + `d`                   | Spawn *dmenucmd* (default **dmenu**) |
| `MOD` + `Shift` + `l`         | Spawn *lock* (default **fxlock**)    |
| `MOD` + `m`                   | Change layout                        |
| `MOD` + `q`                   | Close current client                 |
| `MOD` + `f`                   | Fullscreen current client            |
| `MOD` + `k`/`right`           | Focus next client                    |
| `MOD` + `j`/`left`            | Focus previous client                |
| `MOD` + `1-9`                 | Switch to tag *n*                    |
| `MOD` + `Shift` + `1-9`       | Move focused client to tag *n*       |
| `MOD` + `Shift` + `x`/`right` | Follow client to next tag            |
| `MOD` + `Shift` + `y`/`left`  | Follow client to previous tag        |
| `MOD` + `CTRL` + `k`/`right`  | Focus next tag                       |
| `MOD` + `CTRL` + `j`/`left`   | Focus previous tag                   |
| `MOD` + `period`              | Focus next monitor                   |
| `MOD` + `comma`               | Focus previous monitor               |
| `MOD` + `Shift` + `period`    | Move client to next monitor          |
| `MOD` + `Shift` + `comma`     | Move client to previous monitor      |
| `MOD` + `Shift` + `k`         | Move client up in stack              |
| `MOD` + `Shift` + `j`         | Move client down in stack            |
| `MOD` + `l`                   | Increase master area (layout)        |
| `MOD` + `h`                   | Decrease master area (layout)        |

## Roadmap

- [ ] Add DWM like swallowing mode
- [ ] Add workspaces (Like virtual monitors)
- [ ] Add full floating support (Be a dynamic window manager)
- [ ] Add option to use a different statusbar than the built-in one
