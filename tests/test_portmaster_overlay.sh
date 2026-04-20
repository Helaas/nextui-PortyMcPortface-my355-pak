#!/bin/sh
set -eu

root="$(mktemp -d)"
trap 'rm -rf "$root"' EXIT

pak_dir="$root/Emus/my355/PORTS.pak"
emu_dir="$pak_dir/PortMaster"
xdg_data_home="$root/home/.local/share"
temp_data_dir="$root/.ports_temp"
log_file="$root/PORTS.txt"

mkdir -p "$pak_dir/files" "$emu_dir/pylibs/harbourmaster" "$emu_dir/miyoo" "$xdg_data_home/PortMaster" "$temp_data_dir"
cp payload/PORTS.pak/Portmaster.sh "$pak_dir/Portmaster.sh"
mkdir -p "$pak_dir/bin" "$pak_dir/lib"
printf '#!/bin/sh\nexit 0\n' >"$pak_dir/bin/runtime-helper"
chmod +x "$pak_dir/bin/runtime-helper"
printf 'stub' >"$pak_dir/lib/libruntime-helper.so"

cat >"$pak_dir/files/control.txt" <<EOF
#!/bin/bash
XDG_DATA_HOME="\${XDG_DATA_HOME:-\$HOME/.local/share}"
runtime_root="\$XDG_DATA_HOME"
if [ -d "\$XDG_DATA_HOME/PortMaster" ]; then
  export controlfolder="\$XDG_DATA_HOME/PortMaster"
else
  export controlfolder="$emu_dir"
  runtime_root="${emu_dir%/PortMaster}"
fi
export directory="${temp_data_dir#/}"
export PATH="\$runtime_root/bin:\$PATH"
export LD_LIBRARY_PATH="\$runtime_root/lib\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
export SDL_GAMECONTROLLERCONFIG_FILE="\$controlfolder/gamecontrollerdb.txt"
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
cmp "$emu_dir/device_info.txt" "$xdg_data_home/PortMaster/device_info.txt"
cmp "$emu_dir/funcs.txt" "$xdg_data_home/PortMaster/funcs.txt"
cmp "$pak_dir/files/PortMaster.txt" "$xdg_data_home/PortMaster/PortMaster.sh"
cmp "$pak_dir/bin/runtime-helper" "$xdg_data_home/bin/runtime-helper"
cmp "$pak_dir/lib/libruntime-helper.so" "$xdg_data_home/lib/libruntime-helper.so"

first_hash="$(cksum "$emu_dir/control.txt" "$emu_dir/miyoo/control.txt" "$emu_dir/PortMaster.sh" "$emu_dir/miyoo/PortMaster.txt" "$emu_dir/gamecontrollerdb.txt" "$xdg_data_home/PortMaster/control.txt" "$xdg_data_home/bin/runtime-helper" "$xdg_data_home/lib/libruntime-helper.so")"

PAK_DIR="$pak_dir" \
EMU_DIR="$emu_dir" \
TEMP_DATA_DIR="$temp_data_dir" \
XDG_DATA_HOME="$xdg_data_home" \
PMI_LOG_FILE="$log_file" \
PMI_TEST_MODE=overlay-sync \
bash "$pak_dir/Portmaster.sh"

second_hash="$(cksum "$emu_dir/control.txt" "$emu_dir/miyoo/control.txt" "$emu_dir/PortMaster.sh" "$emu_dir/miyoo/PortMaster.txt" "$emu_dir/gamecontrollerdb.txt" "$xdg_data_home/PortMaster/control.txt" "$xdg_data_home/bin/runtime-helper" "$xdg_data_home/lib/libruntime-helper.so")"
[ "$first_hash" = "$second_hash" ]

CONTROL_FILE="$xdg_data_home/PortMaster/control.txt" \
EXPECTED_CONTROL="$xdg_data_home/PortMaster" \
EXPECTED_DB="$xdg_data_home/PortMaster/gamecontrollerdb.txt" \
EXPECTED_BIN="$xdg_data_home/bin" \
EXPECTED_LIB="$xdg_data_home/lib" \
HOME="$root/home" \
XDG_DATA_HOME="$xdg_data_home" \
PMI_LOG_FILE="$log_file" \
bash -c '. "$CONTROL_FILE"; get_controls; [ "$controlfolder" = "$EXPECTED_CONTROL" ]; [ "$SDL_GAMECONTROLLERCONFIG_FILE" = "$EXPECTED_DB" ]; case ":$PATH:" in *":$EXPECTED_BIN:"*) ;; *) exit 1;; esac; case ":$LD_LIBRARY_PATH:" in *":$EXPECTED_LIB:"*) ;; *) exit 1;; esac'
