#!/bin/sh
set -eu

outdir="$1"
cache_root="${2:-$outdir/.cache/box64}"
repo_url="${BOX64_GIT_URL:-https://github.com/ptitSeb/box64}"
repo_ref="${BOX64_GIT_REF:-main}"
workdir="$(mktemp -d)"
trap 'rm -rf "$workdir"' EXIT

mkdir -p "$outdir" "$cache_root"

hash_bytes() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum | awk '{print $1}'
        return 0
    fi

    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 | awk '{print $1}'
        return 0
    fi

    cksum | awk '{print $1}'
}

hash_file() {
    cat "$1" | hash_bytes
}

resolve_box64_commit() {
    repo="$1"
    ref="$2"
    commit=""

    case "$ref" in
        [0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]*)
            printf '%s\n' "$ref"
            return 0
            ;;
    esac

    commit=$(
        git ls-remote "$repo" "$ref" "refs/heads/$ref" "refs/tags/$ref^{}" "refs/tags/$ref" 2>/dev/null |
            awk 'NR == 1 { print $1 }'
    )
    [ -n "$commit" ] || return 1
    printf '%s\n' "$commit"
}

clone_box64_repo() {
    repo="$1"
    ref="$2"
    dest="$3"

    if git clone --depth 1 --branch "$ref" "$repo" "$dest" 2>/dev/null; then
        return 0
    fi

    git clone --depth 1 "$repo" "$dest"
    git -C "$dest" fetch --depth 1 origin "$ref"
    git -C "$dest" checkout --detach FETCH_HEAD
}

toolchain_hash="$(aarch64-nextui-linux-gnu-gcc --version 2>/dev/null | hash_bytes)"
script_hash="$(hash_file "$0")"
build_recipe_hash="$(
    cat <<'EOF' | hash_bytes
ARM64=1
ARM_DYNAREC=ON
BOX32=OFF
CMAKE_BUILD_TYPE=RelWithDebInfo
CMAKE_SYSTEM_NAME=Linux
CMAKE_SYSTEM_PROCESSOR=aarch64
CMAKE_C_COMPILER=aarch64-nextui-linux-gnu-gcc
CMAKE_CXX_COMPILER=aarch64-nextui-linux-gnu-g++
CMAKE_ASM_COMPILER=aarch64-nextui-linux-gnu-gcc
EOF
)"
resolved_commit="$(resolve_box64_commit "$repo_url" "$repo_ref" || printf '%s\n' "$repo_ref")"
cache_key="$(
    printf '%s\n%s\n%s\n%s\n' \
        "$resolved_commit" \
        "$toolchain_hash" \
        "$script_hash" \
        "$build_recipe_hash" |
        hash_bytes
)"
cache_dir="$cache_root/$cache_key"
cache_bin="$cache_dir/box64"

if [ -x "$cache_bin" ]; then
    cp -f "$cache_bin" "$outdir/box64"
    chmod +x "$outdir/box64"
    echo "Using cached box64 ($resolved_commit)"
    exit 0
fi

clone_box64_repo "$repo_url" "$repo_ref" "$workdir/box64"
actual_commit="$(git -C "$workdir/box64" rev-parse HEAD)"
if [ "$actual_commit" != "$resolved_commit" ]; then
    cache_key="$(
        printf '%s\n%s\n%s\n%s\n' \
            "$actual_commit" \
            "$toolchain_hash" \
            "$script_hash" \
            "$build_recipe_hash" |
            hash_bytes
    )"
    cache_dir="$cache_root/$cache_key"
    cache_bin="$cache_dir/box64"

    if [ -x "$cache_bin" ]; then
        cp -f "$cache_bin" "$outdir/box64"
        chmod +x "$outdir/box64"
        echo "Using cached box64 ($actual_commit)"
        exit 0
    fi
fi

mkdir -p "$workdir/box64/build"

cd "$workdir/box64/build"
cmake .. \
    -DARM64=1 \
    -DARM_DYNAREC=ON \
    -DBOX32=OFF \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_C_COMPILER=aarch64-nextui-linux-gnu-gcc \
    -DCMAKE_CXX_COMPILER=aarch64-nextui-linux-gnu-g++ \
    -DCMAKE_ASM_COMPILER=aarch64-nextui-linux-gnu-gcc

make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)" box64
aarch64-nextui-linux-gnu-strip box64

tmp_cache_dir="$cache_root/.tmp-$cache_key-$$"
rm -rf "$tmp_cache_dir"
mkdir -p "$tmp_cache_dir"
cp box64 "$tmp_cache_dir/box64"
cat >"$tmp_cache_dir/metadata.txt" <<EOF
repo_url=$repo_url
repo_ref=$repo_ref
commit=$actual_commit
toolchain_hash=$toolchain_hash
script_hash=$script_hash
build_recipe_hash=$build_recipe_hash
EOF
rm -rf "$cache_dir"
mv "$tmp_cache_dir" "$cache_dir"

cp box64 "$outdir/box64"
chmod +x "$outdir/box64"
