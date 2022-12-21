# Kisswm

**Kisswm** is a dead simple tiling window manager inspired a lot
by [DWM](https://dwm.suckless.org/).

It features basic ICCM/EWMH compliance, common navigation functionalities, Multi-monitor support
and multiple tiling layouts switchable per tag.

## Configuration

The configuration is solely done inside *config.h* which is a copy
of the default *kisswm.h* file.

## Installation

> make config

This creates the *config.h* file, which can be edited to customize kisswm

> make

Build kisswm with the custom *config.h* file

> doas make install

Install kisswm to */usr/local/bin/kisswm*

## Why?

I had the urge to write my own window manager and so I did.
