#!/bin/sh
set -eu

PAK_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
EMU_DIR="$PAK_DIR/PortMaster"
ROM_PATH="${1:-$PAK_DIR/Portmaster.sh}"
ROM_DIR=$(CDPATH= cd -- "$(dirname -- "$ROM_PATH")" && pwd)
ROM_NAME=$(basename "$ROM_PATH")
SDCARD_ROOT=$(CDPATH= cd -- "$PAK_DIR/../../.." && pwd)
TEMP_DATA_DIR="$SDCARD_ROOT/.ports_temp"
REAL_PORTS_DIR="$ROM_DIR/.ports"
MOUNTED_PORTS_DIR="$TEMP_DATA_DIR/ports"

if [ -n "${SHARED_USERDATA_PATH:-}" ]; then
    PMI_HOME_ROOT="$SHARED_USERDATA_PATH/PORTS-portmaster"
elif [ -n "${USERDATA_PATH:-}" ]; then
    PMI_HOME_ROOT="$USERDATA_PATH/PORTS-portmaster"
elif [ -n "${PLATFORM:-}" ]; then
    PMI_HOME_ROOT="$SDCARD_ROOT/.userdata/$PLATFORM/PORTS-portmaster"
else
    PMI_HOME_ROOT="$SDCARD_ROOT/.userdata/PORTS-portmaster"
fi

HOME="$PMI_HOME_ROOT"
XDG_DATA_HOME="$HOME/.local/share"
XDG_CONFIG_HOME="$HOME/.config"
XDG_CACHE_HOME="$HOME/.cache"

if [ -n "${LOGS_PATH:-}" ]; then
    PMI_LOG_FILE="$LOGS_PATH/PORTS.txt"
elif [ -n "${USERDATA_PATH:-}" ]; then
    PMI_LOG_FILE="$USERDATA_PATH/logs/PORTS.txt"
elif [ -n "${PLATFORM:-}" ]; then
    PMI_LOG_FILE="$SDCARD_ROOT/.userdata/$PLATFORM/logs/PORTS.txt"
else
    PMI_LOG_FILE="$EMU_DIR/log.txt"
fi

mkdir -p "$HOME" "$XDG_DATA_HOME" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" \
    "$REAL_PORTS_DIR" "$MOUNTED_PORTS_DIR"
mkdir -p "$(dirname "$PMI_LOG_FILE")"

export PAK_DIR
export EMU_DIR
export PM_RUNTIME_ROOT="$EMU_DIR"
export ROM_PATH
export ROM_DIR
export ROM_NAME
export TEMP_DATA_DIR
export HOME="$HOME"
export XDG_DATA_HOME="$HOME/.local/share"
export XDG_CONFIG_HOME="$HOME/.config"
export XDG_CACHE_HOME="$HOME/.cache"
export HM_TOOLS_DIR="$PAK_DIR"
export HM_PORTS_DIR="$MOUNTED_PORTS_DIR"
export HM_SCRIPTS_DIR="$MOUNTED_PORTS_DIR"
export PMI_ARTWORK_CONVERTER="$PAK_DIR/bin/pm-artwork-convert"
export PM_SCRIPTNAME="$ROM_NAME"
export PMI_ROM_ROOT="$ROM_DIR"
export PMI_LOG_FILE
export PMI_HOME_ROOT
export SDL_GAMECONTROLLERCONFIG_FILE="$PM_RUNTIME_ROOT/gamecontrollerdb.txt"
if [ -n "${PATH:-}" ]; then
    export PATH="$EMU_DIR:$PAK_DIR/bin:/usr/bin:/bin:/usr/sbin:/sbin:$PATH"
else
    export PATH="$EMU_DIR:$PAK_DIR/bin:/usr/bin:/bin:/usr/sbin:/sbin"
fi

if [ -n "${LD_LIBRARY_PATH:-}" ]; then
    export LD_LIBRARY_PATH="/usr/trimui/lib:$EMU_DIR/runtimes/love_11.5/libs.aarch64:$PAK_DIR/lib:$LD_LIBRARY_PATH"
else
    export LD_LIBRARY_PATH="/usr/trimui/lib:$EMU_DIR/runtimes/love_11.5/libs.aarch64:$PAK_DIR/lib"
fi

if [ -f "$PAK_DIR/files/ca-certificates.crt" ]; then
    export SSL_CERT_FILE="$PAK_DIR/files/ca-certificates.crt"
fi

export PYSDL2_DLL_PATH="/usr/lib:/usr/trimui/lib:$PAK_DIR/lib"

cd "$PAK_DIR"
exec "$PAK_DIR/Portmaster.sh" "$@"
