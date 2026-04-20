#!/bin/sh
set -eu

version="${BOX64_BUNDLE_VERSION:-v0.4.0}"
outdir="$1"
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

x86_libs_url="https://github.com/ptitSeb/box64/releases/download/${version}/box64-bundle-x86-libs-${version#v}.tar.gz"
x86_pkgs_url="https://github.com/ptitSeb/box64/releases/download/${version}/box64-bundle-x86-pkgs-${version#v}.tar.gz"

download() {
    url="$1"
    dest="$2"
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL "$url" -o "$dest"
    else
        wget -qO "$dest" "$url"
    fi
}

extract_glibc_core() {
    rpm_path="$1"
    lib_dir="$2"
    rpm_tmp="$tmpdir/rpm-$(basename "$rpm_path" .rpm)"

    mkdir -p "$rpm_tmp" "$lib_dir"
    bsdtar -xf "$rpm_path" -C "$rpm_tmp"

    find "$rpm_tmp" \( -path '*/ld-linux*.so*' \
        -o -path '*/libc.so.6' \
        -o -path '*/libdl.so.2' \
        -o -path '*/libm.so.6' \
        -o -path '*/libpthread.so.0' \
        -o -path '*/librt.so.1' \
        -o -path '*/libresolv.so.2' \
        -o -path '*/libutil.so.1' \) -type f | while IFS= read -r file || [ -n "$file" ]; do
        [ -n "$file" ] || continue
        cp -f "$file" "$lib_dir/"
    done
}

mkdir -p "$outdir"
download "$x86_libs_url" "$tmpdir/x86-libs.tar.gz"
download "$x86_pkgs_url" "$tmpdir/x86-pkgs.tar.gz"

tar -xf "$tmpdir/x86-libs.tar.gz" -C "$tmpdir"
rm -rf "$outdir/box64-i386-linux-gnu" "$outdir/box64-x86_64-linux-gnu"
cp -R "$tmpdir/usr/lib/box64-x86_64-linux-gnu" "$outdir/"
mkdir -p "$outdir/box64-i386-linux-gnu"
cp -f "$tmpdir/usr/lib/box64-i386-linux-gnu/libstdc++.so.6" "$outdir/box64-i386-linux-gnu/"
cp -f "$tmpdir/usr/lib/box64-i386-linux-gnu/libgcc_s.so.1" "$outdir/box64-i386-linux-gnu/"

tar -xf "$tmpdir/x86-pkgs.tar.gz" -C "$tmpdir" \
    "./glibc-2.34-125.el9_5.8.alma.1.x86_64.rpm"

extract_glibc_core "$tmpdir/glibc-2.34-125.el9_5.8.alma.1.x86_64.rpm" "$outdir/box64-x86_64-linux-gnu"
