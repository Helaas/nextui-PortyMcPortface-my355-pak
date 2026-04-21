#!/bin/sh
set -eu

root="$(mktemp -d)"
trap 'rm -rf "$root"' EXIT

repo="$root/box64-src"
cache="$root/cache"
out1="$root/out1"
out2="$root/out2"
out3="$root/out3"
bin_dir="$root/test-bin"
build_count_file="$root/build-count"

mkdir -p "$repo" "$cache" "$out1" "$out2" "$out3" "$bin_dir"
git init -b main "$repo" >/dev/null 2>&1
printf 'initial\n' >"$repo/README.md"
git -C "$repo" add README.md
git -C "$repo" -c user.name='Test' -c user.email='test@example.com' commit -m 'initial' >/dev/null 2>&1

cat >"$bin_dir/cmake" <<'EOF'
#!/bin/sh
exit 0
EOF

cat >"$bin_dir/make" <<'EOF'
#!/bin/sh
set -eu

count_file="${PMI_TEST_BUILD_COUNT_FILE:?}"
count=0
if [ -f "$count_file" ]; then
    count=$(cat "$count_file")
fi
count=$((count + 1))
printf '%s\n' "$count" >"$count_file"
printf 'box64 build %s\n' "$count" >box64
chmod +x box64
EOF

cat >"$bin_dir/aarch64-nextui-linux-gnu-strip" <<'EOF'
#!/bin/sh
exit 0
EOF

cat >"$bin_dir/aarch64-nextui-linux-gnu-gcc" <<'EOF'
#!/bin/sh
if [ "${1:-}" = "--version" ]; then
    printf 'fake gcc 1.0\n'
fi
exit 0
EOF

chmod +x "$bin_dir/cmake" "$bin_dir/make" \
    "$bin_dir/aarch64-nextui-linux-gnu-strip" \
    "$bin_dir/aarch64-nextui-linux-gnu-gcc"

PATH="$bin_dir:$PATH" \
PMI_TEST_BUILD_COUNT_FILE="$build_count_file" \
BOX64_GIT_URL="$repo" \
BOX64_GIT_REF="main" \
sh scripts/build-box64-from-source.sh "$out1" "$cache"

test -x "$out1/box64"
test "$(cat "$build_count_file")" = "1"
first_commit="$(git -C "$repo" rev-parse HEAD)"
grep -q "commit=$first_commit" "$cache"/*/metadata.txt

PATH="$bin_dir:$PATH" \
PMI_TEST_BUILD_COUNT_FILE="$build_count_file" \
BOX64_GIT_URL="$repo" \
BOX64_GIT_REF="main" \
sh scripts/build-box64-from-source.sh "$out2" "$cache"

test -x "$out2/box64"
test "$(cat "$build_count_file")" = "1"
cmp "$out1/box64" "$out2/box64"

printf 'update\n' >>"$repo/README.md"
git -C "$repo" add README.md
git -C "$repo" -c user.name='Test' -c user.email='test@example.com' commit -m 'update' >/dev/null 2>&1

PATH="$bin_dir:$PATH" \
PMI_TEST_BUILD_COUNT_FILE="$build_count_file" \
BOX64_GIT_URL="$repo" \
BOX64_GIT_REF="main" \
sh scripts/build-box64-from-source.sh "$out3" "$cache"

test -x "$out3/box64"
test "$(cat "$build_count_file")" = "2"
test "$(find "$cache" -mindepth 1 -maxdepth 1 -type d | wc -l | tr -d ' ')" = "2"
