# -*- coding: utf-8 -*-
"""LuxAI — "Projelerimiz" (Our projects) bolumu, GTK3.

BELGESELSEMO ekosisteminin projelerini tek listede gosterir; her satir ilgili
adresi tarayicida acar. LuxAI hesap penceresine (luxai_account_gtk.py) sekmeli
bir sayfa olarak baglanir; ayri pencere acmaz.
"""
import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk

# (rozet, baslik, kategori, adres, renk)
PROJECTS = [
    ("BS", "BELGESELSEMO — Web Portalı (HD Belgesel & Araçlar)", "Web",
     "https://belgeselsemo.com.tr", "#0ea5e9"),
    ("YT", "BELGESELSEMO Learn & Explore — YouTube", "Web",
     "https://www.youtube.com/@belgeselsemo55", "#ef4444"),
    ("MP", "MATPAY — Matematik Paylaşımları (Lisans Notları)", "Web",
     "http://matpay.blogspot.com", "#6366f1"),
    ("FX", "BELGESELSEMOFLIX — Masaüstü Medya & Araç Üssü", "Linux · Windows · macOS",
     "https://github.com/vesvese55x/belgeselsemoflix", "#10b981"),
    ("EPG", "BELGESELSEMO TV Rehberi / TV Guide (EPG)", "Android",
     "https://play.google.com/store/apps/details?id=com.belgeselsemo.epgviewer", "#22c55e"),
    ("BS", "BELGESELSEMO — HD Belgesel & 20+ Günlük Araç", "Android",
     "https://play.google.com/store/apps/details?id=com.belgeselsemo.tr", "#22c55e"),
    ("N7", "Nöbetçi Eczaneler 7/24 TR-KKTC", "Android",
     "https://play.google.com/store/apps/details?id=com.belgeselsemo.nb724", "#ef4444"),
    ("NT", "NUTUK 1919-1927 — Sesli Kitap, Sözlük & Yarışma", "Android",
     "https://play.google.com/store/apps/details?id=com.belgeselsemo.nutuk", "#f59e0b"),
    ("TR", "BELGESELSEMO TRIVIA — Online Bilgi Yarışması", "Android",
     "https://play.google.com/store/apps/details?id=com.belgeselsemo.trivia", "#a855f7"),
    ("RE", "BELGESELSEMO RETRO EMULATOR", "Coming soon",
     "https://belgeselsemo.com.tr", "#38bdf8"),
    ("EC", "BELGESELSEMO ECONOMY", "Coming soon",
     "https://belgeselsemo.com.tr", "#10b981"),
]

ATTRIB = "BELGESELSEMO · Z.AI · belgeselsemo.com.tr"


def _tr(text):
    try:
        from luxos_i18n import _ as _
        return _(text)
    except Exception:
        return text


def projects_box(open_url_fn):
    """Kaydırılabilir proje listesi dondurur (Gtk.Box)."""
    outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
    head = Gtk.Label(xalign=0, wrap=True, max_width_chars=64)
    head.set_markup("<b>%s</b>\n<small>%s</small>" % (
        _tr("Our projects"),
        _tr("Web portals, Android apps, desktop tools and a YouTube channel") +
        " — " + ATTRIB))
    outer.pack_start(head, False, False, 0)

    sw = Gtk.ScrolledWindow(min_content_height=260, max_content_height=320)
    sw.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
    grid = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
    for badge, title, cat, url, col in PROJECTS:
        row = Gtk.Box(spacing=10)
        lab = Gtk.Label(label=badge)
        lab.set_width_chars(4)
        lab.set_markup('<span foreground="%s" weight="bold">%s</span>' % (col, badge))
        row.pack_start(lab, False, False, 0)
        txt = Gtk.Label(xalign=0, wrap=True, max_width_chars=48)
        txt.set_markup("%s\n<small>%s</small>" % (
            Gtk.glue_escape(title) if hasattr(Gtk, "glue_escape") else
            title.replace("&", "&amp;").replace("<", "&lt;"), cat))
        row.pack_start(txt, True, True, 0)
        btn = Gtk.Button(label=_tr("Open"))
        btn.connect("clicked", lambda _b, u=url: open_url_fn(u))
        row.pack_start(btn, False, False, 0)
        grid.pack_start(row, False, False, 0)
    sw.add(grid)
    outer.pack_start(sw, True, True, 0)
    return outer
