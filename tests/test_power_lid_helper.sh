#!/bin/sh
set -eu

root="$(mktemp -d)"
trap 'rm -rf "$root"; rm -f /tmp/pmi-power-lid-watch.pid' EXIT
rm -f /tmp/pmi-power-lid-watch.pid

events_file="$root/power-lid-helper.events"
run_marker="$root/run.marker"

create_common_tree() {
	root_dir="$1"
	pak_dir="$root_dir/Emus/my355/PORTS.pak"
	emu_dir="$pak_dir/PortMaster"
	temp_data_dir="$root_dir/.ports_temp"
	xdg_data_home="$root_dir/home/.local/share"
	shim_bin="$root_dir/test-bin"

	mkdir -p \
		"$pak_dir/bin" \
		"$pak_dir/files" \
		"$pak_dir/lib/python3.11" \
		"$pak_dir/.ports" \
		"$emu_dir/pylibs/harbourmaster" \
		"$emu_dir/miyoo" \
		"$shim_bin" \
		"$xdg_data_home" \
		"$temp_data_dir"

	cp payload/PORTS.pak/Portmaster.sh "$pak_dir/Portmaster.sh"

	cat >"$pak_dir/bin/pm-power-lid-watch" <<'EOF'
#!/bin/sh
set -eu
printf 'start:%s:%s\n' "${PMI_TEST_FLOW:-unknown}" "$$" >>"$PMI_TEST_POWER_HELPER_EVENTS"
trap 'printf "stop:%s:%s\n" "${PMI_TEST_FLOW:-unknown}" "$$" >>"$PMI_TEST_POWER_HELPER_EVENTS"; exit 0' TERM INT HUP QUIT
while :; do
	sleep 1
done
EOF
	chmod +x "$pak_dir/bin/pm-power-lid-watch"

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

	printf '#!/bin/sh\nexit 0\n' >"$pak_dir/bin/python3"
	chmod +x "$pak_dir/bin/python3"
	printf 'stamp\n' >"$pak_dir/lib/.portmaster-lib-bundle.stamp"
	printf 'module\n' >"$pak_dir/lib/python3.11/module.py"
	printf 'theora\n' >"$pak_dir/lib/libtheoradec.so.1"

	cat >"$pak_dir/files/control.txt" <<'EOF'
#!/bin/sh
exit 0
EOF
	cat >"$pak_dir/files/PortMaster.txt" <<'EOF'
#!/bin/sh
exit 0
EOF
	cat >"$pak_dir/files/config.py" <<'EOF'
# test
EOF
	cat >"$pak_dir/files/config.json" <<'EOF'
{}
EOF
	cat >"$pak_dir/files/gamecontrollerdb.txt" <<'EOF'
stub
EOF

	cat >"$emu_dir/device_info.txt" <<'EOF'
ANALOG_STICKS=2
EOF
	cat >"$emu_dir/funcs.txt" <<'EOF'
#!/bin/sh
EOF
	cat >"$emu_dir/pylibs/harbourmaster/config.py" <<'EOF'
# test
EOF
	cat >"$emu_dir/pugwash" <<'EOF'
#!/bin/sh
set -eu
printf 'gui\n' >>"$PMI_TEST_RUN_MARKER"
exit 0
EOF
	chmod +x "$emu_dir/pugwash"
}

run_gui_flow() {
	root_dir="$1"
	pak_dir="$root_dir/Emus/my355/PORTS.pak"
	emu_dir="$pak_dir/PortMaster"
	temp_data_dir="$root_dir/.ports_temp"
	log_file="$root_dir/PORTS.txt"
	xdg_data_home="$root_dir/home/.local/share"
	shim_bin="$root_dir/test-bin"

	PMI_TEST_FLOW=gui \
	PMI_TEST_POWER_HELPER_EVENTS="$events_file" \
	PMI_TEST_RUN_MARKER="$run_marker" \
	PAK_DIR="$pak_dir" \
	EMU_DIR="$emu_dir" \
	HOME="$root_dir/home" \
	XDG_DATA_HOME="$xdg_data_home" \
	PATH="$shim_bin:$PATH" \
	TEMP_DATA_DIR="$temp_data_dir" \
	PMI_LOG_FILE="$log_file" \
	bash "$pak_dir/Portmaster.sh"
}

run_port_flow() {
	root_dir="$1"
	pak_dir="$root_dir/Emus/my355/PORTS.pak"
	emu_dir="$pak_dir/PortMaster"
	temp_data_dir="$root_dir/.ports_temp"
	log_file="$root_dir/PORTS.txt"
	rom_dir="$root_dir/Roms/Ports (PORTS)"
	real_ports_dir="$rom_dir/.ports"
	xdg_data_home="$root_dir/home/.local/share"
	shim_bin="$root_dir/test-bin"

	mkdir -p "$real_ports_dir"
	cat >"$rom_dir/TestPort.sh" <<'EOF'
#!/bin/sh
exit 0
EOF
	chmod +x "$rom_dir/TestPort.sh"
	cat >"$real_ports_dir/TestPort.sh" <<'EOF'
#!/bin/sh
set -eu
printf 'port\n' >>"$PMI_TEST_RUN_MARKER"
exit 0
EOF
	chmod +x "$real_ports_dir/TestPort.sh"

	PMI_TEST_FLOW=port \
	PMI_TEST_POWER_HELPER_EVENTS="$events_file" \
	PMI_TEST_RUN_MARKER="$run_marker" \
	PMI_PORT_SCRIPT="$real_ports_dir/TestPort.sh" \
	PAK_DIR="$pak_dir" \
	EMU_DIR="$emu_dir" \
	HOME="$root_dir/home" \
	XDG_DATA_HOME="$xdg_data_home" \
	PATH="$shim_bin:$PATH" \
	TEMP_DATA_DIR="$temp_data_dir" \
	PMI_LOG_FILE="$log_file" \
	bash "$pak_dir/Portmaster.sh" "$rom_dir/TestPort.sh"
}

create_common_tree "$root"
run_gui_flow "$root"
run_port_flow "$root"

grep -q '^start:gui:' "$events_file"
grep -q '^stop:gui:' "$events_file"
grep -q '^start:port:' "$events_file"
grep -q '^stop:port:' "$events_file"
grep -q '^gui$' "$run_marker"
grep -q '^port$' "$run_marker"
