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

	cat >"$shim_bin/amixer" <<'EOF'
#!/bin/sh
if [ -n "${PMI_TEST_AMIXER_LOG:-}" ]; then
	printf '%s\n' "$*" >>"$PMI_TEST_AMIXER_LOG"
fi
case "${1:-}:${2:-}" in
	"sget:Playback Path")
		printf "Simple mixer control 'Playback Path',0\n  Item0: 'SPK'\n"
		;;
	"sget:SPK")
		printf 'Mono: 2 [100%%]\n'
		;;
esac
exit 0
EOF
	chmod +x "$shim_bin/amixer"

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

	cat >"$pak_dir/bin/pm-port-probe" <<'EOF'
#!/bin/sh
set -eu

mode="${1:-}"
port_dir="${2:-}"

[ "$mode" = "scan-aarch64-launch-port" ] || exit 2

if [ -n "${PMI_TEST_PORT_PROBE_LOG:-}" ]; then
	printf '%s\n' "$port_dir" >>"$PMI_TEST_PORT_PROBE_LOG"
fi

has_gl=0
if [ -d "$port_dir/lib" ] && find "$port_dir/lib" -maxdepth 2 -type f \
	\( -name 'libEGL.so*' -o -name 'libGLES*.so*' -o -name 'libGL.so*' \
	   -o -name 'libgbm.so*' -o -name 'libdrm.so*' -o -name 'libmali.so*' \) \
	| grep -q .; then
	has_gl=1
fi

printf 'PORT\t%s\thas_bundled_native_gl_stack=%s\n' "$port_dir" "$has_gl"

find "$port_dir" -maxdepth 2 -type f | LC_ALL=C sort | while IFS= read -r file || [ -n "$file" ]; do
	[ -n "$file" ] || continue
	case "$file" in
		*.pmi-*|*.sh|*.bash|*.so|*.so.*|*.original)
			continue
			;;
	esac

	is_wrapper=0
	inspect="$file"
	if /bin/head -n 2 "$file" 2>/dev/null | /usr/bin/grep -Eq '^# PMI_AARCH64_(SDL_COMPAT|NATIVE_COMPAT)_WRAPPER=1$'; then
		inspect="${file}.original"
		[ -f "$inspect" ] || continue
		is_wrapper=1
	fi

	header=$(/bin/dd if="$inspect" bs=1 count=5 2>/dev/null | /usr/bin/od -An -tx1 | tr -d ' \n')
	[ "$header" = "7f454c4602" ] || continue

	needs_default_audio_info=0
	needs_pulse_simple=0
	uses_sdl_gl_windowing=0
	if /usr/bin/grep -aq 'SDL_GetDefaultAudioInfo' "$inspect"; then
		needs_default_audio_info=1
	fi
	if /usr/bin/grep -aq 'libpulse-simple.so.0' "$inspect"; then
		needs_pulse_simple=1
	fi
	if /usr/bin/grep -aq 'SDL_GL_CreateContext' "$inspect" && /usr/bin/grep -aq 'SDL_CreateWindow' "$inspect"; then
		uses_sdl_gl_windowing=1
	fi

	printf 'BIN\t%s\tneeds_default_audio_info=%s\tneeds_pulse_simple=%s\tuses_sdl_gl_windowing=%s\tis_wrapper=%s\n' \
		"$file" "$needs_default_audio_info" "$needs_pulse_simple" "$uses_sdl_gl_windowing" "$is_wrapper"
done
EOF
	chmod +x "$pak_dir/bin/pm-port-probe"

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

	printf '\177ELF\001\001\001\000\000\000\000\000\000\000\000\000\002\000\050\000gmloader\n' >"$port_dir/gmloader"
	chmod +x "$port_dir/gmloader"
	printf '\177ELF\001\001\001\000\000\000\000\000\000\000\000\000\002\000\050\000bgdi\n' >"$port_dir/bgdi"
	chmod +x "$port_dir/bgdi"
	printf 'not-elf-data\n' >"$port_dir/SorR.dat"
	chmod +x "$port_dir/SorR.dat"
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

	env \
	PAK_DIR="$pak_dir" \
	EMU_DIR="$emu_dir" \
	ROM_PATH="$rom_dir/$launcher_name" \
	PMI_PORT_SCRIPT="$rom_dir/.ports/$launcher_name" \
	PMI_SDL2_SYSTEM_LIB="${PMI_SDL2_SYSTEM_LIB:-}" \
	PMI_PULSE_SIMPLE_SYSTEM_LIB="${PMI_PULSE_SIMPLE_SYSTEM_LIB:-}" \
	PMI_TEST_PORT_PROBE_LOG="${PMI_TEST_PORT_PROBE_LOG:-}" \
	PMI_TEST_RUN_AARCH64_BINARY="${PMI_TEST_RUN_AARCH64_BINARY:-}" \
	PMI_TEST_AMIXER_LOG="${PMI_TEST_AMIXER_LOG:-}" \
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
	system_pulse_dir="$root/system-pulse"

	mkdir -p "$port_dir" "$pak_dir/runtime/aarch64/lib" "$pak_dir/runtime/aarch64/pulse" "$system_sdl_dir" "$system_pulse_dir"

	cat >"$real_ports_dir/TestAarch64.sh" <<'EOF'
#!/usr/bin/env bash
set -eu
source "$EMU_DIR/control.txt"
GAMEDIR="/$directory/ports/testaarch64"
if [ -n "${PMI_TEST_RUN_AARCH64_BINARY:-}" ]; then
	"$GAMEDIR/$PMI_TEST_RUN_AARCH64_BINARY"
fi
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
	printf '\177ELF\002stub\nlibpulse-simple.so.0\n' >"$port_dir/needs-pulse"
	chmod +x "$port_dir/needs-pulse"
	printf '\177ELF\002stub\nSDL_GetDefaultAudioInfo\nlibpulse-simple.so.0\n' >"$port_dir/needs-both"
	chmod +x "$port_dir/needs-both"

	cat >"$port_dir/not-elf" <<'EOF'
#!/bin/sh
echo SDL_GetDefaultAudioInfo >/dev/null
EOF
	chmod +x "$port_dir/not-elf"
	printf 'skip\n' >"$port_dir/not-elf.pmi-aarch64-sdl-compat"
	printf 'skip\n' >"$port_dir/not-elf.pmi-aarch64-sdl-compat.pmi-aarch64-sdl-compat"

	printf '\177ELF\002stub\nSomeOtherSymbol\n' >"$port_dir/no-sdl"
	chmod +x "$port_dir/no-sdl"
	printf 'legacy-cache\n' >"$port_dir/.pmi-port-probe-v1.tsv"

	printf 'bundled-sdl\n' >"$pak_dir/runtime/aarch64/lib/libSDL2-2.0.so.0"
	printf 'pulse-simple\n' >"$pak_dir/runtime/aarch64/pulse/libpulse-simple.so.0"
	printf 'pulse\n' >"$pak_dir/runtime/aarch64/pulse/libpulse.so.0"
	printf 'pulse-common\n' >"$pak_dir/runtime/aarch64/pulse/libpulsecommon-13.99.so"
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
test -f "$success_root/Roms/Ports (PORTS)/.ports/testport/bgdi"
test -f "$success_root/Roms/Ports (PORTS)/.ports/testport/bgdi.original"
test -f "$success_root/Roms/Ports (PORTS)/.ports/testport/SorR.dat"
test ! -f "$success_root/Roms/Ports (PORTS)/.ports/testport/SorR.dat.original"
grep -q '^# PMI_ARMHF_EXEC_COMPAT_WRAPPER=1$' "$success_root/Roms/Ports (PORTS)/.ports/testport/gmloader"
grep -q 'REAL_BINARY="\$SELF_DIR/\${SELF_NAME}\.original"' "$success_root/Roms/Ports (PORTS)/.ports/testport/gmloader"
grep -q '^# PMI_ARMHF_EXEC_COMPAT_WRAPPER=1$' "$success_root/Roms/Ports (PORTS)/.ports/testport/bgdi"
grep -q 'REAL_BINARY="\$SELF_DIR/\${SELF_NAME}\.original"' "$success_root/Roms/Ports (PORTS)/.ports/testport/bgdi"

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
PMI_TEST_PORT_PROBE_LOG="$aarch64_wrap_root/port-probe.log" \
run_direct_launch_for_script "$aarch64_wrap_root" "TestAarch64.sh"
test -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl.original"
test -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-pulse.original"
test -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-both.original"
grep -q '# PMI_AARCH64_NATIVE_COMPAT_WRAPPER=1' "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl"
grep -q 'REAL_BINARY="\$SELF_DIR/\${SELF_NAME}\.original"' "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl"
grep -q '^PMI_USE_AARCH64_SDL_COMPAT="1"$' "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl"
grep -q '^PMI_USE_AARCH64_PULSE_COMPAT="0"$' "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl"
grep -q '^PMI_USE_AARCH64_SDL_COMPAT="0"$' "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-pulse"
grep -q '^PMI_USE_AARCH64_PULSE_COMPAT="1"$' "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-pulse"
grep -q '^PMI_USE_AARCH64_SDL_COMPAT="1"$' "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-both"
grep -q '^PMI_USE_AARCH64_PULSE_COMPAT="1"$' "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-both"
test -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/not-elf"
test ! -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/not-elf.original"
test -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/no-sdl"
test ! -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/no-sdl.original"
test -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/.pmi-port-probe-v1.tsv"
test -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/.pmi-port-probe-v2.tsv"
test ! -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/not-elf.pmi-aarch64-sdl-compat"
test ! -f "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/not-elf.pmi-aarch64-sdl-compat.pmi-aarch64-sdl-compat"
test "$(wc -l < "$aarch64_wrap_root/port-probe.log" | tr -d ' ')" = "2"
grep -q "PMI_DIAG aarch64_native_compat_applied=$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl sdl=1 pulse=0" "$aarch64_wrap_root/PORTS.txt"
grep -q "PMI_DIAG aarch64_native_compat_applied=$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-pulse sdl=0 pulse=1" "$aarch64_wrap_root/PORTS.txt"
grep -q "PMI_DIAG aarch64_native_compat_applied=$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-both sdl=1 pulse=1" "$aarch64_wrap_root/PORTS.txt"
PMI_SDL2_SYSTEM_LIB="$aarch64_wrap_root/system-sdl/libSDL2-2.0.so.0" \
PMI_TEST_PORT_PROBE_LOG="$aarch64_wrap_root/port-probe.log" \
run_direct_launch_for_script "$aarch64_wrap_root" "TestAarch64.sh"
test "$(wc -l < "$aarch64_wrap_root/port-probe.log" | tr -d ' ')" = "2"

cat >"$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-both.original" <<EOF
#!/bin/sh
printf '%s\n' "\$LD_LIBRARY_PATH" >"$aarch64_wrap_root/.ports_temp/needs-both-ld.txt"
exit 0
EOF
chmod +x "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-both.original"
mkdir -p "$aarch64_wrap_root/.ports_temp/ports"
rm -rf "$aarch64_wrap_root/.ports_temp/ports/testaarch64"
cp -R "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64" "$aarch64_wrap_root/.ports_temp/ports/testaarch64"
: >"$aarch64_wrap_root/amixer.log"
PMI_SDL2_SYSTEM_LIB="$aarch64_wrap_root/system-sdl/libSDL2-2.0.so.0" \
PMI_TEST_RUN_AARCH64_BINARY="needs-both" \
PMI_TEST_AMIXER_LOG="$aarch64_wrap_root/amixer.log" \
run_direct_launch_for_script "$aarch64_wrap_root" "TestAarch64.sh"
unset PMI_TEST_RUN_AARCH64_BINARY PMI_TEST_AMIXER_LOG
grep -q "^$aarch64_wrap_root/Emus/my355/PORTS.pak/runtime/aarch64/lib:$aarch64_wrap_root/Emus/my355/PORTS.pak/runtime/aarch64/pulse:$aarch64_wrap_root/Emus/my355/PORTS.pak/lib" "$aarch64_wrap_root/.ports_temp/needs-both-ld.txt"
grep -q '^sget Playback Path$' "$aarch64_wrap_root/amixer.log"
grep -q '^sget SPK$' "$aarch64_wrap_root/amixer.log"
grep -q '^sset Playback Path OFF$' "$aarch64_wrap_root/amixer.log"
grep -q '^sset Playback Path SPK$' "$aarch64_wrap_root/amixer.log"
grep -q '^sset Speaker on$' "$aarch64_wrap_root/amixer.log"
grep -q '^sset Headphone on$' "$aarch64_wrap_root/amixer.log"
grep -q '^sset SPK 2$' "$aarch64_wrap_root/amixer.log"

cat >"$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl.original" <<EOF
#!/bin/sh
printf '%s\n' "\$LD_LIBRARY_PATH" >"$aarch64_wrap_root/.ports_temp/needs-sdl-ld.txt"
exit 0
EOF
chmod +x "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl.original"
rm -rf "$aarch64_wrap_root/.ports_temp/ports/testaarch64"
cp -R "$aarch64_wrap_root/Roms/Ports (PORTS)/.ports/testaarch64" "$aarch64_wrap_root/.ports_temp/ports/testaarch64"
: >"$aarch64_wrap_root/amixer.log"
PMI_SDL2_SYSTEM_LIB="$aarch64_wrap_root/system-sdl/libSDL2-2.0.so.0" \
PMI_TEST_RUN_AARCH64_BINARY="needs-sdl" \
PMI_TEST_AMIXER_LOG="$aarch64_wrap_root/amixer.log" \
run_direct_launch_for_script "$aarch64_wrap_root" "TestAarch64.sh"
unset PMI_TEST_RUN_AARCH64_BINARY PMI_TEST_AMIXER_LOG
grep -q "^$aarch64_wrap_root/Emus/my355/PORTS.pak/runtime/aarch64/lib:$aarch64_wrap_root/Emus/my355/PORTS.pak/lib" "$aarch64_wrap_root/.ports_temp/needs-sdl-ld.txt"
test ! -s "$aarch64_wrap_root/amixer.log"

create_direct_launch_tree "$aarch64_skip_root"
add_aarch64_sdl_fixture "$aarch64_skip_root"
printf 'SDL_GetDefaultAudioInfo\n' >"$aarch64_skip_root/system-sdl/libSDL2-2.0.so.0"
printf 'pulse-present\n' >"$aarch64_skip_root/system-pulse/libpulse-simple.so.0"
PMI_SDL2_SYSTEM_LIB="$aarch64_skip_root/system-sdl/libSDL2-2.0.so.0" \
PMI_PULSE_SIMPLE_SYSTEM_LIB="$aarch64_skip_root/system-pulse/libpulse-simple.so.0" \
PMI_TEST_PORT_PROBE_LOG="$aarch64_skip_root/port-probe.log" \
run_direct_launch_for_script "$aarch64_skip_root" "TestAarch64.sh"
test -f "$aarch64_skip_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl"
test ! -f "$aarch64_skip_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-sdl.original"
test -f "$aarch64_skip_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-pulse"
test ! -f "$aarch64_skip_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-pulse.original"
test -f "$aarch64_skip_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-both"
test ! -f "$aarch64_skip_root/Roms/Ports (PORTS)/.ports/testaarch64/needs-both.original"
! grep -q 'PMI_DIAG aarch64_native_compat_applied=' "$aarch64_skip_root/PORTS.txt"
test ! -f "$aarch64_skip_root/Roms/Ports (PORTS)/.ports/testaarch64/not-elf.pmi-aarch64-sdl-compat"
test "$(wc -l < "$aarch64_skip_root/port-probe.log" | tr -d ' ')" = "1"

create_direct_launch_tree "$native_gl_root"
add_native_gl_fixture "$native_gl_root" "TestNativeGL.sh" "nativegl" "native-gl-ld.txt" 0
PMI_SDL2_SYSTEM_LIB='' PMI_TEST_PORT_PROBE_LOG="$native_gl_root/port-probe.log" run_direct_launch_for_script "$native_gl_root" "TestNativeGL.sh"
grep -q "PMI_DIAG system_gl_stack_launcher=$native_gl_root/Roms/Ports (PORTS)/.ports/TestNativeGL.sh" "$native_gl_root/PORTS.txt"
grep -q "^$native_gl_root/.ports_temp/ports/nativegl/lib/libarm64:/usr/lib:/usr/trimui/lib$" "$native_gl_root/.ports_temp/native-gl-ld.txt"
test "$(wc -l < "$native_gl_root/port-probe.log" | tr -d ' ')" = "1"

create_direct_launch_tree "$bundled_gl_root"
add_native_gl_fixture "$bundled_gl_root" "TestBundledGL.sh" "bundledgl" "bundled-gl-ld.txt" 1
PMI_SDL2_SYSTEM_LIB='' PMI_TEST_PORT_PROBE_LOG="$bundled_gl_root/port-probe.log" run_direct_launch_for_script "$bundled_gl_root" "TestBundledGL.sh"
! grep -q 'PMI_DIAG system_gl_stack_launcher=' "$bundled_gl_root/PORTS.txt"
grep -q "^$bundled_gl_root/.ports_temp/ports/bundledgl/lib/libarm64:$bundled_gl_root/Emus/my355/PORTS.pak/lib$" "$bundled_gl_root/.ports_temp/bundled-gl-ld.txt"
test "$(wc -l < "$bundled_gl_root/port-probe.log" | tr -d ' ')" = "1"
