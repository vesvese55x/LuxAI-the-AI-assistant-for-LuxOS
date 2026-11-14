#!/usr/bin/env bash
# Build all 4 Linux packages (deb, rpm, Arch zst, AppImage) from vendored upstream LuxAI.
#   WORK=<dir> bash packaging/build-all.sh
set -euo pipefail
cd "$(dirname "$0")/.."
REPO="$PWD"
export WORK="${WORK:-$REPO/.work}"

# 1. vendored upstream tree
bash packaging/prepare.sh

# 2. assemble full translations from part files
mkdir -p "$WORK"
for lang in tr ru zh ja hi; do
    cat src/luxai/i18n/${lang}_a.tsv src/luxai/i18n/${lang}_b.tsv src/luxai/i18n/${lang}_c.tsv \
        > "$WORK/luxai_${lang}.tsv"
done
python3 packaging/verify_tsv.py "$WORK"/luxai_tr.tsv "$WORK"/luxai_ru.tsv \
    "$WORK"/luxai_zh.tsv "$WORK"/luxai_ja.tsv "$WORK"/luxai_hi.tsv

# 3. binary patch (13-language dropdown + schema + langmap, modern CSS)
python3 packaging/patch_binary.py

# 4. i18n finalize (16 languages + BELGESELSEMO signature + account tsvs)
python3 packaging/finalize_i18n.py

# 5. assemble package tree (hicolor icons, translated desktop entry, python vendor)
python3 packaging/assemble_tree.py

# 6. packages
# AppImageKit appimagetool (before packages; appimagetool bundled mksquashfs)
if [ ! -f "$WORK/appimagekit/appimagetool.AppImage" ]; then
    curl -sfL -o "$WORK/appimagekit/appimagetool.AppImage" \
        https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
    chmod +x "$WORK/appimagekit/appimagetool.AppImage"
fi

python3 packaging/build_packages.py

echo "dist contents:"
ls -la "$WORK/dist"
