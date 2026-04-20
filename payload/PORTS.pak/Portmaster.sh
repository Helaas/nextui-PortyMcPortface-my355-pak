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

sync_overlay_file() {
    local src="$1"
    local dst="$2"

    [ -f "$src" ] || return 0
    mkdir -p "$(dirname "$dst")"
    cp -f "$src" "$dst"
}

sync_overlay_tree() {
    local src_dir="$1"
    local dst_dir="$2"

    [ -d "$src_dir" ] || return 0
    mkdir -p "$dst_dir"
    cp -Rf "$src_dir"/. "$dst_dir"/
}

ensure_runtime_parity_overlay() {
    local xdg_pm_dir="$XDG_DATA_HOME/PortMaster"

    sync_overlay_file "$PAK_DIR/files/control.txt" "$PM_RUNTIME_ROOT/control.txt"
    sync_overlay_file "$PAK_DIR/files/control.txt" "$PM_RUNTIME_ROOT/miyoo/control.txt"
    sync_overlay_file "$PAK_DIR/files/PortMaster.txt" "$PM_RUNTIME_ROOT/PortMaster.txt"
    sync_overlay_file "$PAK_DIR/files/PortMaster.txt" "$PM_RUNTIME_ROOT/PortMaster.sh"
    sync_overlay_file "$PAK_DIR/files/PortMaster.txt" "$PM_RUNTIME_ROOT/miyoo/PortMaster.txt"
    sync_overlay_file "$PAK_DIR/files/config.py" "$PM_RUNTIME_ROOT/pylibs/harbourmaster/config.py"
    sync_overlay_file "$PAK_DIR/files/gamecontrollerdb.txt" "$PM_RUNTIME_ROOT/gamecontrollerdb.txt"
    sync_overlay_file "$PAK_DIR/files/control.txt" "$xdg_pm_dir/control.txt"
    sync_overlay_file "$PAK_DIR/files/gamecontrollerdb.txt" "$xdg_pm_dir/gamecontrollerdb.txt"
    chmod +x "$PM_RUNTIME_ROOT/PortMaster.txt" 2>/dev/null || true
    chmod +x "$PM_RUNTIME_ROOT/PortMaster.sh" 2>/dev/null || true
    chmod +x "$PM_RUNTIME_ROOT/miyoo/PortMaster.txt" 2>/dev/null || true

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
        mkdir -p "$PAK_DIR/lib"
        : >"$lib_bootstrap_stamp"
    fi
    sanitize_runtime_lib_dir
    if [ -f "$PAK_DIR/files/libffi.so.7" ] && [ ! -f "$PAK_DIR/lib/libffi.so.7" ]; then
        mkdir -p "$PAK_DIR/lib"
        cp -f "$PAK_DIR/files/libffi.so.7" "$PAK_DIR/lib/libffi.so.7"
    fi
    if ! command -v pkill >/dev/null 2>&1; then
        cat >"$PAK_DIR/bin/pkill" <<'EOF'
#!/bin/sh
set -eu

if [ "${1:-}" = "-f" ] && [ $# -ge 2 ]; then
    pattern="$2"
    self_pid="$$"
    if ! command -v pgrep >/dev/null 2>&1; then
        exit 0
    fi

    pgrep -f "$pattern" | while IFS= read -r pid || [ -n "$pid" ]; do
        [ -n "$pid" ] || continue
        [ "$pid" = "$self_pid" ] && continue
        kill "$pid" >/dev/null 2>&1 || true
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
    local staged_script="$staged_dir/$(basename "$source_script")"

    mkdir -p "$staged_dir"
    cp -f "$source_script" "$staged_script"
    rewrite_script_file "$staged_script"
    chmod +x "$staged_script" 2>/dev/null || true
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

launcher_requires_armhf() {
    local launcher_path="$1"
    local port_json shell_name

    for port_json in "$REAL_PORTS_DIR"/*/port.json; do
        [ -f "$port_json" ] || continue
        shell_name=$(port_shell_from_json "$port_json")
        [ -n "$shell_name" ] || continue
        if [ "$REAL_PORTS_DIR/$shell_name" = "$launcher_path" ] && port_has_arch "$port_json" "armhf"; then
            return 0
        fi
    done

    return 1
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

    grep -q 'sbc_4_3_rcv12' "$launcher_path" 2>/dev/null
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
    heal_installed_port_launchers
    apply_port_arch_rewrites
    refresh_box_runtime_wrappers "$REAL_PORTS_DIR"
    process_squashfs_files "$REAL_PORTS_DIR"
    copy_game_scripts
    copy_artwork_to_media
    clear_ports_cache
}

launch_gui() {
    chmod -R +x "$PM_RUNTIME_ROOT" 2>/dev/null || true
    rm -f "$PM_RUNTIME_ROOT/.pugwash-reboot"

    while true; do
        ./pugwash --debug

        if [ ! -f "$PM_RUNTIME_ROOT/.pugwash-reboot" ]; then
            break
        fi

        rm -f "$PM_RUNTIME_ROOT/.pugwash-reboot"
    done

    post_gui_rewrites
}

launch_port_script() {
    local source_script="${PMI_PORT_SCRIPT:-$ROM_PATH}"
    local script_to_run

    heal_installed_port_launchers
    apply_port_arch_rewrites
    refresh_box_runtime_wrappers "$REAL_PORTS_DIR"
    script_to_run=$(stage_launch_script "$source_script")
    echo "PMI_DIAG selected_port_script=$source_script"
    echo "PMI_DIAG rewritten_launch_path=$script_to_run"
    if launcher_requires_armhf "$source_script"; then
        seed_x86_runtime_libs "$REAL_PORTS_DIR"
    fi
    if launcher_requires_flip_libmali "$source_script"; then
        bind_flip_libmali
    fi
    prepare_flip_joy_type_for_port
    chmod +x "$script_to_run" 2>/dev/null || true
    bash "$script_to_run"
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
