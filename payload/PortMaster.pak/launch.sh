#!/bin/sh
set -eu

TOOL_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PLATFORM_DIR=$(CDPATH= cd -- "$TOOL_DIR/.." && pwd)
TOOLS_DIR=$(CDPATH= cd -- "$PLATFORM_DIR/.." && pwd)
SDROOT=$(CDPATH= cd -- "$TOOLS_DIR/.." && pwd)
PLATFORM=$(basename "$PLATFORM_DIR")
TARGET_PAK="$SDROOT/Emus/$PLATFORM/PORTS.pak"
ROM_STUB="$SDROOT/Roms/Ports (PORTS)/0) Portmaster.sh"

exec "$TARGET_PAK/launch.sh" "$ROM_STUB"
