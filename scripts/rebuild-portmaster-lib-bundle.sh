#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
SPRUCE_LIB_DIR="${1:-/Volumes/Storage/GitHub/spruceOS/spruce/flip/lib}"
BUNDLE_PATH="${2:-$REPO_ROOT/payload/PORTS.pak/files/lib.tar.gz}"

if [ ! -d "$SPRUCE_LIB_DIR" ]; then
    echo "Spruce lib directory not found: $SPRUCE_LIB_DIR" >&2
    exit 1
fi

if [ ! -f "$BUNDLE_PATH" ]; then
    echo "Lib bundle not found: $BUNDLE_PATH" >&2
    exit 1
fi

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

CURRENT_DIR="$TMPDIR/current"
MERGED_DIR="$TMPDIR/merged"

mkdir -p "$CURRENT_DIR" "$MERGED_DIR"
COPYFILE_DISABLE=1 tar -xzf "$BUNDLE_PATH" -C "$CURRENT_DIR"
COPYFILE_DISABLE=1 cp -RL "$CURRENT_DIR"/. "$MERGED_DIR"/
COPYFILE_DISABLE=1 cp -RL "$SPRUCE_LIB_DIR"/. "$MERGED_DIR"/

find "$MERGED_DIR" \( -name '._*' -o -name '.DS_Store' \) -delete
rm -rf "$MERGED_DIR"/__MACOSX
rm -f \
    "$MERGED_DIR"/libpulse.so.0 \
    "$MERGED_DIR"/libpulse-simple.so.0 \
    "$MERGED_DIR"/libpulsecommon-13.99.so \
    "$MERGED_DIR"/libpulsecommon-14.2.so \
    "$MERGED_DIR"/libpulsecommon-15.0.so

if find "$MERGED_DIR" -type l | grep -q .; then
    echo "Refusing to write lib bundle with symlinks" >&2
    exit 1
fi

COPYFILE_DISABLE=1 tar -czf "$BUNDLE_PATH" -C "$MERGED_DIR" .
