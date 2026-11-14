#!/usr/bin/env bash
# Build LuxAI-Setup.exe (Windows installer, WSL-based) with mingw-w64.
# Requires: gcc-mingw-w64-x86-64-posix (apt install gcc-mingw-w64-x86-64-posix)
set -euo pipefail
cd "$(dirname "$0")/.."
REPO="$PWD"
WORK="${WORK:-$REPO/.work}"
VERSION=3.1.0
EXE="$WORK/exe"
mkdir -p "$EXE" "$WORK/dist"

# 1. PNG -> ICO (256x256 PNG-encoded entry, Vista+)
python3 - "$WORK/build/usr/share/icons/hicolor/256x256/apps/luxai.png" "$EXE/luxai.ico" <<'PYEOF'
import struct, sys
png = open(sys.argv[1], "rb").read()
ico = struct.pack("<HHH", 0, 1, 1) + struct.pack("<BBBBHHII", 0, 0, 0, 0, 1, 32, len(png), 22)
open(sys.argv[2], "wb").write(ico + png)
PYEOF

# 2. resources (embedded .deb + icon)
cat > "$EXE/luxai_res.rc" <<EOF
1 ICON "$EXE/luxai.ico"
101 RCDATA "$WORK/dist/luxai_${VERSION}_amd64.deb"
102 RCDATA "$EXE/luxai.ico"
EOF
if ! command -v x86_64-w64-mingw32-gcc >/dev/null && command -v x86_64-w64-mingw32-gcc-posix >/dev/null; then
    sudo ln -sf "$(command -v x86_64-w64-mingw32-gcc-posix)" /usr/bin/x86_64-w64-mingw32-gcc || true
fi
x86_64-w64-mingw32-windres "$EXE/luxai_res.rc" -O coff -o "$EXE/luxai_res.o"

# 3. compile
x86_64-w64-mingw32-gcc-posix -O2 -Wall \
    -municode -mwindows -s \
    -o "$WORK/dist/LuxAI-${VERSION}-Setup.exe" \
    "$REPO/packaging/luxai_setup.c" "$EXE/luxai_res.o" \
    -lole32 -lshell32 -luuid -lcomctl32

file "$WORK/dist/LuxAI-${VERSION}-Setup.exe"

# 4. verify embedded payload
python3 - "$WORK/dist/LuxAI-${VERSION}-Setup.exe" "$WORK/dist/luxai_${VERSION}_amd64.deb" <<'PYEOF'
import sys
exe = open(sys.argv[1], "rb").read()
deb = open(sys.argv[2], "rb").read()
assert exe[:2] == b"MZ", "not a PE"
assert deb[:64] in exe, "deb payload missing"
print("exe OK:", len(exe), "bytes; deb embedded:", len(deb), "bytes")
PYEOF
