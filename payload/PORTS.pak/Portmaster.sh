#!/bin/bash
set -eu

: "${PAK_DIR:=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)}"
: "${EMU_DIR:=$PAK_DIR/PortMaster}"
: "${PM_RUNTIME_ROOT:=$EMU_DIR}"
: "${ROM_PATH:=${1:-$PAK_DIR/Portmaster.sh}}"
: "${ROM_DIR:=$(CDPATH= cd -- "$(dirname -- "$ROM_PATH")" && pwd)}"
: "${ROM_NAME:=$(basename "$ROM_PATH")}"
: "${TEMP_DATA_DIR:=$PAK_DIR/.ports_temp}"

REAL_PORTS_DIR="$ROM_DIR/.ports"
MOUNTED_PORTS_DIR="$TEMP_DATA_DIR/ports"
LOG_FILE="${PMI_LOG_FILE:-$PM_RUNTIME_ROOT/log.txt}"
PORTS_BIND_MOUNTED=0
LIBMALI_BIND_MOUNTED=0
SPRUCE_ARMHF_ROOTFS_MOUNT="$TEMP_DATA_DIR/spruceflip-rootfs"
JOY_TYPE_NODE="${PMI_JOY_TYPE_NODE:-/sys/class/miyooio_chr_dev/joy_type}"
JOY_TYPE_OVERRIDE_VALUE="-1"
JOY_TYPE_ORIGINAL=""
JOY_TYPE_RESTORE_NEEDED=0
PMI_POWER_LID_HELPER_PID=""
PMI_POWER_LID_HELPER_PIDFILE="/tmp/pmi-power-lid-watch.pid"
PMI_LAUNCH_INDEX_READY=0
PMI_LAUNCHER_METADATA_SOURCE=""
PMI_LAUNCHER_METADATA_PORT_DIR=""
PMI_LAUNCHER_METADATA_ARMHF=0
PMI_LAUNCHER_METADATA_AARCH64=0
PMI_LAUNCHER_METADATA_NEEDS_FLIP_LIBMALI=0
PMI_AARCH64_PROBE_PORT_DIR=""
PMI_AARCH64_PROBE_CACHE_PATH=""
PMI_AARCH64_PROBE_HAS_BUNDLED_NATIVE_GL=0
PMI_AARCH64_PROBE_NEEDS_SYSTEM_GL=0

mkdir -p "$PM_RUNTIME_ROOT"
mkdir -p "$(dirname "$LOG_FILE")"
rm -f "$LOG_FILE"
exec >>"$LOG_FILE" 2>&1

echo "PortMaster launcher starting"
echo "HOME=${HOME:-}"
echo "XDG_DATA_HOME=${XDG_DATA_HOME:-}"
echo "HM_TOOLS_DIR=${HM_TOOLS_DIR:-}"
echo "HM_PORTS_DIR=${HM_PORTS_DIR:-}"
echo "HM_SCRIPTS_DIR=${HM_SCRIPTS_DIR:-}"
echo "PM_RUNTIME_ROOT=${PM_RUNTIME_ROOT:-}"

cleanup() {
    stop_power_lid_helper
    restore_flip_joy_type
    if mount | grep -q "on $SPRUCE_ARMHF_ROOTFS_MOUNT "; then
        umount "$SPRUCE_ARMHF_ROOTFS_MOUNT" >/dev/null 2>&1 || true
    fi
    if [ "$LIBMALI_BIND_MOUNTED" = "1" ]; then
        umount /lib/libmali.so.1.9.0 >/dev/null 2>&1 || true
        umount /usr/lib/libmali.so.1.9.0 >/dev/null 2>&1 || true
    fi
    if [ "$PORTS_BIND_MOUNTED" = "1" ]; then
        umount "$MOUNTED_PORTS_DIR" >/dev/null 2>&1 || true
    fi
}

power_lid_helper_path() {
    printf '%s/bin/pm-power-lid-watch\n' "$PAK_DIR"
}

stop_power_lid_helper() {
    local pid="${PMI_POWER_LID_HELPER_PID:-}"
    local pidfile_pid=""

    if [ -f "$PMI_POWER_LID_HELPER_PIDFILE" ]; then
        pidfile_pid=$(cat "$PMI_POWER_LID_HELPER_PIDFILE" 2>/dev/null || true)
    fi
    if [ -z "$pid" ]; then
        pid="$pidfile_pid"
    fi
    if [ -z "$pid" ]; then
        rm -f "$PMI_POWER_LID_HELPER_PIDFILE"
        return 0
    fi

    if kill -0 "$pid" >/dev/null 2>&1; then
        echo "PMI_DIAG power_lid_helper_stop=$pid"
        kill "$pid" >/dev/null 2>&1 || true
        wait "$pid" >/dev/null 2>&1 || true
    fi

    if [ -n "$pidfile_pid" ] && [ "$pidfile_pid" = "$pid" ]; then
        rm -f "$PMI_POWER_LID_HELPER_PIDFILE"
    fi
    PMI_POWER_LID_HELPER_PID=""
}

start_power_lid_helper() {
    local helper
    local pid
    local existing_pid=""

    helper=$(power_lid_helper_path)
    if [ ! -x "$helper" ]; then
        echo "PMI_WARN power_lid_helper_missing=$helper"
        return 0
    fi

    if [ -n "${PMI_POWER_LID_HELPER_PID:-}" ] && kill -0 "$PMI_POWER_LID_HELPER_PID" >/dev/null 2>&1; then
        echo "PMI_DIAG power_lid_helper_running=$PMI_POWER_LID_HELPER_PID"
        return 0
    fi

    if [ -f "$PMI_POWER_LID_HELPER_PIDFILE" ]; then
        existing_pid=$(cat "$PMI_POWER_LID_HELPER_PIDFILE" 2>/dev/null || true)
        if [ -n "$existing_pid" ] && kill -0 "$existing_pid" >/dev/null 2>&1; then
            echo "PMI_DIAG power_lid_helper_replacing=$existing_pid"
            kill "$existing_pid" >/dev/null 2>&1 || true
            wait "$existing_pid" >/dev/null 2>&1 || true
        fi
        rm -f "$PMI_POWER_LID_HELPER_PIDFILE"
    fi

    "$helper" &
    pid=$!
    PMI_POWER_LID_HELPER_PID="$pid"
    printf '%s\n' "$pid" >"$PMI_POWER_LID_HELPER_PIDFILE"
    echo "PMI_DIAG power_lid_helper_started=$pid"
}

read_joy_type_value() {
    local raw_value

    [ -e "$JOY_TYPE_NODE" ] || return 1
    raw_value=$(cat "$JOY_TYPE_NODE" 2>/dev/null || true)
    [ -n "$raw_value" ] || return 1
    printf '%s\n' "${raw_value%% *}"
}

prepare_flip_joy_type_for_port() {
    local current_value

    [ -e "$JOY_TYPE_NODE" ] || return 0
    if [ ! -w "$JOY_TYPE_NODE" ]; then
        echo "PMI_WARN joy_type_node_not_writable=$JOY_TYPE_NODE"
        return 0
    fi
    if ! current_value=$(read_joy_type_value); then
        echo "PMI_WARN joy_type_read_failed=$JOY_TYPE_NODE"
        return 0
    fi

    JOY_TYPE_ORIGINAL="$current_value"
    echo "PMI_DIAG joy_type_original=$JOY_TYPE_ORIGINAL"
    if [ "$JOY_TYPE_ORIGINAL" = "$JOY_TYPE_OVERRIDE_VALUE" ]; then
        return 0
    fi

    if printf '%s\n' "$JOY_TYPE_OVERRIDE_VALUE" >"$JOY_TYPE_NODE" 2>/dev/null; then
        JOY_TYPE_RESTORE_NEEDED=1
        echo "PMI_DIAG joy_type_override=$JOY_TYPE_OVERRIDE_VALUE"
    else
        echo "PMI_WARN joy_type_override_failed=$JOY_TYPE_NODE"
    fi
}

restore_flip_joy_type() {
    if [ "$JOY_TYPE_RESTORE_NEEDED" != "1" ]; then
        return 0
    fi

    JOY_TYPE_RESTORE_NEEDED=0
    if [ -z "$JOY_TYPE_ORIGINAL" ]; then
        echo "PMI_WARN joy_type_restore_missing_original"
        return 0
    fi
    if [ ! -e "$JOY_TYPE_NODE" ]; then
        echo "PMI_WARN joy_type_restore_missing_node=$JOY_TYPE_NODE"
        return 0
    fi

    if printf '%s\n' "$JOY_TYPE_ORIGINAL" >"$JOY_TYPE_NODE" 2>/dev/null; then
        echo "PMI_DIAG joy_type_restored=$JOY_TYPE_ORIGINAL"
    else
        echo "PMI_WARN joy_type_restore_failed=$JOY_TYPE_NODE"
    fi
}

mount_ports_dir() {
    mkdir -p "$REAL_PORTS_DIR" "$MOUNTED_PORTS_DIR"
    if mount | grep -q "on $MOUNTED_PORTS_DIR "; then
        return 0
    fi
    if mount -o bind "$REAL_PORTS_DIR" "$MOUNTED_PORTS_DIR"; then
        PORTS_BIND_MOUNTED=1
        export HM_PORTS_DIR="$MOUNTED_PORTS_DIR"
        export HM_SCRIPTS_DIR="$MOUNTED_PORTS_DIR"
    else
        export HM_PORTS_DIR="$REAL_PORTS_DIR"
        export HM_SCRIPTS_DIR="$REAL_PORTS_DIR"
    fi
}

ensure_runtime_config() {
    mkdir -p "$EMU_DIR/config"
    if [ ! -f "$EMU_DIR/config/config.json" ]; then
        cp -f "$PAK_DIR/files/config.json" "$EMU_DIR/config/config.json"
    fi
}

overlay_sync_stamp_path() {
    printf '%s/.pmi-overlay-sync-v1.stamp\n' "$PM_RUNTIME_ROOT"
}

launcher_index_path() {
    printf '%s/.pmi-launch-index-v1.tsv\n' "$REAL_PORTS_DIR"
}

global_port_repairs_stamp_path() {
    printf '%s/.pmi-global-repairs-v1.stamp\n' "$REAL_PORTS_DIR"
}

touch_stamp_file() {
    local stamp_path="$1"

    mkdir -p "$(dirname "$stamp_path")"
    : >"$stamp_path"
}

ensure_exec_file() {
    local file_path="$1"

    [ -f "$file_path" ] || return 0
    [ -x "$file_path" ] || chmod +x "$file_path" 2>/dev/null || true
}

sync_overlay_file() {
    local src="$1"
    local dst="$2"

    [ -f "$src" ] || return 0
    mkdir -p "$(dirname "$dst")"
    cp -f "$src" "$dst"
}

sync_overlay_file_if_needed() {
    local src="$1"
    local dst="$2"

    [ -f "$src" ] || return 0
    mkdir -p "$(dirname "$dst")"
    if [ ! -f "$dst" ] || ! cmp -s "$src" "$dst" 2>/dev/null; then
        cp -f "$src" "$dst"
    fi
}

sync_overlay_tree() {
    local src_dir="$1"
    local dst_dir="$2"

    [ -d "$src_dir" ] || return 0
    mkdir -p "$dst_dir"
    cp -Rf "$src_dir"/. "$dst_dir"/
}

overlay_file_is_stale() {
    local src="$1"
    local dst="$2"
    local stamp_path="$3"

    [ -f "$src" ] || return 1
    [ -f "$stamp_path" ] || return 0
    [ -f "$dst" ] || return 0
    if [ "$src" -nt "$stamp_path" ]; then
        return 0
    fi
    if ! cmp -s "$src" "$dst" 2>/dev/null; then
        return 0
    fi
    return 1
}

runtime_overlay_is_stale() {
    local xdg_pm_dir="$1"
    local stamp_path

    stamp_path=$(overlay_sync_stamp_path)
    [ -f "$stamp_path" ] || return 0

    overlay_file_is_stale "$PAK_DIR/files/control.txt" "$PM_RUNTIME_ROOT/control.txt" "$stamp_path" && return 0
    overlay_file_is_stale "$PAK_DIR/files/control.txt" "$PM_RUNTIME_ROOT/miyoo/control.txt" "$stamp_path" && return 0
    overlay_file_is_stale "$PAK_DIR/files/PortMaster.txt" "$PM_RUNTIME_ROOT/PortMaster.txt" "$stamp_path" && return 0
    overlay_file_is_stale "$PAK_DIR/files/PortMaster.txt" "$PM_RUNTIME_ROOT/PortMaster.sh" "$stamp_path" && return 0
    overlay_file_is_stale "$PAK_DIR/files/PortMaster.txt" "$PM_RUNTIME_ROOT/miyoo/PortMaster.txt" "$stamp_path" && return 0
    overlay_file_is_stale "$PAK_DIR/files/config.py" "$PM_RUNTIME_ROOT/pylibs/harbourmaster/config.py" "$stamp_path" && return 0
    overlay_file_is_stale "$PAK_DIR/files/gamecontrollerdb.txt" "$PM_RUNTIME_ROOT/gamecontrollerdb.txt" "$stamp_path" && return 0
    overlay_file_is_stale "$PAK_DIR/files/control.txt" "$xdg_pm_dir/control.txt" "$stamp_path" && return 0
    overlay_file_is_stale "$PAK_DIR/files/gamecontrollerdb.txt" "$xdg_pm_dir/gamecontrollerdb.txt" "$stamp_path" && return 0

    [ -x "$PM_RUNTIME_ROOT/PortMaster.txt" ] || return 0
    [ -x "$PM_RUNTIME_ROOT/PortMaster.sh" ] || return 0
    [ -x "$PM_RUNTIME_ROOT/miyoo/PortMaster.txt" ] || return 0
    return 1
}

ensure_runtime_parity_overlay() {
    local xdg_pm_dir="$XDG_DATA_HOME/PortMaster"
    local stamp_path

    mkdir -p "$xdg_pm_dir"
    stamp_path=$(overlay_sync_stamp_path)
    if runtime_overlay_is_stale "$xdg_pm_dir"; then
        sync_overlay_file_if_needed "$PAK_DIR/files/control.txt" "$PM_RUNTIME_ROOT/control.txt"
        sync_overlay_file_if_needed "$PAK_DIR/files/control.txt" "$PM_RUNTIME_ROOT/miyoo/control.txt"
        sync_overlay_file_if_needed "$PAK_DIR/files/PortMaster.txt" "$PM_RUNTIME_ROOT/PortMaster.txt"
        sync_overlay_file_if_needed "$PAK_DIR/files/PortMaster.txt" "$PM_RUNTIME_ROOT/PortMaster.sh"
        sync_overlay_file_if_needed "$PAK_DIR/files/PortMaster.txt" "$PM_RUNTIME_ROOT/miyoo/PortMaster.txt"
        sync_overlay_file_if_needed "$PAK_DIR/files/config.py" "$PM_RUNTIME_ROOT/pylibs/harbourmaster/config.py"
        sync_overlay_file_if_needed "$PAK_DIR/files/gamecontrollerdb.txt" "$PM_RUNTIME_ROOT/gamecontrollerdb.txt"
        sync_overlay_file_if_needed "$PAK_DIR/files/control.txt" "$xdg_pm_dir/control.txt"
        sync_overlay_file_if_needed "$PAK_DIR/files/gamecontrollerdb.txt" "$xdg_pm_dir/gamecontrollerdb.txt"
        touch_stamp_file "$stamp_path"
        echo "PMI_DIAG overlay_sync_refresh=$stamp_path"
    else
        echo "PMI_DIAG overlay_sync_cached=$stamp_path"
    fi
    ensure_exec_file "$PM_RUNTIME_ROOT/PortMaster.txt"
    ensure_exec_file "$PM_RUNTIME_ROOT/PortMaster.sh"
    ensure_exec_file "$PM_RUNTIME_ROOT/miyoo/PortMaster.txt"

    echo "PMI_DIAG overlay_runtime_root=$PM_RUNTIME_ROOT"
    echo "PMI_DIAG overlay_xdg_root=$xdg_pm_dir"
    echo "PMI_DIAG overlay_controller_db=$PM_RUNTIME_ROOT/gamecontrollerdb.txt"
}

process_runtime_autoinstall_dir() {
    local autoinstall_dir="$1"
    local file_name

    [ -d "$autoinstall_dir" ] || return 0
    mkdir -p "$PM_RUNTIME_ROOT/libs"

    if [ -f "$autoinstall_dir/runtimes.zip" ]; then
        echo "PMI_DIAG autoinstall_runtimes_zip=$autoinstall_dir/runtimes.zip"
        unzip -oq "$autoinstall_dir/runtimes.zip" -d "$PM_RUNTIME_ROOT/libs"
        rm -f "$autoinstall_dir/runtimes.zip"
    fi

    for file_name in "$autoinstall_dir"/*.squashfs; do
        [ -f "$file_name" ] || continue
        echo "PMI_DIAG autoinstall_squashfs=$(basename "$file_name")"
        mv -f "$file_name" "$PM_RUNTIME_ROOT/libs/"
    done
}

process_runtime_autoinstall() {
    process_runtime_autoinstall_dir "$PM_RUNTIME_ROOT/autoinstall"
    process_runtime_autoinstall_dir "$XDG_DATA_HOME/PortMaster/autoinstall"
}

sanitize_runtime_lib_dir() {
    [ -d "$PAK_DIR/lib" ] || return 0

    find "$PAK_DIR/lib" -maxdepth 1 \( -name '._*' -o -name '.DS_Store' \) -exec rm -f {} +
    rm -rf "$PAK_DIR/lib/__MACOSX"
    rm -f \
        "$PAK_DIR/lib/libpulse.so.0" \
        "$PAK_DIR/lib/libpulse-simple.so.0" \
        "$PAK_DIR/lib/libpulsecommon-13.99.so" \
        "$PAK_DIR/lib/libpulsecommon-14.2.so" \
        "$PAK_DIR/lib/libpulsecommon-15.0.so"
}

unpack_tar() {
    local archive_path="$1"
    local dest_dir="$2"

    [ -f "$archive_path" ] || return 0
    mkdir -p "$dest_dir"
    case "$archive_path" in
        *.tar.gz|*.tgz)
            if ! gunzip -c "$archive_path" | tar -xf - -C "$dest_dir"; then
                return 1
            fi
            ;;
        *)
            if ! tar -xf "$archive_path" -C "$dest_dir"; then
                return 1
            fi
            ;;
    esac
    if [ ! -e "$dest_dir" ] || [ -z "$(ls -A "$dest_dir" 2>/dev/null)" ]; then
        return 1
    fi
    rm -f "$archive_path"
}

ensure_runtime_bootstrap() {
    local lib_bootstrap_stamp="$PAK_DIR/lib/.portmaster-lib-bundle.stamp"
    local needs_runtime_refresh=0

    if [ ! -x "$PAK_DIR/bin/python3" ]; then
        if ! unpack_tar "$PAK_DIR/files/bin.tar.gz" "$PAK_DIR/bin"; then
            echo "PMI_ERROR failed to unpack runtime bin bundle"
            return 1
        fi
    fi
    if [ ! -f "$lib_bootstrap_stamp" ]; then
        rm -f "$lib_bootstrap_stamp"
        if [ -f "$PAK_DIR/files/lib.tar.gz" ]; then
            if ! unpack_tar "$PAK_DIR/files/lib.tar.gz" "$PAK_DIR/lib"; then
                echo "PMI_ERROR failed to unpack runtime lib bundle"
                return 1
            fi
        fi
        if [ ! -d "$PAK_DIR/lib/python3.11" ] || [ ! -f "$PAK_DIR/lib/libtheoradec.so.1" ]; then
            echo "PMI_ERROR runtime lib bundle incomplete after bootstrap"
            return 1
        fi
        needs_runtime_refresh=1
    fi
    if [ ! -f "$lib_bootstrap_stamp" ] || [ "$PAK_DIR/Portmaster.sh" -nt "$lib_bootstrap_stamp" ]; then
        needs_runtime_refresh=1
    fi
    if [ "$needs_runtime_refresh" = "1" ]; then
        sanitize_runtime_lib_dir
        if ! command -v pkill >/dev/null 2>&1; then
            cat >"$PAK_DIR/bin/pkill" <<'EOF'
#!/bin/sh
set -eu

signal_opt=""
pattern=""
self_pid="$$"

if [ "${1:-}" = "-f" ] && [ $# -ge 2 ]; then
    pattern="$2"
elif [ "${1:-}" != "" ] && [ "${2:-}" = "-f" ] && [ $# -ge 3 ]; then
    signal_opt="$1"
    pattern="$3"
fi

if [ -n "$pattern" ]; then
    self_pid="$$"
    if ! command -v pgrep >/dev/null 2>&1; then
        exit 0
    fi

    pgrep -f "$pattern" | while IFS= read -r pid || [ -n "$pid" ]; do
        [ -n "$pid" ] || continue
        [ "$pid" = "$self_pid" ] && continue
        if [ -n "$signal_opt" ]; then
            kill "$signal_opt" "$pid" >/dev/null 2>&1 || true
        else
            kill "$pid" >/dev/null 2>&1 || true
        fi
    done
    exit 0
fi

if command -v killall >/dev/null 2>&1; then
    killall "$@" >/dev/null 2>&1 || true
fi
exit 0
EOF
            chmod +x "$PAK_DIR/bin/pkill"
        fi
        touch_stamp_file "$lib_bootstrap_stamp"
        echo "PMI_DIAG runtime_bootstrap_refresh=$lib_bootstrap_stamp"
    else
        echo "PMI_DIAG runtime_bootstrap_cached=$lib_bootstrap_stamp"
    fi
    if [ -f "$PAK_DIR/files/libffi.so.7" ] && [ ! -f "$PAK_DIR/lib/libffi.so.7" ]; then
        mkdir -p "$PAK_DIR/lib"
        cp -f "$PAK_DIR/files/libffi.so.7" "$PAK_DIR/lib/libffi.so.7"
    fi
}

unpack_pylibs() {
    if [ -f "$PM_RUNTIME_ROOT/pylibs.zip" ]; then
        unzip -oq "$PM_RUNTIME_ROOT/pylibs.zip" -d "$PM_RUNTIME_ROOT"
        rm -f "$PM_RUNTIME_ROOT/pylibs.zip" "$PM_RUNTIME_ROOT/pylibs.zip.md5"
    fi
}

find_shell_scripts() {
    find "$1" -type f \
        \( -name "*.sh" -o -name "*.bash" -o -name "*.src" -o -name "*.txt" -o ! -name "*.*" \) \
        | while IFS= read -r file || [ -n "$file" ]; do
            [ -z "$file" ] && continue
            if head -n 1 "$file" 2>/dev/null | grep -Eq '^#!.*(sh|bash)'; then
                echo "$file"
            fi
        done
}

rewrite_script_file() {
    local file="$1"

    if grep -q '^#!/bin/bash' "$file" 2>/dev/null; then
        sed -i '1s|^#!/bin/bash$|#!/usr/bin/env bash|' "$file"
    fi

    sed -i "s|controlfolder=\"\\\$XDG_DATA_HOME/PortMaster\"|controlfolder=\"$PM_RUNTIME_ROOT\"|g" "$file"
    sed -i "s|controlfolder=\"\\\$EMU_DIR\"|controlfolder=\"$PM_RUNTIME_ROOT\"|g" "$file"
    sed -i "s|/roms/ports/PortMaster|$PM_RUNTIME_ROOT|g" "$file"
    sed -i "s|/mnt/SDCARD/Apps/PortMaster/PortMaster|$PM_RUNTIME_ROOT|g" "$file"
    sed -i "s|/mnt/sdcard/MIYOO_EX/PortMaster|$PM_RUNTIME_ROOT|g" "$file"
    sed -i "s|/mnt/SDCARD/MIYOO_EX/PortMaster|$PM_RUNTIME_ROOT|g" "$file"
    sed -i "s|/mnt/sdcard/Roms/.portmaster/PortMaster|$PM_RUNTIME_ROOT|g" "$file"
    sed -i "s|/mnt/SDCARD/Roms/.portmaster/PortMaster|$PM_RUNTIME_ROOT|g" "$file"
    sed -i "s|/mnt/SDCARD/Data/ports|$REAL_PORTS_DIR|g" "$file"
    sed -i "s|/mnt/SDCARD/Roms/PORTS|$ROM_DIR|g" "$file"
    sed -i 's|$ESUDO systemctl restart oga_events &$|if command -v systemctl >/dev/null 2>\&1; then $ESUDO systemctl restart oga_events \& fi|g' "$file"
}

heal_installed_script_file() {
    local file="$1"

    [ -f "$file" ] || return 0
    if grep -q '^#!/bin/bash' "$file" 2>/dev/null; then
        sed -i '1s|^#!/bin/bash$|#!/usr/bin/env bash|' "$file"
    fi
    sed -i "s|controlfolder=\"\\\$XDG_DATA_HOME/PortMaster\"|controlfolder=\"$PM_RUNTIME_ROOT\"|g" "$file"
    sed -i "s|controlfolder=\"\\\$EMU_DIR\"|controlfolder=\"$PM_RUNTIME_ROOT\"|g" "$file"
}

heal_installed_port_launchers() {
    local source_script

    [ -d "$REAL_PORTS_DIR" ] || return 0
    for source_script in "$REAL_PORTS_DIR"/*.sh; do
        [ -f "$source_script" ] || continue
        heal_installed_script_file "$source_script"
    done
}

stage_launch_script() {
    local source_script="$1"
    local staged_dir="$TEMP_DATA_DIR/launchers"
    local staged_id
    local staged_script

    staged_id=$(printf '%s' "$source_script" | cksum | awk '{print $1}')
    staged_script="$staged_dir/$(basename "$source_script").$staged_id"

    mkdir -p "$staged_dir"
    if [ -f "$staged_script" ] &&
        [ ! "$source_script" -nt "$staged_script" ] &&
        [ ! "$PAK_DIR/Portmaster.sh" -nt "$staged_script" ] &&
        [ ! "$PAK_DIR/files/control.txt" -nt "$staged_script" ]; then
        echo "PMI_DIAG staged_launch_reused=$staged_script" >&2
    else
        cp -f "$source_script" "$staged_script"
        rewrite_script_file "$staged_script"
        chmod +x "$staged_script" 2>/dev/null || true
        echo "PMI_DIAG staged_launch_refreshed=$staged_script" >&2
    fi
    printf '%s\n' "$staged_script"
}

rewrite_scripts_in_tree() {
    local search_path="$1"

    [ -d "$search_path" ] || return 0
    find_shell_scripts "$search_path" | while IFS= read -r file || [ -n "$file" ]; do
        [ -z "$file" ] && continue
        rewrite_script_file "$file"
        chmod +x "$file" 2>/dev/null || true
    done
}

port_has_arch() {
    local port_json="$1"
    local wanted_arch="$2"
    local compact

    [ -f "$port_json" ] || return 1
    compact=$(tr '\n' ' ' < "$port_json" 2>/dev/null || true)
    printf '%s' "$compact" | grep -Eq "\"arch\"[[:space:]]*:[[:space:]]*\\[[^]]*\"$wanted_arch\""
}

refresh_launcher_index() {
    local index_path
    local tmp_index
    local port_json
    local shell_name
    local launcher_path
    local port_dir
    local has_armhf
    local has_aarch64
    local needs_flip_libmali

    mkdir -p "$REAL_PORTS_DIR"
    index_path=$(launcher_index_path)
    tmp_index="${index_path}.tmp.$$"
    : >"$tmp_index"

    for port_json in "$REAL_PORTS_DIR"/*/port.json; do
        [ -f "$port_json" ] || continue
        shell_name=$(port_shell_from_json "$port_json")
        [ -n "$shell_name" ] || continue
        launcher_path="$REAL_PORTS_DIR/$shell_name"
        [ -f "$launcher_path" ] || continue
        port_dir=$(dirname "$port_json")
        has_armhf=0
        has_aarch64=0
        needs_flip_libmali=0
        port_has_arch "$port_json" "armhf" && has_armhf=1
        port_has_arch "$port_json" "aarch64" && has_aarch64=1
        grep -q 'sbc_4_3_rcv12' "$launcher_path" 2>/dev/null && needs_flip_libmali=1
        printf '%s\t%s\t%s\t%s\t%s\n' \
            "$launcher_path" "$port_dir" "$has_armhf" "$has_aarch64" "$needs_flip_libmali" >>"$tmp_index"
    done

    for launcher_path in "$REAL_PORTS_DIR"/*.sh; do
        [ -f "$launcher_path" ] || continue
        if grep -Fq "$(printf '%s\t' "$launcher_path")" "$tmp_index" 2>/dev/null; then
            continue
        fi
        needs_flip_libmali=0
        grep -q 'sbc_4_3_rcv12' "$launcher_path" 2>/dev/null && needs_flip_libmali=1
        printf '%s\t\t0\t0\t%s\n' "$launcher_path" "$needs_flip_libmali" >>"$tmp_index"
    done

    mv -f "$tmp_index" "$index_path"
    PMI_LAUNCH_INDEX_READY=1
    PMI_LAUNCHER_METADATA_SOURCE=""
    echo "PMI_DIAG launcher_index_refresh=$index_path"
}

launcher_index_is_stale() {
    local index_path="$1"
    local launcher_path

    [ -f "$index_path" ] || return 0
    if [ "$PAK_DIR/Portmaster.sh" -nt "$index_path" ]; then
        return 0
    fi
    for launcher_path in "$REAL_PORTS_DIR"/*.sh; do
        [ -f "$launcher_path" ] || continue
        if [ "$launcher_path" -nt "$index_path" ]; then
            return 0
        fi
    done
    if find "$REAL_PORTS_DIR" -mindepth 2 -maxdepth 2 -type f -name 'port.json' -newer "$index_path" | grep -q .; then
        return 0
    fi
    return 1
}

ensure_launcher_index() {
    local index_path

    index_path=$(launcher_index_path)
    if [ "$PMI_LAUNCH_INDEX_READY" = "1" ] && [ -f "$index_path" ]; then
        return 0
    fi
    if launcher_index_is_stale "$index_path"; then
        refresh_launcher_index
    else
        PMI_LAUNCH_INDEX_READY=1
        echo "PMI_DIAG launcher_index_cached=$index_path"
    fi
}

lookup_launcher_metadata() {
    local launcher_path="$1"
    local index_path
    local entry_path
    local entry_port_dir
    local entry_armhf
    local entry_aarch64
    local entry_needs_flip_libmali
    local refreshed_once=0

    [ -f "$launcher_path" ] || return 1
    if [ "$PMI_LAUNCHER_METADATA_SOURCE" = "$launcher_path" ]; then
        return 0
    fi

    ensure_launcher_index || return 1
    index_path=$(launcher_index_path)
    while :; do
        while IFS="$(printf '\t')" read -r entry_path entry_port_dir entry_armhf entry_aarch64 entry_needs_flip_libmali || [ -n "${entry_path:-}" ]; do
            [ -n "${entry_path:-}" ] || continue
            if [ "$entry_path" != "$launcher_path" ]; then
                continue
            fi
            PMI_LAUNCHER_METADATA_SOURCE="$entry_path"
            PMI_LAUNCHER_METADATA_PORT_DIR="$entry_port_dir"
            PMI_LAUNCHER_METADATA_ARMHF="${entry_armhf:-0}"
            PMI_LAUNCHER_METADATA_AARCH64="${entry_aarch64:-0}"
            PMI_LAUNCHER_METADATA_NEEDS_FLIP_LIBMALI="${entry_needs_flip_libmali:-0}"
            return 0
        done < "$index_path"
        if [ "$refreshed_once" = "1" ]; then
            break
        fi
        refreshed_once=1
        refresh_launcher_index || return 1
    done

    PMI_LAUNCHER_METADATA_SOURCE="$launcher_path"
    PMI_LAUNCHER_METADATA_PORT_DIR=""
    PMI_LAUNCHER_METADATA_ARMHF=0
    PMI_LAUNCHER_METADATA_AARCH64=0
    PMI_LAUNCHER_METADATA_NEEDS_FLIP_LIBMALI=0
    return 0
}

global_port_repairs_are_stale() {
    local stamp_path="$1"

    [ -f "$stamp_path" ] || return 0
    if launcher_index_is_stale "$(launcher_index_path)"; then
        return 0
    fi
    if [ "$PAK_DIR/Portmaster.sh" -nt "$stamp_path" ]; then
        return 0
    fi
    if find "$REAL_PORTS_DIR" -type f \( -path '*/box86/box86' -o -path '*/box64/box64' \) -newer "$stamp_path" | grep -q .; then
        return 0
    fi
    return 1
}

ensure_global_port_repairs() {
    local stamp_path

    stamp_path=$(global_port_repairs_stamp_path)
    if global_port_repairs_are_stale "$stamp_path"; then
        heal_installed_port_launchers
        apply_port_arch_rewrites
        refresh_box_runtime_wrappers "$REAL_PORTS_DIR"
        refresh_launcher_index
        touch_stamp_file "$stamp_path"
        echo "PMI_DIAG global_port_repairs_refresh=$stamp_path"
    else
        ensure_launcher_index || return 1
        echo "PMI_DIAG global_port_repairs_cached=$stamp_path"
    fi
}

launcher_requires_armhf() {
    local launcher_path="$1"
    launcher_armhf_port_dir "$launcher_path" >/dev/null 2>&1
}

launcher_armhf_port_dir() {
    local launcher_path="$1"

    lookup_launcher_metadata "$launcher_path" || return 1
    [ "$PMI_LAUNCHER_METADATA_ARMHF" = "1" ] || return 1
    [ -n "$PMI_LAUNCHER_METADATA_PORT_DIR" ] || return 1
    printf '%s\n' "$PMI_LAUNCHER_METADATA_PORT_DIR"
}

rewrite_armhf_port_launcher() {
    local launcher_path="$1"

    [ -f "$launcher_path" ] || return 0

    sed -i 's|cd "$BASEDIR/64"|cd "$BASEDIR/32"|g' "$launcher_path"
    sed -i 's|BOX64_LOG|BOX86_LOG|g' "$launcher_path"
    sed -i 's|BOX64_DYNAREC|BOX86_DYNAREC|g' "$launcher_path"
    sed -i 's|BOX64_NOBANNER|BOX86_NOBANNER|g' "$launcher_path"
    sed -i 's|BOX64_PATH|BOX86_PATH|g' "$launcher_path"
    sed -i 's|BOX64_LD_LIBRARY_PATH|BOX86_LD_LIBRARY_PATH|g' "$launcher_path"
    sed -i 's|^export BOX86_PATH=.*$|export BOX86_PATH="$BASEDIR/32/lib${BOX86_PATH:+:$BOX86_PATH}"|' "$launcher_path"
    sed -i 's|^export BOX86_LD_LIBRARY_PATH=.*$|export BOX86_LD_LIBRARY_PATH="$BASEDIR/32/lib${BOX86_LD_LIBRARY_PATH:+:$BOX86_LD_LIBRARY_PATH}"|' "$launcher_path"
    sed -i 's|^\(export LD_LIBRARY_PATH="\$GAMEDIR/gl4es:\)\$BASEDIR/64/lib:\(.*\)$|\1$BASEDIR/32/lib:\2|' "$launcher_path"
    sed -i 's|/box64/box64|/box86/box86|g' "$launcher_path"
    sed -i 's|"$PAK_DIR/bin/box64"|"$GAMEDIR/box86/box86"|g' "$launcher_path"
    sed -i 's|"$PAK_DIR/bin/box86"|"$GAMEDIR/box86/box86"|g' "$launcher_path"
}

apply_port_arch_rewrites() {
    local port_json shell_script launcher_path

    for port_json in "$REAL_PORTS_DIR"/*/port.json; do
        [ -f "$port_json" ] || continue

        shell_script=$(port_shell_from_json "$port_json")
        [ -n "$shell_script" ] || continue

        launcher_path="$REAL_PORTS_DIR/$shell_script"
        [ -f "$launcher_path" ] || continue

        if port_has_arch "$port_json" "armhf" || grep -q 'PORT_32BIT=Y' "$launcher_path" 2>/dev/null; then
            rewrite_armhf_port_launcher "$launcher_path"
        fi
    done
}

bind_flip_libmali() {
    local source_lib="$PAK_DIR/files/libmali-g2p0.so.1.9.0"

    if [ ! -f "$source_lib" ]; then
        echo "PMI_WARN flip_libmali_missing=$source_lib"
        return 0
    fi

    if [ -f /lib/libmali.so.1.9.0 ] && ! mount | grep -q "on /lib/libmali.so.1.9.0 "; then
        mount -o bind "$source_lib" /lib/libmali.so.1.9.0
        LIBMALI_BIND_MOUNTED=1
        echo "PMI_DIAG flip_libmali_bound=/lib/libmali.so.1.9.0"
    fi

    if [ -f /usr/lib/libmali.so.1.9.0 ] && ! mount | grep -q "on /usr/lib/libmali.so.1.9.0 "; then
        mount -o bind "$source_lib" /usr/lib/libmali.so.1.9.0
        LIBMALI_BIND_MOUNTED=1
        echo "PMI_DIAG flip_libmali_bound=/usr/lib/libmali.so.1.9.0"
    fi

    return 0
}

launcher_requires_flip_libmali() {
    local launcher_path="$1"

    lookup_launcher_metadata "$launcher_path" || return 1
    [ "$PMI_LAUNCHER_METADATA_NEEDS_FLIP_LIBMALI" = "1" ]
}

port_probe_helper_path() {
    printf '%s/bin/pm-port-probe\n' "$PAK_DIR"
}

port_probe_cache_path() {
    local port_dir="$1"

    printf '%s/.pmi-port-probe-v2.tsv\n' "$port_dir"
}

armhf_probe_cache_path() {
    local port_dir="$1"

    printf '%s/.pmi-armhf-port-probe-v1.tsv\n' "$port_dir"
}

cleanup_legacy_port_probe_artifacts() {
    local port_dir="$1"

    [ -d "$port_dir" ] || return 0
    find "$port_dir" -type f \
        \( -name '*.pmi-aarch64-sdl-compat*' -o -name '*.pmi-system-gl-windowing*' -o -name '*.pmi-aarch64-elf64-candidates*' \) \
        -exec rm -f {} + 2>/dev/null || true
}

port_probe_cache_is_stale() {
    local port_dir="$1"
    local cache_path="$2"
    local helper
    local cache_source="${PAK_DIR:-}/Portmaster.sh"

    [ -f "$cache_path" ] || return 0

    if find "$port_dir" -maxdepth 2 -type f -newer "$cache_path" | grep -q .; then
        return 0
    fi

    if [ -d "$port_dir/lib" ] && find "$port_dir/lib" -maxdepth 2 -type f \
        \( -name 'libEGL.so*' -o -name 'libGLES*.so*' -o -name 'libGL.so*' \
           -o -name 'libgbm.so*' -o -name 'libdrm.so*' -o -name 'libmali.so*' \) \
        -newer "$cache_path" | grep -q .; then
        return 0
    fi

    helper=$(port_probe_helper_path)
    if [ -e "$helper" ] && [ "$helper" -nt "$cache_path" ]; then
        return 0
    fi

    if [ -e "$cache_source" ] && [ "$cache_source" -nt "$cache_path" ]; then
        return 0
    fi

    return 1
}

refresh_port_probe_cache() {
    local port_dir="$1"
    local cache_path
    local helper
    local tmp_cache

    [ -d "$port_dir" ] || return 1

    cache_path=$(port_probe_cache_path "$port_dir")
    if ! port_probe_cache_is_stale "$port_dir" "$cache_path"; then
        return 0
    fi

    cleanup_legacy_port_probe_artifacts "$port_dir"

    helper=$(port_probe_helper_path)
    if [ ! -x "$helper" ]; then
        if [ "${PMI_PORT_PROBE_HELPER_WARNED:-0}" != "1" ]; then
            PMI_PORT_PROBE_HELPER_WARNED=1
            echo "PMI_WARN port_probe_helper_missing=$helper"
        fi
        return 1
    fi

    tmp_cache="${cache_path}.tmp.$$"
    if "$helper" scan-aarch64-launch-port "$port_dir" >"$tmp_cache"; then
        mv -f "$tmp_cache" "$cache_path"
        return 0
    fi

    rm -f "$tmp_cache" 2>/dev/null || true
    return 1
}

armhf_probe_cache_is_stale() {
    local port_dir="$1"
    local cache_path="$2"
    local helper
    local cache_source="${PAK_DIR:-}/Portmaster.sh"

    [ -f "$cache_path" ] || return 0

    if find "$port_dir" -maxdepth 2 -type f -newer "$cache_path" | grep -q .; then
        return 0
    fi

    helper=$(port_probe_helper_path)
    if [ -e "$helper" ] && [ "$helper" -nt "$cache_path" ]; then
        return 0
    fi

    if [ -e "$cache_source" ] && [ "$cache_source" -nt "$cache_path" ]; then
        return 0
    fi

    return 1
}

refresh_armhf_probe_cache() {
    local port_dir="$1"
    local cache_path
    local helper
    local tmp_cache

    [ -d "$port_dir" ] || return 1

    cache_path=$(armhf_probe_cache_path "$port_dir")
    if ! armhf_probe_cache_is_stale "$port_dir" "$cache_path"; then
        return 0
    fi

    helper=$(port_probe_helper_path)
    if [ ! -x "$helper" ]; then
        if [ "${PMI_ARMHF_PROBE_HELPER_WARNED:-0}" != "1" ]; then
            PMI_ARMHF_PROBE_HELPER_WARNED=1
            echo "PMI_WARN armhf_probe_helper_missing=$helper"
        fi
        return 1
    fi

    tmp_cache="${cache_path}.tmp.$$"
    if "$helper" scan-armhf-launch-port "$port_dir" >"$tmp_cache"; then
        mv -f "$tmp_cache" "$cache_path"
        return 0
    fi

    rm -f "$tmp_cache" 2>/dev/null || true
    return 1
}

launcher_aarch64_port_dir() {
    local launcher_path="$1"

    [ -f "$launcher_path" ] || return 1
    lookup_launcher_metadata "$launcher_path" || return 1
    [ "$PMI_LAUNCHER_METADATA_AARCH64" = "1" ] || return 1
    [ -n "$PMI_LAUNCHER_METADATA_PORT_DIR" ] || return 1
    printf '%s\n' "$PMI_LAUNCHER_METADATA_PORT_DIR"
}

write_box64_wrapper() {
    local wrapper_path="$1"

    cat >"$wrapper_path" <<EOF
#!/bin/sh
set -eu
exec "$PAK_DIR/bin/box64" "\$@"
EOF
    chmod +x "$wrapper_path" 2>/dev/null || true
}

write_box86_compat_wrapper() {
    local wrapper_path="$1"
    local wrapper_dir original_path

    wrapper_dir=$(dirname "$wrapper_path")
    original_path="$wrapper_dir/box86.original"

    if [ ! -f "$original_path" ]; then
        mv "$wrapper_path" "$original_path"
    fi

    cat >"$wrapper_path" <<'EOF'
#!/bin/sh
set -eu

SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PAK_DIR="${PAK_DIR:-}"
TEMP_DATA_DIR="${TEMP_DATA_DIR:-$PAK_DIR/.ports_temp}"
SPRUCE_ARMHF_ROOTFS_MOUNT="$TEMP_DATA_DIR/spruceflip-rootfs"
SPRUCE_ARMHF_ROOTFS_IMAGE="$PAK_DIR/runtime/armhf/miyoo355_rootfs_32.img"
SPRUCE_ARMHF_ROOTFS_PARTAA="$PAK_DIR/runtime/armhf/miyoo355_rootfs_32.img_partaa"
SPRUCE_ARMHF_ROOTFS_PARTAB="$PAK_DIR/runtime/armhf/miyoo355_rootfs_32.img_partab"
SPRUCE_ARMHF_ROOTFS_PARTAC="$PAK_DIR/runtime/armhf/miyoo355_rootfs_32.img_partac"
X86_LIB_PATH="$PAK_DIR/lib/box64-i386-linux-gnu"
REAL_BOX86="$SELF_DIR/box86.original"

if [ -n "$PAK_DIR" ] && { [ -f "$SPRUCE_ARMHF_ROOTFS_IMAGE" ] || [ -f "$SPRUCE_ARMHF_ROOTFS_PARTAA" ]; }; then
    mkdir -p "$SPRUCE_ARMHF_ROOTFS_MOUNT"
    if [ ! -f "$SPRUCE_ARMHF_ROOTFS_IMAGE" ] && [ -f "$SPRUCE_ARMHF_ROOTFS_PARTAA" ] && [ -f "$SPRUCE_ARMHF_ROOTFS_PARTAB" ] && [ -f "$SPRUCE_ARMHF_ROOTFS_PARTAC" ]; then
        cat "$SPRUCE_ARMHF_ROOTFS_PARTAA" "$SPRUCE_ARMHF_ROOTFS_PARTAB" "$SPRUCE_ARMHF_ROOTFS_PARTAC" > "$SPRUCE_ARMHF_ROOTFS_IMAGE.tmp"
        mv "$SPRUCE_ARMHF_ROOTFS_IMAGE.tmp" "$SPRUCE_ARMHF_ROOTFS_IMAGE"
    fi
    if [ -f "$SPRUCE_ARMHF_ROOTFS_IMAGE" ] && ! mount | grep -q "on $SPRUCE_ARMHF_ROOTFS_MOUNT "; then
        mount -t squashfs "$SPRUCE_ARMHF_ROOTFS_IMAGE" "$SPRUCE_ARMHF_ROOTFS_MOUNT" >/dev/null 2>&1 || true
    fi
fi

ARMHF_ROOT="$SPRUCE_ARMHF_ROOTFS_MOUNT"
ARMHF_LIB_PATH="$ARMHF_ROOT/usr/lib"
ARMHF_LOADER="$ARMHF_ROOT/usr/lib/ld-linux-armhf.so.3"
export BOX86_PATH="$X86_LIB_PATH${BOX86_PATH:+:$BOX86_PATH}"
export BOX86_LD_LIBRARY_PATH="$ARMHF_LIB_PATH${BOX86_LD_LIBRARY_PATH:+:$BOX86_LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH="$ARMHF_LIB_PATH${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

if [ -z "$PAK_DIR" ] || [ ! -x "$REAL_BOX86" ] || [ ! -f "$ARMHF_LOADER" ]; then
    echo "FATAL: missing box86 compatibility runtime" >&2
    exit 1
fi

exec "$ARMHF_LOADER" --library-path "$ARMHF_LIB_PATH" "$REAL_BOX86" "$@"
EOF
    chmod +x "$wrapper_path" 2>/dev/null || true
}

write_armhf_exec_compat_wrapper() {
    local wrapper_path="$1"
    local wrapper_dir wrapper_name original_path
    local helper_path="$PAK_DIR/bin/pm-armhf-exec-wrapper"

    wrapper_dir=$(dirname "$wrapper_path")
    wrapper_name=$(basename "$wrapper_path")
    original_path="$wrapper_dir/${wrapper_name}.original"

    if [ ! -f "$original_path" ]; then
        mv "$wrapper_path" "$original_path"
    fi

    if [ -x "$helper_path" ]; then
        if [ ! -f "$wrapper_path" ] || ! cmp -s "$helper_path" "$wrapper_path" 2>/dev/null; then
            cp -f "$helper_path" "$wrapper_path"
            chmod +x "$wrapper_path" 2>/dev/null || true
        fi
        return 0
    fi

    echo "PMI_WARN armhf_exec_wrapper_helper_missing=$helper_path"
    cat >"$wrapper_path" <<'EOF'
#!/bin/sh
# PMI_ARMHF_EXEC_COMPAT_WRAPPER=1
set -eu

SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SELF_NAME=$(basename -- "$0")
PAK_DIR="${PAK_DIR:-}"
TEMP_DATA_DIR="${TEMP_DATA_DIR:-$PAK_DIR/.ports_temp}"
SPRUCE_ARMHF_ROOTFS_MOUNT="$TEMP_DATA_DIR/spruceflip-rootfs"
SPRUCE_ARMHF_ROOTFS_IMAGE="$PAK_DIR/runtime/armhf/miyoo355_rootfs_32.img"
SPRUCE_ARMHF_ROOTFS_PARTAA="$PAK_DIR/runtime/armhf/miyoo355_rootfs_32.img_partaa"
SPRUCE_ARMHF_ROOTFS_PARTAB="$PAK_DIR/runtime/armhf/miyoo355_rootfs_32.img_partab"
SPRUCE_ARMHF_ROOTFS_PARTAC="$PAK_DIR/runtime/armhf/miyoo355_rootfs_32.img_partac"
REAL_BINARY="$SELF_DIR/${SELF_NAME}.original"

if [ -n "$PAK_DIR" ] && { [ -f "$SPRUCE_ARMHF_ROOTFS_IMAGE" ] || [ -f "$SPRUCE_ARMHF_ROOTFS_PARTAA" ]; }; then
    mkdir -p "$SPRUCE_ARMHF_ROOTFS_MOUNT"
    if [ ! -f "$SPRUCE_ARMHF_ROOTFS_IMAGE" ] && [ -f "$SPRUCE_ARMHF_ROOTFS_PARTAA" ] && [ -f "$SPRUCE_ARMHF_ROOTFS_PARTAB" ] && [ -f "$SPRUCE_ARMHF_ROOTFS_PARTAC" ]; then
        cat "$SPRUCE_ARMHF_ROOTFS_PARTAA" "$SPRUCE_ARMHF_ROOTFS_PARTAB" "$SPRUCE_ARMHF_ROOTFS_PARTAC" > "$SPRUCE_ARMHF_ROOTFS_IMAGE.tmp"
        mv "$SPRUCE_ARMHF_ROOTFS_IMAGE.tmp" "$SPRUCE_ARMHF_ROOTFS_IMAGE"
    fi
    if [ -f "$SPRUCE_ARMHF_ROOTFS_IMAGE" ] && ! mount | grep -q "on $SPRUCE_ARMHF_ROOTFS_MOUNT "; then
        mount -t squashfs "$SPRUCE_ARMHF_ROOTFS_IMAGE" "$SPRUCE_ARMHF_ROOTFS_MOUNT" >/dev/null 2>&1 || true
    fi
fi

ARMHF_ROOT="$SPRUCE_ARMHF_ROOTFS_MOUNT"
ARMHF_LIB_PATH="$ARMHF_ROOT/usr/lib"
ARMHF_LOADER="$ARMHF_ROOT/usr/lib/ld-linux-armhf.so.3"
ARMHF_EXTRA_LIB_PATH="$PAK_DIR/runtime/armhf/lib"
ARMHF_PORT_LIB_PATH=""
OLD_IFS="$IFS"
IFS=':'
for path_entry in ${LD_LIBRARY_PATH:-}; do
    case "$path_entry" in
        "$SELF_DIR"|"$SELF_DIR"/*)
            if [ -n "$ARMHF_PORT_LIB_PATH" ]; then
                ARMHF_PORT_LIB_PATH="$ARMHF_PORT_LIB_PATH:$path_entry"
            else
                ARMHF_PORT_LIB_PATH="$path_entry"
            fi
            ;;
    esac
done
IFS="$OLD_IFS"
if [ -d "$ARMHF_EXTRA_LIB_PATH" ]; then
    if [ -n "$ARMHF_PORT_LIB_PATH" ]; then
        ARMHF_PORT_LIB_PATH="$ARMHF_EXTRA_LIB_PATH:$ARMHF_PORT_LIB_PATH"
    else
        ARMHF_PORT_LIB_PATH="$ARMHF_EXTRA_LIB_PATH"
    fi
fi
ARMHF_EFFECTIVE_LD_LIBRARY_PATH="$ARMHF_LIB_PATH${ARMHF_PORT_LIB_PATH:+:$ARMHF_PORT_LIB_PATH}"
export LD_LIBRARY_PATH="$ARMHF_EFFECTIVE_LD_LIBRARY_PATH"

if [ -z "$PAK_DIR" ] || [ ! -x "$REAL_BINARY" ] || [ ! -f "$ARMHF_LOADER" ]; then
    echo "FATAL: missing armhf compatibility runtime" >&2
    exit 1
fi

exec "$ARMHF_LOADER" --argv0 "$SELF_NAME" --library-path "$ARMHF_EFFECTIVE_LD_LIBRARY_PATH" "$REAL_BINARY" "$@"
EOF
    chmod +x "$wrapper_path" 2>/dev/null || true
}

refresh_box_runtime_wrappers() {
    local search_path="$1"
    local file

    [ -d "$search_path" ] || return 0

    find "$search_path" -type f \( -path '*/box86/box86' -o -path '*/box64/box64' \) | while IFS= read -r file || [ -n "$file" ]; do
        [ -n "$file" ] || continue
        case "$file" in
            */box86/box86)
                write_box86_compat_wrapper "$file"
                ;;
            */box64/box64)
                write_box64_wrapper "$file"
                ;;
        esac
    done
}

process_armhf_binary_wrappers_for_port() {
    local port_dir="$1"
    local cache_path
    local record_type record_path field1
    local file

    [ -d "$port_dir" ] || return 0

    if refresh_armhf_probe_cache "$port_dir"; then
        cache_path=$(armhf_probe_cache_path "$port_dir")
        if [ -f "$cache_path" ]; then
            while IFS="$(printf '\t')" read -r record_type record_path field1 || [ -n "${record_type:-}" ]; do
                [ "$record_type" = "BIN" ] || continue
                [ -f "$record_path" ] || continue
                write_armhf_exec_compat_wrapper "$record_path"
            done < "$cache_path"
            touch "$cache_path" 2>/dev/null || true
            return 0
        fi
    fi

    find "$port_dir" -maxdepth 2 -type f -perm -111 | while IFS= read -r file || [ -n "$file" ]; do
        [ -n "$file" ] || continue
        case "$(basename "$file")" in
            *.sh|*.bash|*.so|*.so.*|*.original)
                continue
                ;;
        esac
        set -- $(dd if="$file" bs=1 count=20 2>/dev/null | od -An -tx1 -v 2>/dev/null)
        [ $# -ge 20 ] || continue
        [ "$1" = "7f" ] || continue
        [ "$2" = "45" ] || continue
        [ "$3" = "4c" ] || continue
        [ "$4" = "46" ] || continue
        [ "$5" = "01" ] || continue
        [ "${19}" = "28" ] || continue
        [ "${20}" = "00" ] || continue
        write_armhf_exec_compat_wrapper "$file"
    done
    if [ -f "$cache_path" ]; then
        touch "$cache_path" 2>/dev/null || true
    fi
}

refresh_armhf_binary_wrappers() {
    local search_path="$1"
    local port_json port_dir

    [ -d "$search_path" ] || return 0

    for port_json in "$search_path"/*/port.json; do
        [ -f "$port_json" ] || continue
        port_has_arch "$port_json" "armhf" || continue
        port_dir=$(dirname "$port_json")
        process_armhf_binary_wrappers_for_port "$port_dir"
    done
}

refresh_armhf_binary_wrappers_for_launcher() {
    local launcher_path="$1"
    local port_dir

    if ! port_dir=$(launcher_armhf_port_dir "$launcher_path"); then
        return 0
    fi

    process_armhf_binary_wrappers_for_port "$port_dir"
}

default_sdl2_candidate_path() {
    if [ -n "${PMI_SDL2_SYSTEM_LIB:-}" ] && [ -f "$PMI_SDL2_SYSTEM_LIB" ]; then
        printf '%s\n' "$PMI_SDL2_SYSTEM_LIB"
        return 0
    fi

    if [ -f /usr/lib/libSDL2-2.0.so.0 ]; then
        printf '%s\n' /usr/lib/libSDL2-2.0.so.0
        return 0
    fi

    if [ -f /lib/libSDL2-2.0.so.0 ]; then
        printf '%s\n' /lib/libSDL2-2.0.so.0
        return 0
    fi

    return 1
}

default_sdl2_has_default_audio_info_symbol() {
    local system_sdl

    if [ "${PMI_SDL2_DEFAULT_AUDIO_INFO_SCANNED:-0}" = "1" ]; then
        [ "${PMI_SDL2_DEFAULT_AUDIO_INFO_PRESENT:-0}" = "1" ]
        return $?
    fi

    PMI_SDL2_DEFAULT_AUDIO_INFO_SCANNED=1
    PMI_SDL2_DEFAULT_AUDIO_INFO_PRESENT=0

    command -v strings >/dev/null 2>&1 || return 1

    if ! system_sdl=$(default_sdl2_candidate_path); then
        return 1
    fi

    PMI_SDL2_SYSTEM_LIB_CACHED="$system_sdl"
    if strings "$system_sdl" 2>/dev/null | grep -qx 'SDL_GetDefaultAudioInfo'; then
        PMI_SDL2_DEFAULT_AUDIO_INFO_PRESENT=1
    fi

    [ "$PMI_SDL2_DEFAULT_AUDIO_INFO_PRESENT" = "1" ]
}

default_pulse_simple_candidate_path() {
    if [ -n "${PMI_PULSE_SIMPLE_SYSTEM_LIB:-}" ] && [ -f "$PMI_PULSE_SIMPLE_SYSTEM_LIB" ]; then
        printf '%s\n' "$PMI_PULSE_SIMPLE_SYSTEM_LIB"
        return 0
    fi

    if [ -f /usr/lib/libpulse-simple.so.0 ]; then
        printf '%s\n' /usr/lib/libpulse-simple.so.0
        return 0
    fi

    if [ -f /lib/libpulse-simple.so.0 ]; then
        printf '%s\n' /lib/libpulse-simple.so.0
        return 0
    fi

    return 1
}

default_pulse_simple_available() {
    local system_pulse

    if [ "${PMI_PULSE_SIMPLE_SCANNED:-0}" = "1" ]; then
        [ "${PMI_PULSE_SIMPLE_PRESENT:-0}" = "1" ]
        return $?
    fi

    PMI_PULSE_SIMPLE_SCANNED=1
    PMI_PULSE_SIMPLE_PRESENT=0

    if ! system_pulse=$(default_pulse_simple_candidate_path); then
        return 1
    fi

    PMI_PULSE_SIMPLE_SYSTEM_LIB_CACHED="$system_pulse"
    PMI_PULSE_SIMPLE_PRESENT=1
    return 0
}

is_aarch64_sdl_compat_wrapper() {
    local file="$1"

    [ -f "$file" ] || return 1
    head -n 2 "$file" 2>/dev/null | grep -Eq '^# PMI_AARCH64_(SDL_COMPAT|NATIVE_COMPAT)_WRAPPER=1$'
}

restore_aarch64_sdl_compat_wrapper() {
    local wrapper_path="$1"
    local original_path="${wrapper_path}.original"

    is_aarch64_sdl_compat_wrapper "$wrapper_path" || return 0
    [ -f "$original_path" ] || return 0

    rm -f "$wrapper_path"
    mv "$original_path" "$wrapper_path"
}

wrapper_matches_aarch64_compat_mode() {
    local wrapper_path="$1"
    local use_sdl="$2"
    local use_pulse="$3"

    [ -f "$wrapper_path" ] || return 1
    grep -q "^PMI_USE_AARCH64_SDL_COMPAT=\"$use_sdl\"$" "$wrapper_path" 2>/dev/null || return 1
    grep -q "^PMI_USE_AARCH64_PULSE_COMPAT=\"$use_pulse\"$" "$wrapper_path" 2>/dev/null || return 1
    return 0
}

write_aarch64_sdl_compat_wrapper() {
    local wrapper_path="$1"
    local use_sdl="$2"
    local use_pulse="$3"
    local wrapper_dir wrapper_name original_path

    wrapper_dir=$(dirname "$wrapper_path")
    wrapper_name=$(basename "$wrapper_path")
    original_path="$wrapper_dir/${wrapper_name}.original"

    if [ ! -f "$original_path" ]; then
        mv "$wrapper_path" "$original_path"
    fi

    cat >"$wrapper_path" <<EOF
#!/bin/sh
# PMI_AARCH64_NATIVE_COMPAT_WRAPPER=1
set -eu

SELF_DIR=\$(CDPATH= cd -- "\$(dirname -- "\$0")" && pwd)
SELF_NAME=\$(basename -- "\$0")
PAK_DIR="\${PAK_DIR:-}"
AARCH64_SDL_RUNTIME_LIB="\$PAK_DIR/runtime/aarch64/lib"
AARCH64_PULSE_RUNTIME_LIB="\$PAK_DIR/runtime/aarch64/pulse"
PMI_USE_AARCH64_SDL_COMPAT="$use_sdl"
PMI_USE_AARCH64_PULSE_COMPAT="$use_pulse"
REAL_BINARY="\$SELF_DIR/\${SELF_NAME}.original"
COMPAT_LD_LIBRARY_PATH=""

append_compat_path() {
    if [ -z "\$1" ]; then
        return 0
    fi

    if [ -n "\$COMPAT_LD_LIBRARY_PATH" ]; then
        COMPAT_LD_LIBRARY_PATH="\$COMPAT_LD_LIBRARY_PATH:\$1"
    else
        COMPAT_LD_LIBRARY_PATH="\$1"
    fi
}

capture_audio_state() {
    PMI_ORIG_PLAYBACK_PATH=\$(amixer sget "Playback Path" 2>/dev/null | sed -n "s/.*Item0: '\\([^']*\\)'.*/\\1/p" | head -n 1)
    PMI_ORIG_SPK=\$(amixer sget SPK 2>/dev/null | sed -n 's/.*Mono: \\([0-9][0-9]*\\) \\[[0-9][0-9]*%\\].*/\\1/p' | head -n 1)
}

restore_audio_state() {
    PMI_RESTORE_TARGET_PATH="\${PMI_ORIG_PLAYBACK_PATH:-SPK}"

    amixer sset "Playback Path" OFF >/dev/null 2>&1 || true
    amixer sset "Playback Path" "\$PMI_RESTORE_TARGET_PATH" >/dev/null 2>&1 || true
    amixer sset Speaker on >/dev/null 2>&1 || true
    amixer sset Headphone on >/dev/null 2>&1 || true
    if [ -n "\${PMI_ORIG_SPK:-}" ]; then
        amixer sset SPK "\$PMI_ORIG_SPK" >/dev/null 2>&1 || true
    fi
}

if [ "\$PMI_USE_AARCH64_SDL_COMPAT" = "1" ]; then
    append_compat_path "\$AARCH64_SDL_RUNTIME_LIB"
fi

if [ "\$PMI_USE_AARCH64_PULSE_COMPAT" = "1" ]; then
    append_compat_path "\$AARCH64_PULSE_RUNTIME_LIB"
fi

if [ -z "\$PAK_DIR" ] || [ ! -x "\$REAL_BINARY" ]; then
    echo "FATAL: missing aarch64 native compatibility runtime" >&2
    exit 1
fi

if [ "\$PMI_USE_AARCH64_SDL_COMPAT" = "1" ] && [ ! -f "\$AARCH64_SDL_RUNTIME_LIB/libSDL2-2.0.so.0" ]; then
    echo "FATAL: missing aarch64 SDL compatibility runtime" >&2
    exit 1
fi

if [ "\$PMI_USE_AARCH64_PULSE_COMPAT" = "1" ] && \
    { [ ! -f "\$AARCH64_PULSE_RUNTIME_LIB/libpulse-simple.so.0" ] || [ ! -f "\$AARCH64_PULSE_RUNTIME_LIB/libpulse.so.0" ] || [ ! -f "\$AARCH64_PULSE_RUNTIME_LIB/libpulsecommon-13.99.so" ]; }; then
    echo "FATAL: missing aarch64 Pulse compatibility runtime" >&2
    exit 1
fi

if [ -n "\$COMPAT_LD_LIBRARY_PATH" ]; then
    export LD_LIBRARY_PATH="\$COMPAT_LD_LIBRARY_PATH\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
fi

if [ "\$PMI_USE_AARCH64_PULSE_COMPAT" = "1" ]; then
    capture_audio_state
fi

set +e
"\$REAL_BINARY" "\$@"
PMI_NATIVE_COMPAT_STATUS=\$?
set -e

if [ "\$PMI_USE_AARCH64_PULSE_COMPAT" = "1" ]; then
    restore_audio_state
fi

exit "\$PMI_NATIVE_COMPAT_STATUS"
EOF
    chmod +x "$wrapper_path" 2>/dev/null || true
}

maybe_refresh_aarch64_sdl_compat_binary_from_probe() {
    local binary_path="$1"
    local needs_default_audio_info_field="$2"
    local needs_pulse_simple_field="$3"
    local needs_default_audio_info="${needs_default_audio_info_field#needs_default_audio_info=}"
    local needs_pulse_simple="${needs_pulse_simple_field#needs_pulse_simple=}"
    local runtime_sdl="$PAK_DIR/runtime/aarch64/lib/libSDL2-2.0.so.0"
    local runtime_pulse_dir="$PAK_DIR/runtime/aarch64/pulse"
    local use_sdl=0
    local use_pulse=0

    [ -f "$binary_path" ] || return 0

    if [ "$needs_default_audio_info" = "1" ] && ! default_sdl2_has_default_audio_info_symbol; then
        use_sdl=1
    fi

    if [ "$needs_pulse_simple" = "1" ] && ! default_pulse_simple_available; then
        use_pulse=1
    fi

    if [ "$use_sdl" = "0" ] && [ "$use_pulse" = "0" ]; then
        restore_aarch64_sdl_compat_wrapper "$binary_path"
        return 0
    fi

    if [ "$use_sdl" = "1" ] && [ ! -f "$runtime_sdl" ]; then
        echo "PMI_WARN aarch64_sdl_compat_missing=$runtime_sdl"
        return 0
    fi

    if [ "$use_pulse" = "1" ] && \
        { [ ! -f "$runtime_pulse_dir/libpulse-simple.so.0" ] || [ ! -f "$runtime_pulse_dir/libpulse.so.0" ] || [ ! -f "$runtime_pulse_dir/libpulsecommon-13.99.so" ]; }; then
        echo "PMI_WARN aarch64_pulse_compat_missing=$runtime_pulse_dir"
        return 0
    fi

    if ! wrapper_matches_aarch64_compat_mode "$binary_path" "$use_sdl" "$use_pulse"; then
        write_aarch64_sdl_compat_wrapper "$binary_path" "$use_sdl" "$use_pulse"
        echo "PMI_DIAG aarch64_native_compat_applied=$binary_path sdl=$use_sdl pulse=$use_pulse"
    fi
}

load_aarch64_port_probe_state() {
    local port_dir="$1"
    local cache_path
    local record_type
    local record_path
    local field1
    local field2
    local field3
    local field4
    local uses_sdl_gl_windowing
    local is_wrapper

    if [ "$PMI_AARCH64_PROBE_PORT_DIR" = "$port_dir" ] && [ -f "$PMI_AARCH64_PROBE_CACHE_PATH" ]; then
        return 0
    fi

    refresh_port_probe_cache "$port_dir" || return 1
    cache_path=$(port_probe_cache_path "$port_dir")
    [ -f "$cache_path" ] || return 1

    PMI_AARCH64_PROBE_PORT_DIR="$port_dir"
    PMI_AARCH64_PROBE_CACHE_PATH="$cache_path"
    PMI_AARCH64_PROBE_HAS_BUNDLED_NATIVE_GL=0
    PMI_AARCH64_PROBE_NEEDS_SYSTEM_GL=0

    while IFS=$'\t' read -r record_type record_path field1 field2 field3 field4 || [ -n "${record_type:-}" ]; do
        [ -n "${record_type:-}" ] || continue
        case "$record_type" in
            PORT)
                if [ "$field1" = "has_bundled_native_gl_stack=1" ]; then
                    PMI_AARCH64_PROBE_HAS_BUNDLED_NATIVE_GL=1
                fi
                ;;
            BIN)
                uses_sdl_gl_windowing="${field3#uses_sdl_gl_windowing=}"
                is_wrapper="${field4#is_wrapper=}"
                if [ "$uses_sdl_gl_windowing" = "1" ] && [ "$is_wrapper" != "1" ]; then
                    PMI_AARCH64_PROBE_NEEDS_SYSTEM_GL=1
                fi
                ;;
        esac
    done < "$cache_path"

    if [ "$PMI_AARCH64_PROBE_HAS_BUNDLED_NATIVE_GL" = "1" ]; then
        PMI_AARCH64_PROBE_NEEDS_SYSTEM_GL=0
    fi
}

process_aarch64_sdl_compat_for_port() {
    local port_dir="$1"
    local record_type
    local record_path
    local field1
    local field2
    local field3
    local field4

    load_aarch64_port_probe_state "$port_dir" || return 0

    while IFS=$'\t' read -r record_type record_path field1 field2 field3 field4 || [ -n "${record_type:-}" ]; do
        [ -n "${record_type:-}" ] || continue
        [ "$record_type" = "BIN" ] || continue
        maybe_refresh_aarch64_sdl_compat_binary_from_probe "$record_path" "$field1" "$field2"
    done < "$PMI_AARCH64_PROBE_CACHE_PATH"
    touch "$PMI_AARCH64_PROBE_CACHE_PATH" 2>/dev/null || true
}

refresh_aarch64_sdl_compat_wrappers() {
    local search_path="$1"
    local port_json
    local port_dir

    [ -d "$search_path" ] || return 0

    for port_json in "$search_path"/*/port.json; do
        [ -f "$port_json" ] || continue
        if ! port_has_arch "$port_json" "aarch64"; then
            continue
        fi

        port_dir=$(dirname "$port_json")
        process_aarch64_sdl_compat_for_port "$port_dir"
    done
}

refresh_aarch64_sdl_compat_for_launcher() {
    local launcher_path="$1"
    local port_dir

    if ! port_dir=$(launcher_aarch64_port_dir "$launcher_path"); then
        return 0
    fi

    process_aarch64_sdl_compat_for_port "$port_dir"
}

launcher_requires_system_gl_stack() {
    local launcher_path="$1"
    local port_dir

    if ! port_dir=$(launcher_aarch64_port_dir "$launcher_path"); then
        return 1
    fi

    load_aarch64_port_probe_state "$port_dir" || return 1
    [ "$PMI_AARCH64_PROBE_NEEDS_SYSTEM_GL" = "1" ]
}

seed_x86_runtime_libs() {
    local search_path="$1"
    local runtime_dir="$PAK_DIR/lib/box64-i386-linux-gnu"
    local lib_dir

    [ -d "$search_path" ] || return 0
    [ -d "$runtime_dir" ] || return 0

    find "$search_path" -type d -path '*/gamedata/*/32/lib' | while IFS= read -r lib_dir || [ -n "$lib_dir" ]; do
        [ -n "$lib_dir" ] || continue
        [ -f "$lib_dir/libstdc++.so.6" ] || cp -f "$runtime_dir/libstdc++.so.6" "$lib_dir/" 2>/dev/null || true
        [ -f "$lib_dir/libgcc_s.so.1" ] || cp -f "$runtime_dir/libgcc_s.so.1" "$lib_dir/" 2>/dev/null || true
    done
}

process_squashfs_files() {
    local search_dir="$1"
    local unsquashfs_bin
    local mksquashfs_bin

    [ -d "$search_dir" ] || return 0
    unsquashfs_bin=$(command -v unsquashfs || true)
    mksquashfs_bin=$(command -v mksquashfs || true)
    if [ -z "$unsquashfs_bin" ] || [ -z "$mksquashfs_bin" ]; then
        echo "Skipping squashfs rewrite; unsquashfs/mksquashfs unavailable"
        return 0
    fi

    find "$search_dir" -type f -name '*.squashfs' | while IFS= read -r squashfs_file || [ -n "$squashfs_file" ]; do
        local processed_marker tmpdir

        [ -z "$squashfs_file" ] && continue
        processed_marker="${squashfs_file}.processed"
        if [ -f "$processed_marker" ] && [ "$squashfs_file" -ot "$processed_marker" ]; then
            continue
        fi

        tmpdir=$(mktemp -d) || continue
        if "$unsquashfs_bin" -no-progress -d "$tmpdir" "$squashfs_file"; then
            rewrite_scripts_in_tree "$tmpdir"
            rm -f "$squashfs_file"
            if "$mksquashfs_bin" "$tmpdir" "$squashfs_file" -noappend -comp xz -no-progress; then
                touch "$processed_marker"
            fi
        fi
        rm -rf "$tmpdir"
    done
}

write_game_wrapper() {
    local wrapper_path="$1"
    local target_script="$2"

    cat >"$wrapper_path" <<EOF
#!/bin/sh
set -eu

export PMI_PORT_SCRIPT="$target_script"
exec "$PAK_DIR/launch.sh" "$wrapper_path"
EOF
    chmod +x "$wrapper_path" 2>/dev/null || true
}

copy_game_scripts() {
    local source_script wrapper_path

    mkdir -p "$ROM_DIR"
    for source_script in "$REAL_PORTS_DIR"/*.sh; do
        [ -f "$source_script" ] || continue
        wrapper_path="$ROM_DIR/$(basename "$source_script")"
        write_game_wrapper "$wrapper_path" "$source_script"
    done
}

port_shell_from_json() {
    local port_json="$1"

    [ -f "$port_json" ] || return 1
    sed -n 's|.*"\([^"]*\.sh\)".*|\1|p' "$port_json" | head -n 1
}

write_artwork_png() {
    local artwork_file="$1"
    local dest_file="$2"

    case "$artwork_file" in
        *.png|*.PNG)
            cp -f "$artwork_file" "$dest_file" 2>/dev/null || true
            ;;
        *.jpg|*.JPG|*.jpeg|*.JPEG)
            if [ -x "$PAK_DIR/bin/pm-artwork-convert" ]; then
                "$PAK_DIR/bin/pm-artwork-convert" "$artwork_file" "$dest_file" >/dev/null 2>&1 || \
                    echo "Failed to convert artwork for $dest_file from $artwork_file"
            else
                echo "Skipping JPEG artwork; converter unavailable: $artwork_file"
            fi
            ;;
        *)
            echo "Skipping unsupported artwork for $dest_file: $artwork_file"
            ;;
    esac
}

copy_artwork_to_media() {
    local port_dir port_json shell_script artwork_file screenshot_candidate dest_file

    mkdir -p "$ROM_DIR/.media"
    for port_dir in "$REAL_PORTS_DIR"/*/; do
        [ -d "$port_dir" ] || continue
        port_json="$port_dir/port.json"
        [ -f "$port_json" ] || continue

        shell_script=$(port_shell_from_json "$port_json")
        [ -n "$shell_script" ] || continue

        artwork_file="$port_dir/cover.png"
        if [ ! -f "$artwork_file" ]; then
            screenshot_candidate=$(find "$port_dir" -maxdepth 1 -type f \( -name 'screenshot*' -o -name '*-pre.png' -o -name '*-pre.jpg' \) | head -n 1)
            artwork_file="$screenshot_candidate"
        fi
        [ -n "${artwork_file:-}" ] && [ -f "$artwork_file" ] || continue

        dest_file="$ROM_DIR/.media/$(basename "${shell_script%.*}").png"
        write_artwork_png "$artwork_file" "$dest_file"
    done
}

clear_ports_cache() {
    rm -f "$ROM_DIR/PORTS_cache7.db" "$ROM_DIR/Ports_cache7.db" 2>/dev/null || true
}

post_gui_rewrites() {
    process_squashfs_files "$REAL_PORTS_DIR"
    ensure_global_port_repairs
    copy_game_scripts
    copy_artwork_to_media
    clear_ports_cache
}

launch_gui() {
    chmod -R +x "$PM_RUNTIME_ROOT" 2>/dev/null || true
    rm -f "$PM_RUNTIME_ROOT/.pugwash-reboot"
    start_power_lid_helper

    while true; do
        ./pugwash --debug

        if [ ! -f "$PM_RUNTIME_ROOT/.pugwash-reboot" ]; then
            break
        fi

        rm -f "$PM_RUNTIME_ROOT/.pugwash-reboot"
    done

    stop_power_lid_helper
    post_gui_rewrites
}

launch_port_script() {
    local source_script="${PMI_PORT_SCRIPT:-$ROM_PATH}"
    local script_to_run
    local armhf_port_dir

    ensure_global_port_repairs
    refresh_armhf_binary_wrappers_for_launcher "$source_script"
    refresh_aarch64_sdl_compat_for_launcher "$source_script"
    script_to_run=$(stage_launch_script "$source_script")
    echo "PMI_DIAG selected_port_script=$source_script"
    echo "PMI_DIAG rewritten_launch_path=$script_to_run"
    if armhf_port_dir=$(launcher_armhf_port_dir "$source_script"); then
        seed_x86_runtime_libs "$armhf_port_dir"
    fi
    if launcher_requires_flip_libmali "$source_script"; then
        bind_flip_libmali
    fi
    prepare_flip_joy_type_for_port
    chmod +x "$script_to_run" 2>/dev/null || true
    start_power_lid_helper
    if launcher_requires_system_gl_stack "$source_script"; then
        echo "PMI_DIAG system_gl_stack_launcher=$source_script"
        PMI_LD_LIBRARY_STRATEGY=system-gl bash "$script_to_run"
    else
        bash "$script_to_run"
    fi
    stop_power_lid_helper
}

trap cleanup EXIT INT TERM HUP QUIT

if [ "${PMI_TEST_MODE:-}" = "overlay-sync" ]; then
    ensure_runtime_parity_overlay
    process_runtime_autoinstall
    exit 0
fi

mount_ports_dir
ensure_runtime_config
ensure_runtime_bootstrap
unpack_pylibs
ensure_runtime_parity_overlay
process_runtime_autoinstall
cd "$PM_RUNTIME_ROOT"

if echo "$ROM_NAME" | grep -qi "portmaster"; then
    launch_gui
else
    launch_port_script
fi
