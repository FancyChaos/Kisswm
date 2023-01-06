#!/bin/sh

set -e

which Xephyr >/dev/null 2>&1 || { echo "Missing application - Xephyr" 1>&2; exit 1; }

RES=$(xrandr | awk 'NR==2 { print $4 }')

M=make
[  $(uname) = OpenBSD ] && M=gmake

$M clean
$M

Xephyr -br -ac -reset -screen $RES :1 &
export DISPLAY=:1
sleep 2

fwallpaper &
./kisswm
