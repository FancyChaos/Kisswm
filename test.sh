#!/bin/sh

Xephyr -br -ac -reset -screen 1920x1080 :1 &
sleep 1
export DISPLAY=:1

fwallpaper &
./kisswm
