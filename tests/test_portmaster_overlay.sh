#!/bin/sh
set -eu

root="$(mktemp -d)"
trap 'rm -rf "$root"' EXIT

pak_dir="$root/Emus/my355/PORTS.pak"
emu_dir="$pak_dir/PortMaster"
home_root="$root/home"
xdg_data_home="$home_root/.local/share"
temp_data_dir="$root/.ports_temp"
log_file="$root/PORTS.txt"
helper_marker="$root/power-lid-helper.marker"

mkdir -p "$pak_dir/files" "$emu_dir/pylibs/harbourmaster" "$emu_dir/miyoo" "$home_root" "$xdg_data_home/PortMaster" "$temp_data_dir"
cp payload/PORTS.pak/Portmaster.sh "$pak_dir/Portmaster.sh"
mkdir -p "$pak_dir/bin" "$pak_dir/lib"
printf '#!/bin/sh\nexit 0\n' >"$pak_dir/bin/runtime-helper"
chmod +x "$pak_dir/bin/runtime-helper"
cat >"$pak_dir/bin/pm-power-lid-watch" <<EOF
#!/bin/sh
set -eu
touch "$helper_marker"
exit 0
EOF
chmod +x "$pak_dir/bin/pm-power-lid-watch"
printf 'stub' >"$pak_dir/lib/libruntime-helper.so"

cat >"$pak_dir/files/control.txt" <<EOF
#!/bin/bash
CUR_TTY=/dev/null
XDG_DATA_HOME="\${XDG_DATA_HOME:-\$HOME/.local/share}"
runtime_root="\$PM_RUNTIME_ROOT"
export controlfolder="\$runtime_root"
export directory="${temp_data_dir#/}"
export PATH="\${runtime_root%/PortMaster}/bin:\$PATH"
export LD_LIBRARY_PATH="\${runtime_root%/PortMaster}/lib\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
export ESUDO=""
export ESUDOKILL="-1"
export SDL_GAMECONTROLLERCONFIG_FILE="\${PMI_GAMECONTROLLERDB_FILE:-\$controlfolder/gamecontrollerdb.txt}"
get_controls() {
  LOWRES="N"
  ANALOGSTICKS="\${ANALOG_STICKS:-0}"
}
source "\$controlfolder/device_info.txt"
source "\$controlfolder/funcs.txt"
export GPTOKEYB2="stub"
export GPTOKEYB="stub"
EOF

cat >"$pak_dir/files/PortMaster.txt" <<'EOF'
#!/bin/sh
exit 0
EOF

cat >"$pak_dir/files/config.py" <<'EOF'
HM_TOOLS_DIR=None
EOF

cat >"$pak_dir/files/gamecontrollerdb.txt" <<'EOF'
03000000de2800000112000001000000,Steam Controller,a:b0,b:b1,start:b7,platform:Linux,
EOF

cat >"$pak_dir/files/gamecontrollerdb_nintendo.txt" <<'EOF'
03000000de2800000112000001000000,Steam Controller,a:b1,b:b0,start:b7,platform:Linux,
EOF

cp "$pak_dir/files/gamecontrollerdb.txt" "$xdg_data_home/PortMaster/gamecontrollerdb.txt"
touch -t 202001010000 "$xdg_data_home/PortMaster/gamecontrollerdb.txt"

cat >"$emu_dir/device_info.txt" <<'EOF'
ANALOG_STICKS=2
DEVICE_ARCH=aarch64
EOF

cat >"$emu_dir/funcs.txt" <<'EOF'
#!/bin/sh
EOF

PAK_DIR="$pak_dir" \
EMU_DIR="$emu_dir" \
TEMP_DATA_DIR="$temp_data_dir" \
XDG_DATA_HOME="$xdg_data_home" \
PMI_LOG_FILE="$log_file" \
HOME="$home_root" \
PMI_TEST_MODE=overlay-sync \
bash "$pak_dir/Portmaster.sh"

cmp "$pak_dir/files/control.txt" "$emu_dir/control.txt"
cmp "$pak_dir/files/control.txt" "$emu_dir/miyoo/control.txt"
cmp "$pak_dir/files/PortMaster.txt" "$emu_dir/PortMaster.txt"
cmp "$pak_dir/files/PortMaster.txt" "$emu_dir/PortMaster.sh"
cmp "$pak_dir/files/PortMaster.txt" "$emu_dir/miyoo/PortMaster.txt"
cmp "$pak_dir/files/config.py" "$emu_dir/pylibs/harbourmaster/config.py"
cmp "$pak_dir/files/gamecontrollerdb.txt" "$emu_dir/gamecontrollerdb.txt"
cmp "$pak_dir/files/control.txt" "$xdg_data_home/PortMaster/control.txt"
cmp "$pak_dir/files/gamecontrollerdb.txt" "$xdg_data_home/PortMaster/gamecontrollerdb.txt"

first_hash="$(cksum "$emu_dir/control.txt" "$emu_dir/miyoo/control.txt" "$emu_dir/PortMaster.sh" "$emu_dir/miyoo/PortMaster.txt" "$emu_dir/gamecontrollerdb.txt" "$xdg_data_home/PortMaster/control.txt" "$xdg_data_home/PortMaster/gamecontrollerdb.txt")"

PAK_DIR="$pak_dir" \
EMU_DIR="$emu_dir" \
TEMP_DATA_DIR="$temp_data_dir" \
XDG_DATA_HOME="$xdg_data_home" \
PMI_LOG_FILE="$log_file" \
HOME="$home_root" \
PMI_TEST_MODE=overlay-sync \
bash "$pak_dir/Portmaster.sh"

second_hash="$(cksum "$emu_dir/control.txt" "$emu_dir/miyoo/control.txt" "$emu_dir/PortMaster.sh" "$emu_dir/miyoo/PortMaster.txt" "$emu_dir/gamecontrollerdb.txt" "$xdg_data_home/PortMaster/control.txt" "$xdg_data_home/PortMaster/gamecontrollerdb.txt")"
[ "$first_hash" = "$second_hash" ]
[ ! -e "$helper_marker" ]

: >"$home_root/nintendo"
PAK_DIR="$pak_dir" \
EMU_DIR="$emu_dir" \
TEMP_DATA_DIR="$temp_data_dir" \
XDG_DATA_HOME="$xdg_data_home" \
PMI_LOG_FILE="$log_file" \
HOME="$home_root" \
PMI_TEST_MODE=overlay-sync \
bash "$pak_dir/Portmaster.sh"

cmp "$pak_dir/files/gamecontrollerdb.txt" "$emu_dir/gamecontrollerdb.txt"
cmp "$pak_dir/files/gamecontrollerdb.txt" "$xdg_data_home/PortMaster/gamecontrollerdb.txt"
grep -q 'PMI_DIAG overlay_controller_layout=xbox' "$log_file"
! grep -q 'PMI_DIAG overlay_controller_layout_sentinel=' "$log_file"

rm -f "$home_root/nintendo"
PAK_DIR="$pak_dir" \
EMU_DIR="$emu_dir" \
TEMP_DATA_DIR="$temp_data_dir" \
XDG_DATA_HOME="$xdg_data_home" \
PMI_LOG_FILE="$log_file" \
HOME="$home_root" \
PMI_TEST_MODE=overlay-sync \
bash "$pak_dir/Portmaster.sh"

cmp "$pak_dir/files/gamecontrollerdb.txt" "$emu_dir/gamecontrollerdb.txt"
cmp "$pak_dir/files/gamecontrollerdb.txt" "$xdg_data_home/PortMaster/gamecontrollerdb.txt"
grep -q 'PMI_DIAG overlay_controller_layout=xbox' "$log_file"

CONTROL_FILE="$xdg_data_home/PortMaster/control.txt" \
EXPECTED_CONTROL="$emu_dir" \
EXPECTED_DB="$emu_dir/gamecontrollerdb.txt" \
EXPECTED_BIN="$pak_dir/bin" \
EXPECTED_LIB="$pak_dir/lib" \
HOME="$root/home" \
XDG_DATA_HOME="$xdg_data_home" \
PM_RUNTIME_ROOT="$emu_dir" \
PMI_LOG_FILE="$log_file" \
bash -c '. "$CONTROL_FILE"; get_controls; [ "$controlfolder" = "$EXPECTED_CONTROL" ]; [ "$SDL_GAMECONTROLLERCONFIG_FILE" = "$EXPECTED_DB" ]; case ":$PATH:" in *":$EXPECTED_BIN:"*) ;; *) exit 1;; esac; case ":$LD_LIBRARY_PATH:" in *":$EXPECTED_LIB:"*) ;; *) exit 1;; esac'

CONTROL_FILE="$xdg_data_home/PortMaster/control.txt" \
EXPECTED_CONTROL="$emu_dir" \
EXPECTED_DB="$pak_dir/files/gamecontrollerdb_nintendo.txt" \
EXPECTED_BIN="$pak_dir/bin" \
EXPECTED_LIB="$pak_dir/lib" \
HOME="$root/home" \
XDG_DATA_HOME="$xdg_data_home" \
PM_RUNTIME_ROOT="$emu_dir" \
PMI_GAMECONTROLLERDB_FILE="$pak_dir/files/gamecontrollerdb_nintendo.txt" \
PMI_LOG_FILE="$log_file" \
bash -c '. "$CONTROL_FILE"; get_controls; [ "$controlfolder" = "$EXPECTED_CONTROL" ]; [ "$SDL_GAMECONTROLLERCONFIG_FILE" = "$EXPECTED_DB" ]; case ":$PATH:" in *":$EXPECTED_BIN:"*) ;; *) exit 1;; esac; case ":$LD_LIBRARY_PATH:" in *":$EXPECTED_LIB:"*) ;; *) exit 1;; esac'
