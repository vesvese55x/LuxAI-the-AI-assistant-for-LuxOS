#!/usr/bin/env bash
# LuxAI native Windows derlemesi (mingw-w64 capraz derleyici).
#   Debian/Ubuntu: sudo apt install gcc-mingw-w64-x86-64 binutils-mingw-w64-x86-64
#   Cikti: build/LuxAI.exe  (Windows 7 SP1+ / 8 / 10 / 11, x64)
set -euo pipefail
cd "$(dirname "$0")"
CC=${CC:-x86_64-w64-mingw32-gcc}
CXX=${CXX:-x86_64-w64-mingw32-g++}
RC=${RC:-x86_64-w64-mingw32-windres}
OUT=build/LuxAI.exe

command -v "$CC" >/dev/null || { echo "hata: $CC bulunamadi (mingw-w64 kurun)"; exit 1; }
command -v "$CXX" >/dev/null || { echo "hata: $CXX bulunamadi (g++-mingw-w64 kurun)"; exit 1; }
mkdir -p build

echo "[1/4] araç şeması üretiliyor (tools.json -> tools_data.c)"
python3 src/gen_tools.py src/tools.json build/tools_data.c

echo "[2/4] kaynaklar (ikon + manifest + sürüm bilgisi)"
( cd res && "$RC" luxai.rc -O coff -o "../build/luxai_res.o" )

echo "[3/4] derleme"
CFLAGS=(-O2 -municode -std=gnu11 -DUNICODE -D_UNICODE -DWINVER=0x0601 -D_WIN32_WINNT=0x0601
        -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type)
CXXFLAGS=(-O2 -municode -std=gnu++17 -DUNICODE -D_UNICODE -DWINVER=0x0601 -D_WIN32_WINNT=0x0601
          -Wall -Wextra -Wno-unused-parameter)
OBJS=()
for f in main util json http config prompt chat tools ui; do
    "$CC" "${CFLAGS[@]}" -c "src/$f.c" -o "build/$f.o"
    OBJS+=("build/$f.o")
done
"$CC" "${CFLAGS[@]}" -c build/tools_data.c -o build/tools_data.o
OBJS+=("build/tools_data.o")
# arayuz cizim katmani (GDI+, C++)
"$CXX" "${CXXFLAGS[@]}" -c src/skin.cpp -o build/skin.o
OBJS+=("build/skin.o")
OBJS+=("build/luxai_res.o")

"$CXX" -s -municode -mwindows -o "$OUT" "${OBJS[@]}" \
      -static-libgcc -static-libstdc++ \
      -Wl,--major-os-version,6 -Wl,--minor-os-version,1 \
      -Wl,--major-subsystem-version,6 -Wl,--minor-subsystem-version,1 \
      -lwinhttp -lcomctl32 -lgdiplus -luxtheme -lshell32 -lole32 -luuid -lshlwapi \
      -lgdi32 -luser32 -ladvapi32

echo "[4/4] doğrulama"
python3 - "$OUT" <<'PY'
import sys, struct
d = open(sys.argv[1], "rb").read()
assert d[:2] == b"MZ", "PE degil"
pe = struct.unpack_from("<I", d, 0x3C)[0]
assert d[pe:pe+4] == b"PE\0\0"
machine, nsec = struct.unpack_from("<HH", d, pe+4)
opt = pe + 24
magic = struct.unpack_from("<H", d, opt)[0]
osver = struct.unpack_from("<HH", d, opt + 40)[0:2]
subsys = struct.unpack_from("<H", d, opt + 68)[0]
print("PE ok: machine=%#x (%s), sürüm=%d.%d, alt sistem=%d (%s), boyut=%d bayt"
      % (machine, {0x8664:"x86-64",0x14c:"i386"}.get(machine,"?"), osver[0], osver[1],
         subsys, "GUI" if subsys == 2 else "konsol", len(d)))
PY
ls -l "$OUT"
