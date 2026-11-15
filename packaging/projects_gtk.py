# -*- coding: utf-8 -*-
"""LuxAI — "Projelerimiz" (Our projects) bolumu, GTK3 — modern kart gorunumu.

BELGESELSEMO ekosisteminin projelerini gercek uygulama simgeleriyle
(idm-linux deposundaki app_*.png / proj_*.png) kartlar halinde gosterir;
her karttaki "Ac" butonu ilgili adresi tarayicida acar. LuxAI hesap
penceresine sekmeli bir sayfa olarak baglanir; ayri pencere acmaz.
Simgeler: /usr/share/luxai/projects/*.png (paketle birlikte kurulur).
"""
import os

import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, GdkPixbuf

# (simge dosyasi, yedek rozet, baslik, kategori, adres, renk)
PROJECTS = [
    ("app_belgeselsemo.png", "BS", "BELGESELSEMO — Web Portalı (HD Belgesel & Araçlar)", "Web",
     "https://belgeselsemo.com.tr", "#0ea5e9"),
    ("proj_youtube.png", "YT", "BELGESELSEMO Learn & Explore — YouTube", "Web",
     "https://www.youtube.com/@belgeselsemo55", "#ef4444"),
    ("proj_matpay.png", "MP", "MATPAY — Matematik Paylaşımları (Lisans Notları)", "Web",
     "http://matpay.blogspot.com", "#6366f1"),
    ("proj_belgeselsemoflix.png", "FX", "BELGESELSEMOFLIX — Masaüstü Medya & Araç Üssü",
     "Linux · Windows · macOS",
     "https://github.com/vesvese55x/belgeselsemoflix", "#10b981"),
    ("app_epg.png", "EPG", "BELGESELSEMO TV Rehberi / TV Guide (EPG)", "Android",
     "https://play.google.com/store/apps/details?id=com.belgeselsemo.epgviewer", "#22c55e"),
    ("app_belgeselsemo.png", "BS", "BELGESELSEMO — HD Belgesel & 20+ Günlük Araç", "Android",
     "https://play.google.com/store/apps/details?id=com.belgeselsemo.tr", "#22c55e"),
    ("app_nb724.png", "N7", "Nöbetçi Eczaneler 7/24 TR-KKTC", "Android",
     "https://play.google.com/store/apps/details?id=com.belgeselsemo.nb724", "#ef4444"),
    ("app_nutuk.png", "NT", "NUTUK 1919-1927 — Sesli Kitap, Sözlük & Yarışma", "Android",
     "https://play.google.com/store/apps/details?id=com.belgeselsemo.nutuk", "#f59e0b"),
    ("app_trivia.png", "TR", "BELGESELSEMO TRIVIA — Online Bilgi Yarışması", "Android",
     "https://play.google.com/store/apps/details?id=com.belgeselsemo.trivia", "#a855f7"),
    ("proj_retro_icon.png", "RE", "BELGESELSEMO RETRO EMULATOR", "Coming soon",
     "https://belgeselsemo.com.tr", "#38bdf8"),
    ("proj_economy.png", "EC", "BELGESELSEMO ECONOMY", "Coming soon",
     "https://belgeselsemo.com.tr", "#10b981"),
]

ATTRIB = "BELGESELSEMO · Z.AI · belgeselsemo.com.tr"

ICON_DIRS = (
    "/usr/share/luxai/projects",
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "projects"),
)


def _tr(text):
    try:
        from luxos_i18n import _ as _
        return _(text)
    except Exception:
        return text


def _esc(t):
    return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def _icon_widget(fname, badge, col):
    for d in ICON_DIRS:
        p = os.path.join(d, fname)
        if os.path.isfile(p):
            try:
                pb = GdkPixbuf.Pixbuf.new_from_file_at_size(p, 48, 48)
                return Gtk.Image.new_from_pixbuf(pb)
            except Exception:
                pass
    lab = Gtk.Label()
    lab.set_markup('<span foreground="%s" weight="bold" size="large">%s</span>' % (col, badge))
    lab.set_size_request(48, 48)
    return lab


_CSS = b"""
.proj-card { background: @theme_base_color; border: 1px solid alpha(@theme_fg_color,0.18);
             border-radius: 12px; padding: 10px 12px; }
.proj-open { border-radius: 16px; padding: 4px 14px; font-weight: bold; }
"""


def projects_box(open_url_fn):
    """Kaydırılabilir, modern kartli proje listesi dondurur (Gtk.Box)."""
    try:
        prov = Gtk.CssProvider()
        prov.load_from_data(_CSS)
        Gtk.StyleContext.add_provider_for_screen(
            Gtk.Screen.get_default(), prov, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
    except Exception:
        pass

    outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
    head = Gtk.Label(xalign=0, wrap=True)
    head.set_markup("<b>%s</b>\n<small>%s</small>" % (
        _esc(_tr("Our projects")),
        _esc(_tr("Web portals, Android apps, desktop tools and a YouTube channel")) +
        " — " + ATTRIB))
    outer.pack_start(head, False, False, 0)

    sw = Gtk.ScrolledWindow(min_content_height=260, max_content_height=340)
    sw.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
    sw.set_hexpand(True)
    grid = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
    grid.set_hexpand(True)
    grid.set_margin_start(2)
    grid.set_margin_end(6)
    grid.set_margin_top(2)
    grid.set_margin_bottom(2)

    for fname, badge, title, cat, url, col in PROJECTS:
        card = Gtk.Box(spacing=10)
        card.get_style_context().add_class("proj-card")
        card.set_hexpand(True)
        card.pack_start(_icon_widget(fname, badge, col), False, False, 0)

        txt = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        txt.set_hexpand(True)
        t = Gtk.Label(xalign=0, wrap=True)
        t.set_markup("<b>%s</b>" % _esc(title))
        t.set_line_wrap(True)
        txt.pack_start(t, False, False, 0)
        c = Gtk.Label(xalign=0, wrap=True)
        c.set_markup('<small><span foreground="%s">●</span> %s</small>' % (col, _esc(cat)))
        txt.pack_start(c, False, False, 0)
        card.pack_start(txt, True, True, 0)

        btn = Gtk.Button(label=_tr("Open"))
        btn.get_style_context().add_class("proj-open")
        btn.set_size_request(76, 34)
        btn.set_hexpand(False)
        btn.set_valign(Gtk.Align.CENTER)
        btn.connect("clicked", lambda _b, u=url: open_url_fn(u))
        card.pack_start(btn, False, False, 0)
        grid.pack_start(card, False, False, 0)

    sw.add(grid)
    outer.pack_start(sw, True, True, 0)
    return outer
