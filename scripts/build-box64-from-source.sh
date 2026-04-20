#!/bin/sh
set -eu

outdir="$1"
workdir="$(mktemp -d)"
trap 'rm -rf "$workdir"' EXIT

mkdir -p "$outdir"

git clone --depth 1 https://github.com/ptitSeb/box64 "$workdir/box64"
mkdir -p "$workdir/box64/build"

cd "$workdir/box64/build"
cmake .. \
    -DARM64=1 \
    -DARM_DYNAREC=ON \
    -DBOX32=OFF \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_C_COMPILER=aarch64-nextui-linux-gnu-gcc \
    -DCMAKE_CXX_COMPILER=aarch64-nextui-linux-gnu-g++ \
    -DCMAKE_ASM_COMPILER=aarch64-nextui-linux-gnu-gcc

make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)" box64
aarch64-nextui-linux-gnu-strip box64

cp box64 "$outdir/box64"
chmod +x "$outdir/box64"
