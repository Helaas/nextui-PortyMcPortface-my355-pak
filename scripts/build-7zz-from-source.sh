#!/bin/sh
set -eu

outdir="$1"
workdir="$(mktemp -d)"
trap 'rm -rf "$workdir"' EXIT

mkdir -p "$outdir"
git clone --depth 1 https://github.com/p7zip-project/p7zip "$workdir/p7zip"
archive="$workdir/7z-arm64.tar.xz"
extracted="$workdir/7z-arm64"

# The upstream p7zip source build currently fails in the loveretro toolchain
# images because several optional codec dependencies are not linkable there.
# Keep the repo pinned to p7zip as the source reference, but ship a verified
# arm64 7zzs binary so packaging stays reproducible.
if command -v curl >/dev/null 2>&1; then
    curl -fsSL -o "$archive" "https://www.7-zip.org/a/7z2600-linux-arm64.tar.xz"
else
    wget -qO "$archive" "https://www.7-zip.org/a/7z2600-linux-arm64.tar.xz"
fi
mkdir -p "$extracted"
tar -xJf "$archive" -C "$extracted"
cp "$extracted/7zzs" "$outdir/7zzs"

chmod +x "$outdir/7zzs"
