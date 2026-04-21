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

	cat >"$shim_bin/strings" <<'EOF'
#!/bin/sh
exec /usr/bin/strings "$@"
EOF
	chmod +x "$shim_bin/strings"

	cat >"$pak_dir/bin/pm-sdl-compat-check" <<'EOF'
#!/bin/sh
set -eu

mode="needs-default-audio-info"
file="$1"
if [ "$#" -ge 2 ]; then
	mode="$1"
	file="$2"
fi
if [ -n "${PMI_TEST_SDL_COMPAT_CHECK_LOG:-}" ]; then
	printf '%s:%s\n' "$mode" "$file" >>"$PMI_TEST_SDL_COMPAT_CHECK_LOG"
fi

header=$(/bin/dd if="$file" bs=1 count=5 2>/dev/null | /usr/bin/od -An -tx1 | tr -d ' \n')
[ "$header" = "7f454c4602" ] || exit 1
case "$mode" in
	needs-default-audio-info)
		/usr/bin/grep -aq 'SDL_GetDefaultAudioInfo' "$file"
		;;
	uses-sdl-gl-windowing)
		/usr/bin/grep -aq 'SDL_GL_CreateContext' "$file"
		/usr/bin/grep -aq 'SDL_CreateWindow' "$file"
		;;
	*)
		exit 2
		;;
esac
EOF
	chmod +x "$pak_dir/bin/pm-sdl-compat-check"

	cat >"$pak_dir/files/control.txt" <<EOF
#!/bin/bash
CUR_TTY=/dev/null
XDG_DATA_HOME="\${XDG_DATA_HOME:-\$HOME/.local/share}"
runtime_root="$emu_dir"
export controlfolder="\$runtime_root"
export directory="${temp_data_dir#/}"
export PATH="\${runtime_root%/PortMaster}/bin:\$PATH"
if [ "\${PMI_LD_LIBRARY_STRATEGY:-}" = "system-gl" ]; then
  export LD_LIBRARY_PATH="/usr/lib:/usr/trimui/lib"
else
  export LD_LIBRARY_PATH="\${runtime_root%/PortMaster}/lib\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
fi
export ESUDO=""
export ESUDOKILL="-1"
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
	port_dir="$real_ports_dir/testport"
	temp_data_dir="$root/.ports_temp"

	create_common_tree "$root"
	write_success_lib_bundle "$root"
	mkdir -p "$rom_dir" "$real_ports_dir" "$port_dir"

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

	cat >"$port_dir/port.json" <<'EOF'
{
  "attr": {
    "arch": ["armhf"]
  },
  "files": {
    "TestPort.sh": "TestPort.sh",
    "testport/": "testport/"
  }
}
EOF

	cat >"$port_dir/gmloader" <<'EOF'
#!/bin/sh
exit 0
EOF
	chmod +x "$port_dir/gmloader"
}

run_direct_launch_for_script() {
	root="$1"
	launcher_name="$2"
	pak_dir="$root/Emus/my355/PORTS.pak"
	emu_dir="$pak_dir/PortMaster"
	shim_bin="$root/test-bin"
	xdg_data_home="$root/home/.local/share"
	temp_data_dir="$root/.ports_temp"
	log_file="$root/PORTS.txt"
	rom_dir="$root/Roms/Ports (PORTS)"

	PAK_DIR="$pak_dir" \
	EMU_DIR="$emu_dir" \
	ROM_PATH="$rom_dir/$launcher_name" \
	PMI_PORT_SCRIPT="$rom_dir/.ports/$launcher_name" \
	PMI_SDL2_SYSTEM_LIB="${PMI_SDL2_SYSTEM_LIB:-}" \
	PMI_TEST_SDL_COMPAT_CHECK_LOG="${PMI_TEST_SDL_COMPAT_CHECK_LOG:-}" \
	TEMP_DATA_DIR="$temp_data_dir" \
	XDG_DATA_HOME="$xdg_data_home" \
	PMI_LOG_FILE="$log_file" \
	PATH="$shim_bin:$PATH" \
	bash "$pak_dir/Portmaster.sh"
}

run_direct_launch() {
	run_direct_launch_for_script "$1" "TestPort.sh"
}

add_aarch64_sdl_fixture() {
	root="$1"
	real_ports_dir="$root/Roms/Ports (PORTS)/.ports"
	port_dir="$real_ports_dir/testaarch64"
	pak_dir="$root/Emus/my355/PORTS.pak"
	system_sdl_dir="$root/system-sdl"

	mkdir -p "$port_dir" "$pak_dir/runtime/aarch64/lib" "$system_sdl_dir"

	cat >"$real_ports_dir/TestAarch64.sh" <<'EOF'
#!/bin/sh
exit 0
EOF
	chmod +x "$real_ports_dir/TestAarch64.sh"

	cat >"$port_dir/port.json" <<'EOF'
{
  "attr": {
    "arch": ["aarch64"]
  },
  "files": {
    "TestAarch64.sh": "TestAarch64.sh",
    "testaarch64/": "testaarch64/"
  }
}
EOF

	printf '\177ELF\002stub\nSDL_GetDefaultAudioInfo\n' >"$port_dir/needs-sdl"
	chmod +x "$port_dir/needs-sdl"

	cat >"$port_dir/not-elf" <<'EOF'
#!/bin/sh
echo SDL_GetDefaultAudioInfo >/dev/null
EOF
	chmod +x "$port_dir/not-elf"

	printf '\177ELF\002stub\nSomeOtherSymbol\n' >"$port_dir/no-sdl"
	chmod +x "$port_dir/no-sdl"

	printf 'bundled-sdl\n' >"$pak_dir/runtime/aarch64/lib/libSDL2-2.0.so.0"
}

add_native_gl_fixture() {
	root="$1"
	launcher_name="$2"
	port_name="$3"
	output_name="$4"
	with_bundled_gl="${5:-0}"
	real_ports_dir="$root/Roms/Ports (PORTS)/.ports"
	port_dir="$real_ports_dir/$port_name"

	mkdir -p "$port_dir/lib/libarm64"

	cat >"$real_ports_dir/$launcher_name" <<EOF
#!/usr/bin/env bash
set -eu
source "\$EMU_DIR/control.txt"
LIB_DIR="/\$directory/ports/$port_name/lib/libarm64"
export LD_LIBRARY_PATH="\$LIB_DIR:\$LD_LIBRARY_PATH"
printf '%s\n' "\$LD_LIBRARY_PATH" >"\$TEMP_DATA_DIR/$output_name"
EOF
	chmod +x "$real_ports_dir/$launcher_name"

	cat >"$port_dir/port.json" <<EOF
{
  "attr": {
    "arch": ["aarch64"]
  },
  "files": {
    "$launcher_name": "$launcher_name",
    "$port_name/": "$port_name/"
  }
}
EOF

	printf '\177ELF\002stub\nSDL_GL_CreateContext\nSDL_CreateWindow\n' >"$port_dir/gl-port"
	chmod +x "$port_dir/gl-port"

	if [ "$with_bundled_gl" = "1" ]; then
		printf 'egl\n' >"$port_dir/lib/libarm64/libEGL.so.1"
	fi
}

failure_root="$(mktemp -d)"
success_root="$(mktemp -d)"
joy_override_root="$(mktemp -d)"
joy_passthrough_root="$(mktemp -d)"
joy_readonly_root="$(mktemp -d)"
aarch64_wrap_root="$(mktemp -d)"
aarch64_skip_root="$(mktemp -d)"
native_gl_root="$(mktemp -d)"
bundled_gl_root="$(mktemp -d)"
trap 'rm -rf "$failure_root" "$success_root" "$joy_override_root" "$joy_passthrough_root" "$joy_readonly_root" "$aarch64_wrap_root" "$aarch64_skip_root" "$native_gl_root" "$bundled_gl_root"' EXIT

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
test -f "$success_root/Roms/Ports (PORTS)/.ports/testport/gmloader"
test -f "$success_root/Roms/Ports (PORTS)/.ports/testport/gmloader.original"
grep -q 'REAL_BINARY="\$SELF_DIR/\${SELF_NAME}\.original"' "$success_root/Roms/Ports (PORTS)/.ports/testport/gmloader"

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

create_direct_launch_tree "$aarch64_wrap_root"
add_aarch64_sdl_fixture "$aarch64_wrap_root"
printf 'system-sdl-without-symbol\n' >"$aarch64_wrap_root/system-sdl/libSDL2-2.0.so.0"
PMI_SDL2_SYSTEM_LIB="$aarch64_wrap_root/system-sdl/libSDL2-2.0.so.0" \
PMI_TEST_SDL_COMPAT_CHECK_LOG="$aarch64_wrap_root/sdl-check.log" \
run_direct_launch_for_script "$aarch64_wrap_root" "TestAarch64.sh"
test -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl.original"
grep -q '# PMI_AARCH64_SDL_COMPAT_WRAPPER=1' "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl"
grep -q 'REAL_BINARY="\$SELF_DIR/\${SELF_NAME}\.original"' "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl"
test -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/not-elf"
test ! -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/not-elf.original"
test -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/no-sdl"
test ! -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/no-sdl.original"
test -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl.pmi-aarch64-sdl-compat"
test -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/not-elf.pmi-aarch64-sdl-compat"
test -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/no-sdl.pmi-aarch64-sdl-compat"
test "$(wc -l < "$aarch64_wrap_root/sdl-check.log" | tr -d ' ')" = "5"
grep -q "PMI_DIAG aarch64_sdl_compat_applied=$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl" "$aarch64_wrap_root/PORTS.txt"
PMI_SDL2_SYSTEM_LIB="$aarch64_wrap_root/system-sdl/libSDL2-2.0.so.0" \
PMI_TEST_SDL_COMPAT_CHECK_LOG="$aarch64_wrap_root/sdl-check.log" \
run_direct_launch_for_script "$aarch64_wrap_root" "TestAarch64.sh"
test "$(wc -l < "$aarch64_wrap_root/sdl-check.log" | tr -d ' ')" = "5"

create_direct_launch_tree "$aarch64_skip_root"
add_aarch64_sdl_fixture "$aarch64_skip_root"
printf 'SDL_GetDefaultAudioInfo\n' >"$aarch64_skip_root/system-sdl/libSDL2-2.0.so.0"
PMI_SDL2_SYSTEM_LIB="$aarch64_skip_root/system-sdl/libSDL2-2.0.so.0" \
PMI_TEST_SDL_COMPAT_CHECK_LOG="$aarch64_skip_root/sdl-check.log" \
run_direct_launch_for_script "$aarch64_skip_root" "TestAarch64.sh"
test -f "$aarch64_skip_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl"
test ! -f "$aarch64_skip_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl.original"
! grep -q 'PMI_DIAG aarch64_sdl_compat_applied=' "$aarch64_skip_root/PORTS.txt"
test "$(wc -l < "$aarch64_skip_root/sdl-check.log" | tr -d ' ')" = "3"
grep -q "^uses-sdl-gl-windowing:$aarch64_skip_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl$" "$aarch64_skip_root/sdl-check.log"

create_direct_launch_tree "$native_gl_root"
add_native_gl_fixture "$native_gl_root" "TestNativeGL.sh" "nativegl" "native-gl-ld.txt" 0
PMI_SDL2_SYSTEM_LIB='' PMI_TEST_SDL_COMPAT_CHECK_LOG="$native_gl_root/gl-check.log" run_direct_launch_for_script "$native_gl_root" "TestNativeGL.sh"
grep -q "PMI_DIAG system_gl_stack_launcher=$native_gl_root/Roms/Ports (PORTS)/.ports/TestNativeGL.sh" "$native_gl_root/PORTS.txt"
grep -q "^$native_gl_root/.ports_temp/ports/nativegl/lib/libarm64:/usr/lib:/usr/trimui/lib$" "$native_gl_root/.ports_temp/native-gl-ld.txt"
grep -q "^uses-sdl-gl-windowing:$native_gl_root/Roms/Ports (PORTS)/.ports/nativegl/gl-port$" "$native_gl_root/gl-check.log"

create_direct_launch_tree "$bundled_gl_root"
add_native_gl_fixture "$bundled_gl_root" "TestBundledGL.sh" "bundledgl" "bundled-gl-ld.txt" 1
PMI_SDL2_SYSTEM_LIB='' PMI_TEST_SDL_COMPAT_CHECK_LOG="$bundled_gl_root/gl-check.log" run_direct_launch_for_script "$bundled_gl_root" "TestBundledGL.sh"
! grep -q 'PMI_DIAG system_gl_stack_launcher=' "$bundled_gl_root/PORTS.txt"
grep -q "^$bundled_gl_root/.ports_temp/ports/bundledgl/lib/libarm64:$bundled_gl_root/Emus/my355/PORTS.pak/lib$" "$bundled_gl_root/.ports_temp/bundled-gl-ld.txt"
test "$(wc -l < "$bundled_gl_root/gl-check.log" | tr -d ' ')" = "1"
grep -q "^needs-default-audio-info:$bundled_gl_root/Roms/Ports (PORTS)/.ports/bundledgl/gl-port$" "$bundled_gl_root/gl-check.log"
