#!/bin/sh

set -e

M=make
[  $(uname) = OpenBSD ] && M=gmake

$M clean
$M

Xephyr -br -ac -reset -screen 1920x1080 :1 &
export DISPLAY=:1
sleep 2

fwallpaper &
./kisswm
