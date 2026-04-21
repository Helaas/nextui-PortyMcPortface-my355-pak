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
	printf 'SomeOtherSymbol\n'
} >"$tmpdir/no-sdl"
printf '#!/bin/sh\necho SDL_GetDefaultAudioInfo\n' >"$tmpdir/not-elf"

"$checker" "$tmpdir/needs-sdl"
if "$checker" "$tmpdir/no-sdl"; then
	exit 1
fi
if "$checker" "$tmpdir/not-elf"; then
	exit 1
fi
