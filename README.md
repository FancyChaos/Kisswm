# Kisswm

**Kisswm** is a simple tiling window manager inspired a lot
by [DWM](https://dwm.suckless.org/).

It features basic ICCM/EWMH compliance, common navigation
functionalities, Multi-monitor support and
multiple tiling layouts switchable per tag.

## Configuration

The configuration is solely done inside *config.h*, which is a copy
of the default *config.h* file from the *src/* directory.

## Installation

Use *gmake* on OpenBSD to compile kisswm.

> make config

This creates the *config.h* file inside the root directory,
which can be edited to customize kisswm

> make

Build kisswm with the custom *config.h* file

> doas make install

Install kisswm to  *$(DESTDIR)\$(PREFIX)/bin*
(most likely */usr/local/bin/kisswm*)

## Planned Features

- [ ] Add workspaces (Like virtual monitors)
- [ ] Add option to use a different statusbar than the built-in one
- [ ] Add DWM like swallowing mode
- [ ] Add full floating support (Be a dynamic window manager)
