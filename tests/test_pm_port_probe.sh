#!/bin/sh
set -eu

helper="$1"
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

create_elf_stub() {
	path="$1"
	shift
	{
		printf '\177ELF\002\001'
		dd if=/dev/zero bs=1 count=58 2>/dev/null
		for symbol in "$@"; do
			printf '%s\n' "$symbol"
		done
	} >"$path"
}

create_armhf_elf_stub() {
	path="$1"
	shift
	{
		printf '\177ELF\001\001'
		printf '\001'
		dd if=/dev/zero bs=1 count=9 2>/dev/null
		printf '\002\000\050\000'
		for symbol in "$@"; do
			printf '%s\n' "$symbol"
		done
	} >"$path"
}

port_with_gl="$tmpdir/port-with-gl"
port_without_gl="$tmpdir/port-without-gl"
mkdir -p "$port_with_gl/dir" "$port_with_gl/deep/nested" "$port_with_gl/lib/libarm64" "$port_without_gl"

create_elf_stub "$port_with_gl/needs-sdl" SDL_GetDefaultAudioInfo
create_elf_stub "$port_with_gl/needs-pulse" libpulse-simple.so.0
create_elf_stub "$port_with_gl/uses-gl" SDL_GL_CreateContext SDL_CreateWindow
create_elf_stub "$port_with_gl/dir/mixed" SDL_GetDefaultAudioInfo libpulse-simple.so.0 SDL_GL_CreateContext SDL_CreateWindow
create_elf_stub "$port_with_gl/deep/nested/too-deep" SDL_GetDefaultAudioInfo
create_elf_stub "$port_with_gl/libskip.so.1" SDL_GetDefaultAudioInfo
create_elf_stub "$port_with_gl/wrapped-bin.original" SDL_GetDefaultAudioInfo
printf '#!/bin/sh\n# PMI_AARCH64_SDL_COMPAT_WRAPPER=1\nexit 0\n' >"$port_with_gl/wrapped-bin"
printf '#!/bin/sh\necho test\n' >"$port_with_gl/script.sh"
printf 'cache\n' >"$port_with_gl/.pmi-port-probe-v2.tsv"
printf 'not-elf\n' >"$port_with_gl/not-elf"
printf 'egl\n' >"$port_with_gl/lib/libarm64/libEGL.so.1"

create_elf_stub "$port_without_gl/no-sdl" SomeOtherSymbol

output="$("$helper" scan-aarch64-launch-port "$port_with_gl")"
printf '%s\n' "$output" | grep -Fx "PORT	$port_with_gl	has_bundled_native_gl_stack=1"
printf '%s\n' "$output" | grep -Fx "BIN	$port_with_gl/dir/mixed	needs_default_audio_info=1	needs_pulse_simple=1	uses_sdl_gl_windowing=1	is_wrapper=0"
printf '%s\n' "$output" | grep -Fx "BIN	$port_with_gl/needs-pulse	needs_default_audio_info=0	needs_pulse_simple=1	uses_sdl_gl_windowing=0	is_wrapper=0"
printf '%s\n' "$output" | grep -Fx "BIN	$port_with_gl/needs-sdl	needs_default_audio_info=1	needs_pulse_simple=0	uses_sdl_gl_windowing=0	is_wrapper=0"
printf '%s\n' "$output" | grep -Fx "BIN	$port_with_gl/uses-gl	needs_default_audio_info=0	needs_pulse_simple=0	uses_sdl_gl_windowing=1	is_wrapper=0"
printf '%s\n' "$output" | grep -Fx "BIN	$port_with_gl/wrapped-bin	needs_default_audio_info=1	needs_pulse_simple=0	uses_sdl_gl_windowing=0	is_wrapper=1"
! printf '%s\n' "$output" | grep -Fq "$port_with_gl/script.sh"
! printf '%s\n' "$output" | grep -Fq "$port_with_gl/libskip.so.1"
! printf '%s\n' "$output" | grep -Fq "$port_with_gl/.pmi-port-probe-v2.tsv"
! printf '%s\n' "$output" | grep -Fq "$port_with_gl/deep/nested/too-deep"
! printf '%s\n' "$output" | grep -Fq "$port_with_gl/not-elf"

output_without_gl="$("$helper" scan-aarch64-launch-port "$port_without_gl")"
printf '%s\n' "$output_without_gl" | grep -Fx "PORT	$port_without_gl	has_bundled_native_gl_stack=0"
printf '%s\n' "$output_without_gl" | grep -Fx "BIN	$port_without_gl/no-sdl	needs_default_audio_info=0	needs_pulse_simple=0	uses_sdl_gl_windowing=0	is_wrapper=0"

armhf_port="$tmpdir/armhf-port"
mkdir -p "$armhf_port/deep/nested"
create_armhf_elf_stub "$armhf_port/gmloader"
create_armhf_elf_stub "$armhf_port/bgdi"
create_armhf_elf_stub "$armhf_port/deep/nested/too-deep"
printf '#!/bin/sh\n# PMI_ARMHF_EXEC_COMPAT_WRAPPER=1\nexit 0\n' >"$armhf_port/wrapped-bgdi"
create_armhf_elf_stub "$armhf_port/wrapped-bgdi.original"
create_elf_stub "$armhf_port/wrapped-bin-helper" PMI_ARMHF_EXEC_COMPAT_BINARY=1
create_armhf_elf_stub "$armhf_port/wrapped-bin-helper.original"
printf '#!/bin/sh\necho test\n' >"$armhf_port/script.sh"
printf 'cache\n' >"$armhf_port/.pmi-armhf-port-probe-v1.tsv"
printf 'not-elf\n' >"$armhf_port/SorR.dat"
chmod +x "$armhf_port/gmloader" "$armhf_port/bgdi" "$armhf_port/wrapped-bgdi" "$armhf_port/wrapped-bin-helper" "$armhf_port/script.sh" "$armhf_port/SorR.dat"

armhf_output="$("$helper" scan-armhf-launch-port "$armhf_port")"
printf '%s\n' "$armhf_output" | grep -Fx "PORT	$armhf_port"
printf '%s\n' "$armhf_output" | grep -Fx "BIN	$armhf_port/bgdi	is_wrapper=0"
printf '%s\n' "$armhf_output" | grep -Fx "BIN	$armhf_port/gmloader	is_wrapper=0"
printf '%s\n' "$armhf_output" | grep -Fx "BIN	$armhf_port/wrapped-bin-helper	is_wrapper=1"
printf '%s\n' "$armhf_output" | grep -Fx "BIN	$armhf_port/wrapped-bgdi	is_wrapper=1"
! printf '%s\n' "$armhf_output" | grep -Fq "$armhf_port/script.sh"
! printf '%s\n' "$armhf_output" | grep -Fq "$armhf_port/.pmi-armhf-port-probe-v1.tsv"
! printf '%s\n' "$armhf_output" | grep -Fq "$armhf_port/SorR.dat"
! printf '%s\n' "$armhf_output" | grep -Fq "$armhf_port/deep/nested/too-deep"
