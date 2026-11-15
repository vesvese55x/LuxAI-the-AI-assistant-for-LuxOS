# -*- coding: utf-8 -*-
"""LuxAI — "Saglayicilar" sekmesi (GTK3), Windows surumuyle birebir.

Windows'ta sohbet basligindaki model secicisinden yapilan islemlerin Linux
karsiligidir: ad + uc nokta (base_url) + API anahtari + model girilir, secim
aninda uygulanir ve `~/.config/luxai/providers.ini` dosyasinda saklanir
(Windows'taki `%APPDATA%\\providers.ini` ile ayni bicim). Secili saglayici
sohbet isteklerini devralir; yerlesik LuxAI'ye donmek icin ilk satir secilir.

Kopru (bridge) calistigi icin secim aninda gecerli olur — uygulamayi yeniden
baslatmak gerekmez.
"""
import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, GLib

try:
    import providers_common as _pc
except Exception:                                    # paket disi calistirma
    _pc = None


def _tr(text):
    try:
        from luxos_i18n import _ as _
        return _(text)
    except Exception:
        return text


def _css(widget, *classes):
    for c in classes:
        try:
            widget.get_style_context().add_class(c)
        except Exception:
            pass
    return widget


class _Panel(Gtk.Box):
    def __init__(self):
        Gtk.Box.__init__(self, orientation=Gtk.Orientation.VERTICAL, spacing=8)
        self.provs, self.sel = ([], -1)
        self.editing = -1
        self.stack = Gtk.Stack(transition_type=Gtk.StackTransitionType.SLIDE_LEFT_RIGHT,
                               transition_duration=150)
        self.stack.add_named(self._build_list(), "list")
        self.stack.add_named(self._build_form(), "form")
        self.pack_start(self.stack, True, True, 0)
        self.reload()

    # ------------------------------------------------------------------ veri
    def reload(self):
        if _pc is None:
            self.provs, self.sel = [], -1
        else:
            try:
                self.provs, self.sel = _pc.load()
            except Exception:
                self.provs, self.sel = [], -1
        self._fill_rows()

    def store(self):
        if _pc is None:
            return
        try:
            self.provs, self.sel = _pc.save(self.provs, self.sel)
        except Exception:
            pass

    def _status_text(self):
        if _pc is None:
            return _tr("Provider support is not available in this build.")
        if 0 <= self.sel < len(self.provs):
            return _tr("Active provider") + ": " + self.provs[self.sel]["name"] + \
                   "  ·  " + self.provs[self.sel].get("model", "")
        return _tr("Active provider") + ": LuxAI (" + _tr("built-in") + ")"

    # ------------------------------------------------------------------ liste
    def _build_list(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        head = Gtk.Label(xalign=0)
        head.set_markup("<b>" + GLib.markup_escape_text(_tr("Providers")) + "</b>")
        box.pack_start(head, False, False, 0)
        hint = _css(Gtk.Label(xalign=0), "dim", "small")
        hint.set_line_wrap(True)
        hint.set_text(_tr("Add any OpenAI-compatible service (name, endpoint, API key, model). "
                          "The selected provider answers your chat messages; the first row "
                          "switches back to built-in LuxAI."))
        box.pack_start(hint, False, False, 0)

        self.status = _css(Gtk.Label(xalign=0), "dim", "small")
        box.pack_start(self.status, False, False, 0)

        sw = Gtk.ScrolledWindow()
        sw.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        sw.set_min_content_height(170)
        sw.set_max_content_height(250)
        sw.set_propagate_natural_height(True)
        self.listbox = Gtk.ListBox(selection_mode=Gtk.SelectionMode.NONE)
        self.listbox.set_header_func(self._sep)
        sw.add(self.listbox)
        box.pack_start(sw, True, True, 0)

        row = Gtk.Box(spacing=6)
        btn = Gtk.Button(label="+ " + _tr("Add provider"))
        _css(btn, "primary", "pill")
        btn.connect("clicked", lambda *_: self.open_form(-1))
        row.pack_start(btn, False, False, 0)
        # Yedekle / Restore Et: Ekle butonuyla ayni tasarimda, hafif mesafeyle
        bak_btn = Gtk.Button(label=_tr("Backup"))
        _css(bak_btn, "pill")
        bak_btn.set_margin_start(4)
        bak_btn.connect("clicked", lambda *_: self.backup())
        row.pack_start(bak_btn, False, False, 0)
        res_btn = Gtk.Button(label=_tr("Restore"))
        _css(res_btn, "pill")
        res_btn.connect("clicked", lambda *_: self.restore())
        row.pack_start(res_btn, False, False, 0)
        self.del_btn = Gtk.Button(label=_tr("Delete"))
        self.del_btn.connect("clicked", lambda *_: self.delete(self.sel))
        row.pack_end(self.del_btn, False, False, 0)
        box.pack_start(row, False, False, 0)
        return box

    @staticmethod
    def _sep(row, before):
        if not before:
            row.set_header(None)
            return
        h = Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL)
        row.set_header(h)

    # ------------------------------------------------------------ yedek/restore
    def _dialog(self, title, text, kind=Gtk.MessageType.INFO):
        dlg = Gtk.MessageDialog(transient_for=self.get_toplevel(),
                                modal=True, message_type=kind,
                                buttons=Gtk.ButtonsType.OK, text=title)
        if text:
            dlg.format_secondary_text(text)
        dlg.run()
        dlg.destroy()

    def backup(self):
        if _pc is None:
            self._dialog(_tr("Backup"), _tr("Provider support is not available in this build."),
                         Gtk.MessageType.WARNING)
            return
        chooser = Gtk.FileChooserDialog(
            title=_tr("Back up providers"), transient_for=self.get_toplevel(),
            action=Gtk.FileChooserAction.SAVE)
        chooser.add_buttons(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                            Gtk.STOCK_SAVE, Gtk.ResponseType.OK)
        chooser.set_do_overwrite_confirmation(True)
        chooser.set_current_name("luxai-providers-backup.json")
        filt = Gtk.FileFilter()
        filt.set_name("LuxAI (*.json, *.ini)")
        filt.add_pattern("*.json")
        filt.add_pattern("*.ini")
        chooser.add_filter(filt)
        try:
            if chooser.run() == Gtk.ResponseType.OK:
                path = chooser.get_filename()
                if path:
                    if not (path.endswith(".json") or path.endswith(".ini")):
                        path += ".json"
                    _pc.export_backup(path, self.provs, self.sel)
                    self._dialog(_tr("Backup"),
                                 _tr("Providers were backed up to:") + "\n" + path)
        except Exception as exc:                       # noqa: BLE001
            self._dialog(_tr("Backup"), str(exc), Gtk.MessageType.ERROR)
        finally:
            chooser.destroy()

    def restore(self):
        if _pc is None:
            self._dialog(_tr("Restore"), _tr("Provider support is not available in this build."),
                         Gtk.MessageType.WARNING)
            return
        chooser = Gtk.FileChooserDialog(
            title=_tr("Restore providers from backup"), transient_for=self.get_toplevel(),
            action=Gtk.FileChooserAction.OPEN)
        chooser.add_buttons(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                            Gtk.STOCK_OPEN, Gtk.ResponseType.OK)
        filt = Gtk.FileFilter()
        filt.set_name("LuxAI (*.json, *.ini)")
        filt.add_pattern("*.json")
        filt.add_pattern("*.ini")
        chooser.add_filter(filt)
        try:
            if chooser.run() == Gtk.ResponseType.OK:
                path = chooser.get_filename()
                if path:
                    try:
                        self.provs, self.sel = _pc.import_backup(path)
                        self._fill_rows()
                        self._dialog(_tr("Restore"),
                                     _tr("Backup restored. Providers are now listed in the Providers tab."))
                    except Exception as exc:           # noqa: BLE001
                        self._dialog(_tr("Restore"),
                                     _tr("This file is not a valid LuxAI providers backup.") +
                                     "\n" + str(exc), Gtk.MessageType.ERROR)
        finally:
            chooser.destroy()


    def _fill_rows(self):
        for ch in self.listbox.get_children():
            self.listbox.remove(ch)
        self._group = None          # radyo grubu her yenilemede bastan kurulur
        self.listbox.add(self._row(-1))
        for i, p in enumerate(self.provs):
            self.listbox.add(self._row(i))
        self.status.set_text(self._status_text())
        self.del_btn.set_sensitive(0 <= self.sel < len(self.provs))
        self.listbox.show_all()

    def _row(self, idx):
        p = self.provs[idx] if idx >= 0 else None
        row = Gtk.ListBoxRow(activatable=False)
        box = _css(Gtk.Box(spacing=8), "srow")
        rb = Gtk.RadioButton(group=getattr(self, "_group", None), draw_indicator=True)
        if getattr(self, "_group", None) is None:
            self._group = rb
        rb.set_active((idx == self.sel) or (idx < 0 and self.sel < 0))
        rb.connect("toggled", lambda w, i=idx: w.get_active() and self.select(i))
        box.pack_start(rb, False, False, 0)
        col = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=1)
        title = Gtk.Label(xalign=0)
        name = p["name"] if p else "LuxAI (" + _tr("built-in") + ")"
        title.set_markup("<b>" + GLib.markup_escape_text(name) + "</b>")
        sub = _css(Gtk.Label(xalign=0), "dim", "small")
        sub.set_text(("%s  ·  %s" % (p.get("endpoint", ""), p.get("model") or "-")) if p
                     else _tr("Original LuxAI cloud (flash / lite)"))
        sub.set_ellipsize(3)
        col.pack_start(title, False, False, 0)
        col.pack_start(sub, False, False, 0)
        box.pack_start(col, True, True, 0)
        if p is not None:
            ed = Gtk.Button(label=_tr("Edit"))
            _css(ed, "flat-small")
            ed.connect("clicked", lambda w, i=idx: self.open_form(i))
            box.pack_end(ed, False, False, 0)
        row.add(box)
        return row

    def select(self, idx):
        self.sel = idx if (0 <= idx < len(self.provs)) else -1
        self.store()
        self.status.set_text(self._status_text())
        self.del_btn.set_sensitive(self.sel >= 0)

    # ------------------------------------------------------------------- form
    def _build_form(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        self.form_title = Gtk.Label(xalign=0)
        box.pack_start(self.form_title, False, False, 0)

        grid = Gtk.Grid(column_spacing=10, row_spacing=6)
        self.e_name = Gtk.Entry()
        self.e_name.set_placeholder_text("OpenAI")
        self.e_url = Gtk.Entry()
        self.e_url.set_placeholder_text("https://api.openai.com/v1")
        self.e_key = Gtk.Entry()
        self.e_key.set_visibility(False)
        self.e_key.set_input_purpose(Gtk.InputPurpose.PASSWORD)
        self.e_key.set_placeholder_text("sk-...")
        self.e_model = Gtk.Entry()
        self.e_model.set_placeholder_text("gpt-4o-mini")
        rows = ((_tr("Name"), self.e_name, _tr("A label you choose")),
                (_tr("Endpoint (base URL)"), self.e_url, _tr("Everything before /chat/completions")),
                (_tr("API key"), self.e_key, _tr("Stored only on this computer (0600)")),
                (_tr("Model"), self.e_model, _tr("Model name sent to the provider")))
        for r, (lab, ent, hint) in enumerate(rows):
            grid.attach(Gtk.Label(label=lab, xalign=0), 0, r * 2, 1, 1)
            grid.attach(ent, 1, r * 2, 1, 1)
            h = _css(Gtk.Label(label=hint, xalign=0), "dim", "small")
            grid.attach(h, 1, r * 2 + 1, 1, 1)
        box.pack_start(grid, False, False, 0)

        self.show_key = Gtk.CheckButton(label=_tr("Show API key"))
        self.show_key.connect("toggled", lambda w: self.e_key.set_visibility(w.get_active()))
        box.pack_start(self.show_key, False, False, 0)

        self.warn = _css(Gtk.Label(xalign=0), "warn-text", "small")
        self.warn.set_line_wrap(True)
        box.pack_start(self.warn, False, False, 0)

        row = Gtk.Box(spacing=6)
        save = Gtk.Button(label=_tr("Save"))
        _css(save, "primary", "pill")
        save.connect("clicked", lambda *_: self.commit())
        cancel = Gtk.Button(label=_tr("Cancel"))
        cancel.connect("clicked", lambda *_: self.stack.set_visible_child_name("list"))
        row.pack_start(save, False, False, 0)
        row.pack_start(cancel, False, False, 0)
        box.pack_start(row, False, False, 0)
        return box

    def open_form(self, idx):
        self.editing = idx
        p = self.provs[idx] if 0 <= idx < len(self.provs) else None
        self.form_title.set_markup("<b>%s</b>" % GLib.markup_escape_text(
            _tr("Add provider") if p is None else _tr("Edit provider")))
        self.e_name.set_text(p["name"] if p else "")
        self.e_url.set_text(p.get("endpoint", "") if p else "")
        self.e_key.set_text(p.get("key", "") if p else "")
        self.e_model.set_text(p.get("model", "") if p else "")
        self.warn.set_text("")
        self.stack.set_visible_child_name("form")
        self.e_name.grab_focus()

    def commit(self):
        name = self.e_name.get_text().strip()
        url = self.e_url.get_text().strip()
        key = self.e_key.get_text().strip()
        model = self.e_model.get_text().strip()
        if not name:
            self.warn.set_text(_tr("Please enter a name."))
            return
        if not url.lower().startswith(("http://", "https://")):
            self.warn.set_text(_tr("The endpoint must start with http:// or https://"))
            return
        rec = {"name": name, "endpoint": url.rstrip("/"), "key": key, "model": model, "agent": 0}
        if 0 <= self.editing < len(self.provs):
            self.provs[self.editing] = rec
            self.sel = self.editing
        else:
            if len(self.provs) >= 8:
                self.warn.set_text(_tr("At most 8 providers can be stored."))
                return
            self.provs.append(rec)
            self.sel = len(self.provs) - 1
        self.store()
        self.reload()
        self.stack.set_visible_child_name("list")

    def delete(self, idx):
        if not (0 <= idx < len(self.provs)):
            return
        del self.provs[idx]
        if self.sel == idx:
            self.sel = -1
        elif self.sel > idx:
            self.sel -= 1
        self.store()
        self.reload()


def providers_box(open_url_fn=None):
    """Hesap penceresine eklenecek sekme icerigi (kaydirma panelin icinde)."""
    return _Panel()
