#!/bin/sh

set -e

make clean
make

Xephyr -br -ac -reset -screen 1920x1080 :1 &
export DISPLAY=:1
sleep 2

fwallpaper &
./kisswm
