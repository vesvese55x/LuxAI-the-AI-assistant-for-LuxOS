#!/usr/bin/env python3
"""Build luxai 3.1.0 packages: .deb, .rpm, Arch .pkg.tar.zst, .AppImage.
Runs without root: dpkg-deb --root-owner-group, rpmbuild with user topdir,
tar --owner=0, mksquashfs -all-root + AppImageKit type-2 runtime.
"""
import os, subprocess, sys, hashlib, struct, shutil, glob, gzip
import os, sys
WORK = os.environ.get("WORK") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".work")


W = "" + WORK + ""
BLD = f"{W}/build"
DIST = f"{W}/dist"
TOOLBIN = os.environ.get("TOOLBIN", f"{W}/tools/root/usr/bin")
TOOLLIB = os.environ.get("TOOLLIB", "")
VERSION = "3.1.0"
RELEASE = "1"
def tool(name):
    p = f"{TOOLBIN}/{name}"
    return p if TOOLBIN and os.path.isfile(p) else name

ENV = {**os.environ,
       "LD_LIBRARY_PATH": (TOOLLIB + ":" + os.environ["LD_LIBRARY_PATH"] if TOOLLIB and os.environ.get("LD_LIBRARY_PATH") else (TOOLLIB or os.environ.get("LD_LIBRARY_PATH", ""))),
       "PATH": (TOOLBIN + ":" if TOOLBIN and os.path.isdir(TOOLBIN) else "") + os.environ["PATH"],
       "ARCH": "x86_64", **({"RPM_CONFIGDIR": f"{W}/tools/root/usr/lib/rpm"} if os.path.isdir(f"{W}/tools/root/usr/lib/rpm") else {})}

DESCRIPTION = ("LuxAI – the AI assistant for LuxOS. Chat with AI or let the agent "
               "work with your computer. Unofficial multi-distro build by BELGESELSEMO "
               "(Z.AI-assisted extraction from the original LuxOS distribution) with "
               "17 fully translated languages: tr, ru, zh, ja, hi, de, fr, es, it, nl, "
               "pl, pt, sk, sv, cs, hu, uk.")

os.makedirs(DIST, exist_ok=True)

# ---------------- .deb ----------------
def build_deb():
    ctrl = f"{W}/deb/DEBIAN"
    shutil.rmtree(f"{W}/deb", ignore_errors=True)
    os.makedirs(ctrl)
    open(f"{ctrl}/control", "w").write(f"""Package: luxai
Version: {VERSION}
Section: utils
Priority: optional
Architecture: amd64
Maintainer: BELGESELSEMO <belgeselsemo@belgeselsemo.com.tr>
Installed-Size: 4200
Depends: libgtk-3-0 (>= 3.24) | libgtk-3-0t64 (>= 3.24), python3 (>= 3.10), python3-gi, gir1.2-gtk-3.0, libcurl4 | libcurl4t64, libglib2.0-0 | libglib2.0-0t64, shared-mime-info
Homepage: http://belgeselsemo.com.tr
Description: LuxAI – the AI assistant for LuxOS (unofficial build)
 Chat with AI or let the agent work with your computer: run commands,
 manage files, open apps, take screenshots – always with your approval.
 .
 Unofficial multi-distro build by BELGESELSEMO, compiled from the original
 LuxOS distribution with Z.AI assistance. 17 fully translated languages
 (tr, ru, zh, ja, hi, de, fr, ...), dynamic system-language detection with
 English fallback, modern window buttons, account panel included.
""")
    open(f"{ctrl}/postinst", "w").write("""#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -qf /usr/share/icons/hicolor || true
fi
exit 0
""")
    os.chmod(f"{ctrl}/postinst", 0o755)
    # copy the tree's usr/ contents into the package root (NOT BLD itself,
    # which would create a doubled usr/usr/ inside the .deb)
    shutil.copytree(f"{BLD}/usr", f"{W}/deb/usr", dirs_exist_ok=True)
    # real installed size (KiB) for dpkg
    isize = sum(os.path.getsize(os.path.join(r, f))
                for r, _, fs in os.walk(f"{W}/deb/usr") for f in fs) // 1024
    ctrl_text = open(f"{ctrl}/control", encoding="utf-8").read()
    open(f"{ctrl}/control", "w", encoding="utf-8").write(
        ctrl_text.replace("Installed-Size: 4200", f"Installed-Size: {isize}"))
    r = subprocess.run(["dpkg-deb", "--root-owner-group", "--build",
                        f"{W}/deb", f"{DIST}/luxai_{VERSION}_amd64.deb"],
                       capture_output=True, text=True)
    print(r.stdout, r.stderr)
    assert r.returncode == 0
    print("deb OK")

# ---------------- .rpm ----------------
def build_rpm():
    top = f"{W}/rpm"
    shutil.rmtree(top, ignore_errors=True)
    for d in ("BUILD", "RPMS", "SOURCES", "SPECS", "SRPMS"):
        os.makedirs(f"{top}/{d}")
    # tarball of build tree as SOURCE
    subprocess.run(["tar", "--owner=0", "--group=0", "-czf",
                    f"{top}/SOURCES/luxai-{VERSION}.tar.gz", "-C", BLD, "usr"], check=True)
    files_lines = []
    for root, dirs, files in os.walk(BLD):
        for f in sorted(files):
            p = os.path.join(root, f)[len(BLD):]
            files_lines.append(p)
    spec = f"""Name:           luxai
Version:        {VERSION}
Release:        {RELEASE}%{{?dist}}
Summary:        LuxAI – the AI assistant for LuxOS (unofficial build)
License:        MIT and LSL
URL:            http://belgeselsemo.com.tr
Source0:        luxai-{VERSION}.tar.gz
Requires:       gtk3, python3, python3-gobject, libcurl, glib2
BuildArch:      x86_64
AutoReqProv:    no

%description
{DESCRIPTION}

%prep
%setup -q -c

%build

%install
mkdir -p %{{buildroot}}/usr
cp -a usr/. %{{buildroot}}/usr/

%files
""" + "\n".join(files_lines) + """

%post
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -qf /usr/share/icons/hicolor || true
fi
exit 0
"""
    open(f"{top}/SPECS/luxai.spec", "w").write(spec)
    defines = [
        f"--define=_topdir {top}",
        "--define=_tmppath /tmp/rpm-tmp",
        f"--define=_dbpath {W}/rpm/db",
        "--define=%__os_install_post %{nil}",
        "--define=%debug_package %{nil}",
    ]
    os.makedirs("/tmp/rpm-tmp", exist_ok=True)
    os.makedirs(f"{W}/rpm/db", exist_ok=True)
    rcfile = os.path.join(TOOLBIN, "..", "lib/rpm/rpmrc")
    cmd = [tool("rpmbuild")]
    if TOOLBIN and os.path.isfile(rcfile):
        cmd += ["--rcfile", rcfile]
    r = subprocess.run(cmd + ["-bb", *defines, f"{top}/SPECS/luxai.spec"],
                       capture_output=True, text=True, env=ENV)
    if r.returncode != 0:
        print(r.stdout[-3000:], r.stderr[-3000:])
        sys.exit("rpmbuild failed")
    out = glob.glob(f"{top}/RPMS/x86_64/luxai-{VERSION}-{RELEASE}*.rpm")[0]
    shutil.copy(out, f"{DIST}/luxai-{VERSION}-{RELEASE}.x86_64.rpm")
    print("rpm OK:", out)

# ---------------- Arch .pkg.tar.zst ----------------
def build_zst():
    """pacman paketi: .PKGINFO (ve .MTREE) ARŞİV KÖKÜNDE olmalı, dosyalar da kökte
    (usr/..., .INSTALL). Önceden her şey luxai/ alt klasöründeydi -> pacman
    "eksik paket bilgisi" hatası veriyordu."""
    root = f"{W}/arch/root"
    shutil.rmtree(f"{W}/arch", ignore_errors=True)
    shutil.copytree(BLD, root)                     # root/usr/...
    import time
    files = [os.path.join(dp, f) for dp, _, fs in os.walk(root) for f in fs]
    size = sum(os.path.getsize(p) for p in files)
    pkginfo = f"""# Generated by BELGESELSEMO build
pkgname = luxai
pkgbase = luxai
pkgver = {VERSION}-{RELEASE}
pkgdesc = LuxAI - the AI assistant for LuxOS (unofficial BELGESELSEMO build, 17 languages)
url = http://belgeselsemo.com.tr
builddate = {int(time.time())}
packager = BELGESELSEMO <belgeselsemo@belgeselsemo.com.tr>
size = {size}
arch = x86_64
license = MIT
license = custom:LSL
replaces = luxai-tr
conflicts = luxai-tr
group = luxos
provides = luxai={VERSION}
depends = gtk3
depends = python
depends = python-gobject
depends = curl
depends = glib2
depends = shared-mime-info
optdepends = polkit: administrator mode support
"""
    open(f"{root}/.PKGINFO", "w", encoding="utf-8").write(pkginfo)
    # paket betikleri (kurus/gncelleme sonrasi önbellekleri tazele)
    install = """post_install() {
  gtk-update-icon-cache -q -t -f usr/share/icons/hicolor 2>/dev/null || true
  update-desktop-database -q usr/share/applications 2>/dev/null || true
}
post_upgrade() {
  post_install
}
post_remove() {
  gtk-update-icon-cache -q -t -f usr/share/icons/hicolor 2>/dev/null || true
  update-desktop-database -q usr/share/applications 2>/dev/null || true
}
"""
    open(f"{root}/.INSTALL", "w", encoding="utf-8").write(install)
    # .MTREE (gzip'li mtree; pacman dogrulama icin)
    # .MTREE: DIZINLER de listelenmeli; aksi halde libalpm dizinleri "file not
    # found in file list" diyerek atlar ve bos dizinler kurulmaz.
    lines = ["#mtree"]
    entries = []
    for dp, dns, fns in os.walk(root):
        for d in dns:
            p = os.path.join(dp, d)
            rel = "./" + os.path.relpath(p, root)
            st = os.stat(p)
            entries.append((rel, f"{rel} time={st.st_mtime:.11f} "
                                 f"mode={st.st_mode & 0o7777:o} type=dir"))
        for f in fns:
            p = os.path.join(dp, f)
            rel = "./" + os.path.relpath(p, root)
            st = os.stat(p)
            data = open(p, "rb").read()
            entries.append((rel, f"{rel} time={st.st_mtime:.11f} mode={st.st_mode & 0o7777:o} "
                                 f"size={st.st_size} type=file "
                                 f"md5digest={hashlib.md5(data).hexdigest()} "
                                 f"sha256digest={hashlib.sha256(data).hexdigest()}"))
    lines += [e[1] for e in sorted(entries)]
    mtree_txt = "\n".join(lines) + "\n"
    open(f"{root}/.MTREE", "wb").write(gzip.compress(mtree_txt.encode(), 9))
    tar_path = f"{W}/arch/pkg.tar"
    # ONEMLI: pacman .PKGINFO'yu arsiv KOKUNDE ve "./" oneki OLMADAN arar.
    # Bu yuzden dosyalari tek tek sayiyoruz (tar -C root . -> "./.PKGINFO" olur
    # ve pacman "eksik paket bilgisi" hatasi verir - kullanicinin gordugu hata).
    tar_entries = sorted(os.listdir(root))
    subprocess.run(["tar", "--owner=0", "--group=0", "--numeric-owner",
                    "-cf", tar_path, "-C", root] + tar_entries, check=True)
    out = f"{DIST}/luxai-{VERSION}-{RELEASE}-x86_64.pkg.tar.zst"
    subprocess.run([tool("zstd"), "-q", "--ultra", "-22", "-f", tar_path, "-o", out],
                   check=True, env=ENV)
    os.remove(tar_path)
    print("zst OK:", out)

# ---------------- AppImage ----------------
def build_appimage():
    appdir = f"{W}/appdir"
    shutil.rmtree(appdir, ignore_errors=True)
    shutil.copytree(BLD, appdir)
    shutil.copy(f"{BLD}/usr/share/applications/luxai.desktop", f"{appdir}/luxai.desktop")
    shutil.copy(f"{BLD}/usr/share/icons/hicolor/256x256/apps/luxai.png", f"{appdir}/luxai.png")
    shutil.copy(f"{BLD}/usr/share/icons/hicolor/256x256/apps/luxai.png", f"{appdir}/.DirIcon")
    apprun = f"{appdir}/AppRun"
    open(apprun, "w").write("""#!/bin/bash
SELF=$(readlink -f "${0}")
HERE=${SELF%/*}
export APPDIR="$HERE"
FIX="$APPDIR/usr/lib/luxai/luxfix.so"
[ -r "$FIX" ] && LD_PRELOAD="$FIX${LD_PRELOAD:+:$LD_PRELOAD}" && export LD_PRELOAD
BIN="$APPDIR/usr/lib/luxai/luxai-bin"
[ -x "$BIN" ] || BIN="$APPDIR/usr/bin/luxai"
exec "$BIN" "$@"
""")
    os.chmod(apprun, 0o755)
    out = f"{DIST}/LuxAI-{VERSION}-x86_64.AppImage"
    r = subprocess.run([f"{W}/appimagekit/appimagetool.AppImage",
                        "--appimage-extract-and-run",
                        "--no-appstream", appdir, out],
                       capture_output=True, text=True, env=ENV)
    if r.returncode != 0:
        print(r.stdout[-2000:], r.stderr[-2000:])
        sys.exit("appimagetool failed")
    print(r.stdout[-500:])
    os.chmod(out, 0o755)
    print("AppImage OK")

build_deb()
if os.environ.get("SKIP_RPM") != "1":   # local runs without rpmbuild; CI builds the rpm
    build_rpm()
build_zst()
build_appimage()

# checksums
os.chdir(DIST)
files = sorted(f for f in os.listdir(DIST) if f != "SHA256SUMS")
sums = []
for f in files:
    h = hashlib.sha256(open(f, "rb").read()).hexdigest()
    sums.append(f"{h}  {f}")
open("SHA256SUMS", "w").write("\n".join(sums) + "\n")
print("\nSHA256SUMS:\n" + "\n".join(sums))
for f in files:
    print(f"{f}: {os.path.getsize(f)/1e6:.2f} MB")
