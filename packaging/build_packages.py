#!/usr/bin/env python3
"""Build luxai 3.1.0 packages: .deb, .rpm, Arch .pkg.tar.zst, .AppImage.
Runs without root: dpkg-deb --root-owner-group, rpmbuild with user topdir,
tar --owner=0, mksquashfs -all-root + AppImageKit type-2 runtime.
"""
import os, subprocess, sys, hashlib, struct, shutil, glob
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
               "16 fully translated languages: tr, ru, zh, ja, de, fr, es, it, nl, pl, "
               "pt, sk, sv, cs, hu, uk.")

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
 LuxOS distribution with Z.AI assistance. 16 fully translated languages
 (tr, ru, zh, ja, de, fr, ...), dynamic system-language detection with
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
    shutil.copytree(BLD, f"{W}/deb/usr", dirs_exist_ok=True)
    # keep upstream doc path convention
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
    root = f"{W}/arch/luxai"
    shutil.rmtree(f"{W}/arch", ignore_errors=True)
    shutil.copytree(BLD, root)
    pkginfo = f"""pkgname = luxai
pkgbase = luxai
pkgver = {VERSION}-{RELEASE}
pkgdesc = LuxAI – the AI assistant for LuxOS (unofficial BELGESELSEMO build, 16 languages)
url = http://belgeselsemo.com.tr
builddate = 1760000000
packager = BELGESELSEMO <belgeselsemo@belgeselsemo.com.tr>
size = {sum(os.path.getsize(os.path.join(r,f)) for r,_,fs in os.walk(root) for f in fs)}
arch = x86_64
license = MIT
license = custom:LSL
replaces = luxai-tr
group = luxos
depends = gtk3
depends = python
depends = python-gobject
depends = curl
depends = glib2
optdepends = polkit: administrator mode support
provides = luxai={VERSION}
"""
    with open(f"{root}/.PKGINFO", "w") as f:
        f.write(pkginfo)
    # .MTREE with sha256 digests
    mtree = ["#mtree"]
    for dirpath, dirs, files in os.walk(root):
        for fn in sorted(files):
            p = os.path.join(dirpath, fn)
            rel = os.path.relpath(p, root)
            if rel in (".MTREE",):
                continue
            st = os.stat(p)
            import time
            digest = hashlib.sha256(open(p, "rb").read()).hexdigest()
            mtree.append(f"{rel} time={st.st_mtime:.0f}.0 size={st.st_size} "
                         f"md5digest={hashlib.md5(open(p,'rb').read()).hexdigest()} "
                         f"sha256digest={digest}")
    open(f"{root}/.MTREE", "w").write("\n".join(mtree) + "\n")
    tar_path = f"{W}/arch/luxai-{VERSION}-{RELEASE}-x86_64.pkg.tar"
    subprocess.run(["tar", "--owner=0", "--group=0", "-cf", tar_path,
                    "-C", f"{W}/arch", "luxai"], check=True)
    subprocess.run([tool("zstd"), "-q", "--ultra", "-22", "-f",
                    tar_path, "-o", f"{DIST}/luxai-{VERSION}-{RELEASE}-x86_64.pkg.tar.zst"],
                   check=True, env=ENV)
    os.remove(tar_path)
    print("zst OK")

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
exec "${APPDIR}/usr/bin/luxai" "$@"
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
