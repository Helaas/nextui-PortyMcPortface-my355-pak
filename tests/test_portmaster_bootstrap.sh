#!/bin/sh
set -eu

create_common_tree() {
	root="$1"
	pak_dir="$root/Emus/my355/PORTS.pak"
	emu_dir="$pak_dir/PortMaster"
	shim_bin="$root/test-bin"
	xdg_data_home="$root/home/.local/share"
	temp_data_dir="$root/.ports_temp"
	log_file="$root/PORTS.txt"

	mkdir -p "$pak_dir/files" "$pak_dir/bin" "$pak_dir/lib" "$emu_dir/pylibs/harbourmaster" \
		"$emu_dir/miyoo" "$xdg_data_home/PortMaster" "$temp_data_dir" "$shim_bin"
	cp payload/PORTS.pak/Portmaster.sh "$pak_dir/Portmaster.sh"
	printf '#!/bin/sh\nexit 0\n' >"$pak_dir/bin/python3"
	chmod +x "$pak_dir/bin/python3"
	cat >"$shim_bin/sed" <<'EOF'
#!/bin/sh
if [ "${1:-}" = "-i" ]; then
	shift
	if /usr/bin/sed --version >/dev/null 2>&1; then
		exec /usr/bin/sed -i "$@"
	else
		exec /usr/bin/sed -i '' "$@"
	fi
fi
exec /usr/bin/sed "$@"
EOF
	chmod +x "$shim_bin/sed"

	cat >"$pak_dir/files/control.txt" <<EOF
#!/bin/bash
XDG_DATA_HOME="\${XDG_DATA_HOME:-\$HOME/.local/share}"
if [ -d "\$XDG_DATA_HOME/PortMaster" ]; then
  export controlfolder="\$XDG_DATA_HOME/PortMaster"
else
  export controlfolder="$emu_dir"
fi
export directory="${temp_data_dir#/}"
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

	cat >"$pak_dir/files/config.json" <<'EOF'
{}
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
}

run_overlay_sync() {
	root="$1"
	pak_dir="$root/Emus/my355/PORTS.pak"
	emu_dir="$pak_dir/PortMaster"
	shim_bin="$root/test-bin"
	xdg_data_home="$root/home/.local/share"
	temp_data_dir="$root/.ports_temp"
	log_file="$root/PORTS.txt"

	PAK_DIR="$pak_dir" \
	EMU_DIR="$emu_dir" \
	TEMP_DATA_DIR="$temp_data_dir" \
	XDG_DATA_HOME="$xdg_data_home" \
	PMI_LOG_FILE="$log_file" \
	PATH="$shim_bin:$PATH" \
	PMI_TEST_MODE=overlay-sync \
	bash "$pak_dir/Portmaster.sh"
}

write_success_lib_bundle() {
	root="$1"
	archive_root="$root/lib-archive"

	rm -rf "$archive_root"
	mkdir -p "$archive_root/python3.11"
	printf 'module\n' >"$archive_root/python3.11/module.py"
	printf 'theora\n' >"$archive_root/libtheoradec.so.1"
	printf 'pulse\n' >"$archive_root/libpulse.so.0"
	printf 'pulse-simple\n' >"$archive_root/libpulse-simple.so.0"
	printf 'pulse-common\n' >"$archive_root/libpulsecommon-13.99.so"
	printf 'junk\n' >"$archive_root/._junk"
	tar -czf "$root/Emus/my355/PORTS.pak/files/lib.tar.gz" -C "$archive_root" .
}

create_direct_launch_tree() {
	root="$1"
	rom_dir="$root/Roms/Ports (PORTS)"
	real_ports_dir="$rom_dir/.ports"
	temp_data_dir="$root/.ports_temp"

	create_common_tree "$root"
	write_success_lib_bundle "$root"
	mkdir -p "$rom_dir" "$real_ports_dir"

	cat >"$rom_dir/TestPort.sh" <<'EOF'
#!/bin/sh
exit 0
EOF
	chmod +x "$rom_dir/TestPort.sh"

	cat >"$real_ports_dir/TestPort.sh" <<EOF
#!/usr/bin/env bash
set -eu
if [ -n "\${PMI_JOY_TYPE_NODE:-}" ]; then
	cat "\$PMI_JOY_TYPE_NODE" >"$temp_data_dir/joy_type_during.txt"
fi
EOF
	chmod +x "$real_ports_dir/TestPort.sh"
}

run_direct_launch() {
	root="$1"
	pak_dir="$root/Emus/my355/PORTS.pak"
	emu_dir="$pak_dir/PortMaster"
	shim_bin="$root/test-bin"
	xdg_data_home="$root/home/.local/share"
	temp_data_dir="$root/.ports_temp"
	log_file="$root/PORTS.txt"
	rom_dir="$root/Roms/Ports (PORTS)"

	PAK_DIR="$pak_dir" \
	EMU_DIR="$emu_dir" \
	ROM_PATH="$rom_dir/TestPort.sh" \
	PMI_PORT_SCRIPT="$rom_dir/.ports/TestPort.sh" \
	TEMP_DATA_DIR="$temp_data_dir" \
	XDG_DATA_HOME="$xdg_data_home" \
	PMI_LOG_FILE="$log_file" \
	PATH="$shim_bin:$PATH" \
	bash "$pak_dir/Portmaster.sh"
}

failure_root="$(mktemp -d)"
success_root="$(mktemp -d)"
joy_override_root="$(mktemp -d)"
joy_passthrough_root="$(mktemp -d)"
joy_readonly_root="$(mktemp -d)"
trap 'rm -rf "$failure_root" "$success_root" "$joy_override_root" "$joy_passthrough_root" "$joy_readonly_root"' EXIT

create_direct_launch_tree "$failure_root"
printf 'not-a-tarball\n' >"$failure_root/Emus/my355/PORTS.pak/files/lib.tar.gz"
if run_direct_launch "$failure_root"; then
	exit 1
fi
test ! -f "$failure_root/Emus/my355/PORTS.pak/lib/.portmaster-lib-bundle.stamp"
test ! -e "$failure_root/Emus/my355/PORTS.pak/lib/libtheoradec.so.1"

create_direct_launch_tree "$success_root"
run_direct_launch "$success_root"
test -f "$success_root/Emus/my355/PORTS.pak/lib/.portmaster-lib-bundle.stamp"
test -d "$success_root/Emus/my355/PORTS.pak/lib/python3.11"
test -f "$success_root/Emus/my355/PORTS.pak/lib/python3.11/module.py"
test -f "$success_root/Emus/my355/PORTS.pak/lib/libtheoradec.so.1"
test ! -f "$success_root/Emus/my355/PORTS.pak/lib/libpulse.so.0"
test ! -f "$success_root/Emus/my355/PORTS.pak/lib/libpulse-simple.so.0"
test ! -f "$success_root/Emus/my355/PORTS.pak/lib/libpulsecommon-13.99.so"
test ! -f "$success_root/Emus/my355/PORTS.pak/lib/._junk"
test ! -f "$success_root/Emus/my355/PORTS.pak/files/lib.tar.gz"

create_direct_launch_tree "$joy_override_root"
printf '0 [-1=none 0=miyoo 1=xbox]\n' >"$joy_override_root/joy_type"
PMI_JOY_TYPE_NODE="$joy_override_root/joy_type" run_direct_launch "$joy_override_root"
grep -q 'PMI_DIAG joy_type_original=0' "$joy_override_root/PORTS.txt"
grep -q 'PMI_DIAG joy_type_override=-1' "$joy_override_root/PORTS.txt"
grep -q 'PMI_DIAG joy_type_restored=0' "$joy_override_root/PORTS.txt"
test "$(cat "$joy_override_root/.ports_temp/joy_type_during.txt")" = "-1"
test "$(cat "$joy_override_root/joy_type")" = "0"

create_direct_launch_tree "$joy_passthrough_root"
printf '%s\n' '-1 [-1=none 0=miyoo 1=xbox]' >"$joy_passthrough_root/joy_type"
PMI_JOY_TYPE_NODE="$joy_passthrough_root/joy_type" run_direct_launch "$joy_passthrough_root"
grep -q 'PMI_DIAG joy_type_original=-1' "$joy_passthrough_root/PORTS.txt"
! grep -q 'PMI_DIAG joy_type_override=-1' "$joy_passthrough_root/PORTS.txt"
! grep -q 'PMI_DIAG joy_type_restored=' "$joy_passthrough_root/PORTS.txt"
test "$(sed 's/ .*//' "$joy_passthrough_root/.ports_temp/joy_type_during.txt")" = "-1"
test "$(sed 's/ .*//' "$joy_passthrough_root/joy_type")" = "-1"

create_direct_launch_tree "$joy_readonly_root"
printf '0 [-1=none 0=miyoo 1=xbox]\n' >"$joy_readonly_root/joy_type"
chmod 444 "$joy_readonly_root/joy_type"
PMI_JOY_TYPE_NODE="$joy_readonly_root/joy_type" run_direct_launch "$joy_readonly_root"
grep -q "PMI_WARN joy_type_node_not_writable=$joy_readonly_root/joy_type" "$joy_readonly_root/PORTS.txt"
test "$(sed 's/ .*//' "$joy_readonly_root/.ports_temp/joy_type_during.txt")" = "0"
test "$(sed 's/ .*//' "$joy_readonly_root/joy_type")" = "0"
