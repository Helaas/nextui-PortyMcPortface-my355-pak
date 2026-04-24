#!/bin/sh
set -eu

checker="$1"
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

{
	printf '\177ELF\002\001'
	dd if=/dev/zero bs=1 count=58 2>/dev/null
	printf 'SDL_GetDefaultAudioInfo\n'
} >"$tmpdir/needs-sdl"
{
	printf '\177ELF\002\001'
	dd if=/dev/zero bs=1 count=58 2>/dev/null
	printf 'SDL_GL_CreateContext\nSDL_CreateWindow\n'
} >"$tmpdir/uses-gl"
{
	printf '\177ELF\002\001'
	dd if=/dev/zero bs=1 count=58 2>/dev/null
	printf 'SDL_GL_CreateContext\n'
} >"$tmpdir/partial-gl"
{
	printf '\177ELF\002\001'
	dd if=/dev/zero bs=1 count=58 2>/dev/null
	printf 'SomeOtherSymbol\n'
} >"$tmpdir/no-sdl"
printf '#!/bin/sh\necho SDL_GetDefaultAudioInfo\n' >"$tmpdir/not-elf"

"$checker" "$tmpdir/needs-sdl"
"$checker" needs-default-audio-info "$tmpdir/needs-sdl"
if "$checker" "$tmpdir/no-sdl"; then
	exit 1
fi
if "$checker" needs-default-audio-info "$tmpdir/no-sdl"; then
	exit 1
fi
if "$checker" "$tmpdir/not-elf"; then
	exit 1
fi
if "$checker" uses-sdl-gl-windowing "$tmpdir/partial-gl"; then
	exit 1
fi
"$checker" uses-sdl-gl-windowing "$tmpdir/uses-gl"
if "$checker" uses-sdl-gl-windowing "$tmpdir/no-sdl"; then
	exit 1
fi
if "$checker" uses-sdl-gl-windowing "$tmpdir/not-elf"; then
	exit 1
fi
