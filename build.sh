#!/bin/sh

if [ ! -f build/release/seshat ]; then
    meson build/release --buildtype release
fi
ninja -C build/release

if [ ! -f build/debug/seshat ]; then
    meson build/debug --buildtype debug
fi
ninja -C build/debug