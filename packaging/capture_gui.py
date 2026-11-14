#!/usr/bin/env python3
"""Capture REAL LuxAI GUI screenshots by running the installed app under Xvfb.

Usage: python3 packaging/capture_gui.py <root-dir> <out-dir> [lang1,lang2,...]
  <root-dir>  tree containing usr/bin/luxai (extracted .deb or system /)
  <out-dir>   where PNG screenshots are written (luxai-<lang>.png)
  langs       default: tr,en,hi

Needs: Xvfb (apt: xvfb) and python-xlib (pip). Each language is launched in
its own Xvfb display with the matching locale so the GUI really renders in
that language; the app window is located via Xlib, centered/resized for a
consistent frame and captured from the root window (no window manager).
"""
import os, subprocess, sys, time, tempfile, shutil

os.environ.setdefault("PYXLIB_NO_THREADS", "1")  # must be set before Xlib import
from Xlib import X, display

WANT_W = 400          # minimum believable main-window width
SCREEN = (1440, 900)
SHOT = (1120, 700)    # target window geometry for the screenshot frame
MAX_WAIT = 30         # seconds to wait for the window


def find_app_window(d):
    """Return the biggest mapped toplevel window (>= WANT_W wide) or None."""
    root = d.screen().root
    best = None
    for child in root.query_tree().children:
        try:
            attrs = child.get_attributes()
            if attrs.map_state != 2:      # not IsViewable
                continue
            g = child.get_geometry()
            if g.width >= WANT_W and (best is None or g.width * g.height > best[0]):
                best = (g.width * g.height, child, g)
        except Exception:
            continue
    return None if best is None else (best[1], best[2])


def grab(d, x, y, w, h, out_path):
    from PIL import Image
    root = d.screen().root
    img = root.get_image(x, y, w, h, X.ZPixmap, 0xffffffff)
    im = Image.frombytes("RGB", (w, h), img.data, "raw", "BGRX")
    im.save(out_path, "PNG")
    print(f"captured {out_path}: {w}x{h}")


def capture_one(lang, root_dir, out_path, verbose_log):
    disp_no = 77
    xvfb = subprocess.Popen(
        ["Xvfb", f":{disp_no}", "-screen", "0", f"{SCREEN[0]}x{SCREEN[1]}x24",
         "-nolisten", "tcp"],
        stdout=verbose_log, stderr=verbose_log)
    time.sleep(1.2)
    home = tempfile.mkdtemp(prefix="luxai-home-")
    env = {**os.environ,
           "DISPLAY": f":{disp_no}",
           "HOME": home,
           "XDG_CONFIG_HOME": f"{home}/.config",
           "XDG_CACHE_HOME": f"{home}/.cache",
           "XDG_DATA_DIRS": f"{root_dir}/usr/share:/usr/local/share:/usr/share",
           "NO_AT_BRIDGE": "1",
           "GDK_BACKEND": "x11",
           "LC_ALL": f"{lang}.UTF-8",
           "LANG": f"{lang}.UTF-8",
           "LANGUAGE": lang}
    app = subprocess.Popen([f"{root_dir}/usr/bin/luxai"], env=env,
                           stdout=verbose_log, stderr=verbose_log)
    ok = False
    try:
        d = None
        for _ in range(MAX_WAIT * 10 // 3):
            time.sleep(0.3)
            if app.poll() is not None:
                break
            try:
                if d is None:
                    d = display.Display(f":{disp_no}")
                found = find_app_window(d)
                if found:
                    child, _g = found
                    # center + fixed frame size so every language matches
                    x = (SCREEN[0] - SHOT[0]) // 2
                    y = (SCREEN[1] - SHOT[1]) // 2
                    child.configure(x=x, y=y, width=SHOT[0], height=SHOT[1])
                    d.flush()
                    time.sleep(1.8)          # let GTK redraw at the new size
                    grab(d, x, y, SHOT[0], SHOT[1], out_path)
                    ok = True
                    break
            except Exception as e:
                print("poll err:", e)
        if d is not None:
            d.close()
    finally:
        app.terminate()
        try:
            app.wait(timeout=5)
        except Exception:
            app.kill()
        xvfb.terminate()
        try:
            xvfb.wait(timeout=5)
        except Exception:
            xvfb.kill()
        shutil.rmtree(home, ignore_errors=True)
    return ok


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    root_dir = os.path.abspath(sys.argv[1])
    out_dir = os.path.abspath(sys.argv[2])
    langs = sys.argv[3].split(",") if len(sys.argv) > 3 else ["tr", "en", "hi"]
    os.makedirs(out_dir, exist_ok=True)
    binary = f"{root_dir}/usr/bin/luxai"
    if not os.path.isfile(binary):
        sys.exit(f"luxai not found at {binary}")
    failures = []
    for lang in langs:
        out = f"{out_dir}/luxai-{lang}.png"
        with open(f"{out_dir}/luxai-{lang}.log", "w") as log:
            if capture_one(lang, root_dir, out, log):
                os.remove(f"{out_dir}/luxai-{lang}.log")
            else:
                failures.append(lang)
                print(f"FAILED to capture {lang}; see {out_dir}/luxai-{lang}.log")
    if failures:
        sys.exit(f"capture failed for: {','.join(failures)}")
    print("all screenshots captured")


if __name__ == "__main__":
    main()
