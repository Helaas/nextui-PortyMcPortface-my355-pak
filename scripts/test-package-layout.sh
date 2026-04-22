#!/bin/sh
set -eu

pakz="$1"
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

unzip -q "$pakz" -d "$tmpdir"

test -f "$tmpdir/Tools/my355/Porty McPortface.pak/launch.sh"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/porty-mcportface"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/resources/bin/7zzs"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/resources/runtime-bin/my355/pm-artwork-convert"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/resources/runtime-bin/my355/pm-sdl-compat-check"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/resources/runtime-bin/my355/pm-port-probe"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/resources/runtime-bin/my355/pm-armhf-exec-wrapper"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/resources/runtime-bin/my355/box64"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/resources/runtime-bin/my355/aarch64/libSDL2-2.0.so.0"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/resources/runtime-bin/my355/aarch64/pulse/libpulse-simple.so.0"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/resources/runtime-bin/my355/aarch64/pulse/libpulse.so.0"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/resources/runtime-bin/my355/aarch64/pulse/libpulsecommon-13.99.so"
test ! -e "$tmpdir/Tools/my355/Porty McPortface.pak/resources/runtime-bin/my355/armhf"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/resources/runtime-bin/my355/box64-i386-linux-gnu/libstdc++.so.6"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/resources/runtime-bin/my355/box64-i386-linux-gnu/libgcc_s.so.1"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/resources/runtime-bin/my355/box64-x86_64-linux-gnu/libc.so.6"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/payload/PORTS.pak/launch.sh"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/payload/PORTS.pak/files/control.txt"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/payload/PORTS.pak/files/PortMaster.txt"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/payload/PORTS.pak/files/config.py"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/payload/PORTS.pak/files/gamecontrollerdb.txt"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/payload/PORTS.pak/files/bin.tar.gz"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/payload/PORTS.pak/files/lib.tar.gz"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/payload/PORTS.pak/files/libmali-g2p0.so.1.9.0"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/payload/PORTS.pak/files/ca-certificates.crt"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/payload/PORTS.pak/files/libffi.so.7"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/payload/PortMaster.pak/launch.sh"
test -f "$tmpdir/Tools/my355/Porty McPortface.pak/payload/PortMaster.pak/pak.json"
test ! -f "$tmpdir/Tools/my355/Porty McPortface.pak/payload/PORTS.pak/bin/python3"
test ! -f "$tmpdir/Tools/my355/Porty McPortface.pak/payload/PORTS.pak/bin/pm-artwork-convert"
test ! -f "$tmpdir/Tools/my355/Porty McPortface.pak/payload/PORTS.pak/lib/libcairo.so.2"

lib_listing="$(tar -tvf "$tmpdir/Tools/my355/Porty McPortface.pak/payload/PORTS.pak/files/lib.tar.gz")"
printf '%s\n' "$lib_listing" | awk 'substr($1, 1, 1) == "l" || substr($1, 1, 1) == "h" { exit 1 }'
printf '%s\n' "$lib_listing" | grep -Eq '^-.*[[:space:]]\./libpython3\.10\.a$'
printf '%s\n' "$lib_listing" | grep -Eq '^-.*[[:space:]]\./libpython3\.10\.so$'
! printf '%s\n' "$lib_listing" | grep -Eq '[[:space:]]\./\._'
! printf '%s\n' "$lib_listing" | grep -Eq '[[:space:]]\./libpulse(\.so\.0|-simple\.so\.0)$'
! printf '%s\n' "$lib_listing" | grep -Eq '[[:space:]]\./libpulsecommon-(13\.99|14\.2|15\.0)\.so$'

docker run --rm --platform linux/arm64 -v "$tmpdir":/mnt alpine:3.20 \
    sh -lc '/mnt/Tools/my355/Porty\ McPortface.pak/resources/bin/7zzs i >/dev/null'
