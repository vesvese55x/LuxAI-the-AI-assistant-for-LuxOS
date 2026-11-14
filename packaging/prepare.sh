#!/usr/bin/env bash
# Prepare the working tree (.work) from vendored upstream packages.
set -euo pipefail
cd "$(dirname "$0")/.."
WORK="${WORK:-$PWD/.work}"
mkdir -p "$WORK/upstream/account" "$WORK/upstream/i18npkg" "$WORK/stage" "$WORK/dist" "$WORK/appimagekit"

tar --use-compress-program="zstd -d" -xf src/vendor/luxai-2.0.2-3-x86_64.pkg.tar.zst       -C "$WORK/upstream" usr
tar --use-compress-program="zstd -d" -xf src/vendor/luxos-account-1.0.0-8-any.pkg.tar.zst  -C "$WORK/upstream/account" usr
tar --use-compress-program="zstd -d" -xf src/vendor/luxos-i18n-1.0.0-64-any.pkg.tar.zst    -C "$WORK/upstream/i18npkg" usr

echo "prepared $WORK"
ls "$WORK/upstream/usr/bin"
