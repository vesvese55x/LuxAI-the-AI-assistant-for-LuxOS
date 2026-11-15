#!/usr/bin/env bash
# LuxAI-Setup.exe (native kurucu, WSL'siz) — LuxAI.exe'yi icine gomer.
set -euo pipefail
cd "$(dirname "$0")"
CC=${CC:-x86_64-w64-mingw32-gcc}
RC=${RC:-x86_64-w64-mingw32-windres}
mkdir -p build

[ -f build/LuxAI.exe ] || { echo "once build-win.sh calistirin"; exit 1; }

cat > build/setup_res.rc <<EOF
#include <winver.h>
1 ICON "$PWD/res/luxai.ico"
1 24 "$PWD/res/luxai.manifest"
201 RCDATA "$PWD/build/LuxAI.exe"
202 RCDATA "$PWD/res/luxai.ico"

VS_VERSION_INFO VERSIONINFO
 FILEVERSION 3,1,0,1
 PRODUCTVERSION 3,1,0,1
 FILEFLAGSMASK 0x3fL
 FILEFLAGS 0x0L
 FILEOS VOS_NT_WINDOWS32
 FILETYPE VFT_APP
 FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "CompanyName",      "BELGESELSEMO topluluk yapisi"
            VALUE "FileDescription",  "LuxAI Setup (native Windows, no WSL)"
            VALUE "FileVersion",      "3.1.0.1"
            VALUE "OriginalFilename", "LuxAI-Setup.exe"
            VALUE "ProductName",      "LuxAI"
            VALUE "ProductVersion",   "3.1.0-win.1"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
EOF

( cd build && "$RC" setup_res.rc -O coff -o setup_res.o )

"$CC" -O2 -s -municode -mwindows -DUNICODE -D_UNICODE -Wall \
      -Wl,--major-subsystem-version,6 -Wl,--minor-subsystem-version,1 \
      -o build/LuxAI-Setup.exe installer/luxai_setup_win.c build/setup_res.o \
      -lole32 -luuid -lshell32 -lshlwapi -ladvapi32

python3 - build/LuxAI-Setup.exe build/LuxAI.exe <<'PY'
import sys
exe = open(sys.argv[1], "rb").read()
inner = open(sys.argv[2], "rb").read()
assert exe[:2] == b"MZ", "PE degil"
assert inner[:64] in exe, "LuxAI.exe gomulu degil"
print("LuxAI-Setup.exe OK: %d bayt (gömülü LuxAI.exe: %d bayt)" % (len(exe), len(inner)))
PY
ls -l build/LuxAI-Setup.exe
