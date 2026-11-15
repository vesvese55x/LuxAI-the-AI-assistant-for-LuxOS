# -*- coding: utf-8 -*-
"""LuxAI — coklu OpenAI uyumlu saglayici destegi (Linux tarafi, yalnizca stdlib).

Windows surumuyle AYNI dosya bicimi kullanilir:

    # ~/.config/luxai/providers.ini
    sel=1
    p=OpenAI|https://api.openai.com/v1|sk-...|gpt-4o-mini|0

  sel  : etkin saglayici indeksi (-1 / yok = yerlesik LuxAI)
  p    : ad|endpoint|anahtar|model|ajan  (ajan: 1 ise ajan modunda da kullanilir)

Iki is yapar:
  1) ini dosyasini okur/yazar (GTK sekmesi bunu kullanir),
  2) kucuk bir yerel kopru (bridge) calistirir: LuxAI ikilisi LUXAI_ORIGIN ile
     bu kopruye baglanir; sohbet istekleri secili saglayiciya yonlendirilir,
     diger tum istekler (oturum, hesap, arama...) gercek sunucuya gecer.
     Boylece Windows'taki "sohbette saglayici degistirme" davranisi Linux'ta da
     BIREBIR calisir.
"""
import json
import os
import ssl
import sys
import threading
import time
import urllib.parse

MAX_PROV = 8
UPSTREAM_DEFAULT = "https://lux-ai.cz"
CHAT_PATH = "/api/ide/chat/completions"


# --------------------------------------------------------------------------- ini
def config_path():
    base = os.environ.get("XDG_CONFIG_HOME") or os.path.join(os.path.expanduser("~"), ".config")
    return os.path.join(base, "luxai", "providers.ini")


def _blank(name="", endpoint="", key="", model="", agent=0):
    return {"name": name, "endpoint": endpoint, "key": key, "model": model, "agent": int(agent or 0)}


def load(path=None):
    """(saglayicilar, secili_indeks) dondurur. Hatali satirlar sessizce atlanir."""
    path = path or config_path()
    provs, sel = [], -1
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            raw = fh.read()
    except OSError:
        return provs, sel
    for line in raw.splitlines():
        line = line.rstrip("\r")
        if line.startswith("sel="):
            try:
                sel = int(line[4:].strip())
            except ValueError:
                sel = -1
        elif line.startswith("p=") and len(provs) < MAX_PROV:
            parts = line[2:].split("|")
            if len(parts) < 2:
                continue
            p = _blank(parts[0], parts[1],
                       parts[2] if len(parts) > 2 else "",
                       parts[3] if len(parts) > 3 else "",
                       parts[4] if len(parts) > 4 else 0)
            if p["name"] and p["endpoint"]:
                provs.append(p)
    if sel >= len(provs):
        sel = -1
    return provs, sel


def save(provs, sel, path=None):
    path = path or config_path()
    os.makedirs(os.path.dirname(path), exist_ok=True)
    provs = [p for p in provs[:MAX_PROV] if p.get("name") and p.get("endpoint")]
    if sel is not None and not (0 <= sel < len(provs)):
        sel = -1
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as fh:
        fh.write("# LuxAI coklu saglayici yapilandirmasi (Windows ile ayni bicim)\n")
        fh.write("sel=%d\n" % (-1 if sel is None else sel))
        for p in provs:
            fields = [p.get("name", ""), p.get("endpoint", ""), p.get("key", ""),
                      p.get("model", ""), str(int(p.get("agent") or 0))]
            # ayirici karakter alanlarda gecmemeli
            fields = [f.replace("|", "/").replace("\n", " ") for f in fields]
            fh.write("p=" + "|".join(fields) + "\n")
    os.replace(tmp, path)
    try:
        os.chmod(path, 0o600)          # anahtar icerir
    except OSError:
        pass
    return provs, sel


def active(path=None):
    provs, sel = load(path)
    if 0 <= sel < len(provs) and provs[sel]["endpoint"]:
        p = dict(provs[sel])
        p["index"] = sel
        return p
    return None


# ------------------------------------------------------------------------ bridge
def _split(url):
    u = urllib.parse.urlsplit(url)
    scheme = u.scheme or "https"
    host = u.hostname or "localhost"
    port = u.port or (443 if scheme == "https" else 80)
    path = u.path or "/"
    if u.query:
        path += "?" + u.query
    return scheme, host, port, path


class _Handler(object):
    """http.server.BaseHTTPRequestHandler alt sinifi main() icinde uretilir."""

    protocol_version = "HTTP/1.0"
    server_version = "LuxAI-bridge"
    upstream = UPSTREAM_DEFAULT
    quiet = True

    def log_message(self, fmt, *args):  # noqa: D401 - gurultuyu kapat
        if not self.quiet:
            sys.stderr.write("bridge: " + (fmt % args) + "\n")

    # --- HTTP metotlari
    def do_GET(self):
        self._handle("GET")

    def do_POST(self):
        self._handle("POST")

    def do_PUT(self):
        self._handle("PUT")

    def do_PATCH(self):
        self._handle("PATCH")

    def do_DELETE(self):
        self._handle("DELETE")

    def do_HEAD(self):
        self._handle("HEAD")

    def do_OPTIONS(self):
        self._handle("OPTIONS")

    # --- govde
    def _read_body(self):
        n = self.headers.get("Content-Length")
        try:
            n = int(n)
        except (TypeError, ValueError):
            n = 0
        if n <= 0:
            return b""
        data = b""
        while len(data) < n:
            chunk = self.rfile.read(n - len(data))
            if not chunk:
                break
            data += chunk
        return data

    def _handle(self, method):
        try:
            body = self._read_body()
            prov = None
            if CHAT_PATH in self.path:
                try:
                    prov = active()
                except Exception:
                    prov = None
            if prov:
                self._to_provider(method, prov, body)
            else:
                self._to_upstream(method, body)
        except (BrokenPipeError, ConnectionResetError):
            pass
        except Exception as exc:  # kopru asla cokmemeli
            try:
                self.send_error(502, "bridge: %s" % exc)
            except Exception:
                pass

    # --- gercek sunucuya oldugu gibi aktar
    def _to_upstream(self, method, body):
        scheme, host, port, path = _split(self.upstream)
        self._relay(method, scheme, host, port, path.rstrip("/") + self.path, self._fwd_headers(), body)

    # --- secili OpenAI uyumlu saglayiciya aktar
    def _to_provider(self, method, prov, body):
        endpoint = prov["endpoint"].strip()
        if not endpoint.lower().startswith(("http://", "https://")):
            endpoint = "https://" + endpoint
        if endpoint.rstrip("/").endswith("/chat/completions"):
            url = endpoint
        else:
            url = endpoint.rstrip("/") + "/chat/completions"
        scheme, host, port, path = _split(url)

        data = body
        try:
            obj = json.loads(body.decode("utf-8"))
            if isinstance(obj, dict):
                if prov.get("model"):
                    obj["model"] = prov["model"]
                obj.pop("agent", None)
                if "stream" in obj:
                    obj["stream"] = True
                data = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        except Exception:
            pass

        headers = {
            "Content-Type": "application/json; charset=utf-8",
            "Accept": "text/event-stream, application/json",
            "X-Lux-Client": "luxai",
        }
        key = prov.get("key") or ""
        if key:
            headers["Authorization"] = "Bearer " + key
            if "azure" in host:                       # Azure OpenAI
                headers["api-key"] = key
            if "anthropic" in host:                   # Anthropic uyumlu gecit
                headers["anthropic-version"] = "2023-06-01"
        self._relay(method, scheme, host, port, path, headers, data)

    def _fwd_headers(self):
        drop = {"host", "connection", "content-length", "keep-alive", "transfer-encoding", "accept-encoding"}
        out = {}
        for k, v in self.headers.items():
            if k.lower() in drop:
                continue
            out[k] = v
        return out

    # --- ortak aktarim (SSE dahil, parca parca)
    def _relay(self, method, scheme, host, port, path, headers, body):
        import http.client
        if scheme == "https":
            conn = http.client.HTTPSConnection(host, port, timeout=600,
                                               context=ssl.create_default_context())
        else:
            conn = http.client.HTTPConnection(host, port, timeout=600)
        try:
            conn.putrequest(method, path, skip_host=True, skip_accept_encoding=True)
            conn.putheader("Host", host if port in (80, 443) else "%s:%d" % (host, port))
            for k, v in headers.items():
                conn.putheader(k, v)
            if body:
                conn.putheader("Content-Length", str(len(body)))
            conn.endheaders()
            if body:
                conn.send(body)
            resp = conn.getresponse()
            self.send_response(resp.status)
            for k, v in resp.getheaders():
                if k.lower() in ("transfer-encoding", "connection", "content-length", "server", "date"):
                    continue
                self.send_header(k, v)
            self.send_header("Cache-Control", "no-store")
            self.send_header("Connection", "close")
            self.end_headers()
            if method != "HEAD":
                while True:
                    chunk = resp.read(2048)
                    if not chunk:
                        break
                    self.wfile.write(chunk)
                    self.wfile.flush()
        finally:
            try:
                conn.close()
            except Exception:
                pass


def make_handler(upstream=UPSTREAM_DEFAULT, quiet=True):
    import http.server
    cls = type("LuxBridgeHandler", (_Handler, http.server.BaseHTTPRequestHandler),
               {"upstream": upstream, "quiet": quiet})
    return cls


def serve(port=0, port_file=None, parent_pid=0, upstream=UPSTREAM_DEFAULT, quiet=True):
    import http.server
    handler = make_handler(upstream, quiet)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", port), handler)
    srv.daemon_threads = True
    real_port = srv.server_address[1]
    if port_file:
        try:
            with open(port_file, "w", encoding="utf-8") as fh:
                fh.write(str(real_port))
        except OSError:
            pass
    if parent_pid:
        def _watch(pid=parent_pid):
            while True:
                time.sleep(1.0)
                try:
                    os.kill(pid, 0)
                except OSError:
                    try:
                        srv.shutdown()
                    except Exception:
                        pass
                    os._exit(0)
        threading.Thread(target=_watch, daemon=True).start()
    try:
        srv.serve_forever(poll_interval=0.3)
    except KeyboardInterrupt:
        pass
    return real_port


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    port, port_file, parent, upstream, quiet = 0, None, 0, UPSTREAM_DEFAULT, True
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--port" and i + 1 < len(argv):
            port = int(argv[i + 1]); i += 2
        elif a == "--port-file" and i + 1 < len(argv):
            port_file = argv[i + 1]; i += 2
        elif a == "--parent" and i + 1 < len(argv):
            parent = int(argv[i + 1]); i += 2
        elif a == "--upstream" and i + 1 < len(argv):
            upstream = argv[i + 1]; i += 2
        elif a == "--verbose":
            quiet = False; i += 1
        elif a in ("-h", "--help"):
            print("usage: providers_bridge.py [--port N] [--port-file PATH] [--parent PID] [--upstream URL]")
            return 0
        else:
            i += 1
    serve(port=port, port_file=port_file, parent_pid=parent, upstream=upstream, quiet=quiet)
    return 0


if __name__ == "__main__":
    sys.exit(main())


# ---------------------------------------------------------------- yedek / restore
BACKUP_FORMAT = "LuxAI-providers-backup"
BACKUP_VERSION = 1


def export_backup(path_out, provs=None, sel=None):
    """Tüm sağlayıcıları (tüm verileriyle) LuxAI yedek dosyasına yazar (JSON).

    Dosya biçimi programın kendi biçimidir; restore ile geri yüklenir.
    Ayrıca eski .ini yedekleri de restore edilebilir (bkz. import_backup).
    """
    if provs is None:
        provs, sel = load()
    data = {
        "format": BACKUP_FORMAT,
        "version": BACKUP_VERSION,
        "app": "LuxAI",
        "sel": (-1 if sel is None else int(sel)),
        "providers": [
            {"name": p.get("name", ""), "endpoint": p.get("endpoint", ""),
             "key": p.get("key", ""), "model": p.get("model", ""),
             "agent": int(p.get("agent") or 0)}
            for p in provs[:MAX_PROV] if p.get("name") and p.get("endpoint")
        ],
    }
    with open(path_out, "w", encoding="utf-8") as fh:
        json.dump(data, fh, ensure_ascii=False, indent=2)
        fh.write("\n")
    try:
        os.chmod(path_out, 0o600)      # icinde API anahtarlari var
    except OSError:
        pass
    return data


def import_backup(path_in):
    """Bir yedek dosyasını (JSON yedek veya providers.ini) okuyup kaydeder.

    (saglayicilar, secili) dondurur; bicim taninmazsa ValueError firlatir.
    """
    with open(path_in, encoding="utf-8", errors="replace") as fh:
        raw = fh.read()
    provs, sel = [], -1
    txt = raw.strip()
    if txt.startswith("{"):
        data = json.loads(txt)
        if not isinstance(data, dict) or "providers" not in data:
            raise ValueError("not a LuxAI providers backup")
        for p in data.get("providers") or []:
            if not isinstance(p, dict):
                continue
            rec = _blank(p.get("name", ""), p.get("endpoint", ""), p.get("key", ""),
                         p.get("model", ""), p.get("agent", 0))
            if rec["name"] and rec["endpoint"]:
                provs.append(rec)
        try:
            sel = int(data.get("sel", -1))
        except (TypeError, ValueError):
            sel = -1
    elif "p=" in raw or "sel=" in raw:
        # ini bicimi: gecici dosyaya yazip ayni ayristiriciyi kullan
        import tempfile
        fd, tmp = tempfile.mkstemp(suffix=".ini")
        try:
            with os.fdopen(fd, "w", encoding="utf-8") as fh:
                fh.write(raw)
            provs, sel = load(tmp)
        finally:
            try:
                os.unlink(tmp)
            except OSError:
                pass
    else:
        raise ValueError("not a LuxAI providers backup")
    provs = provs[:MAX_PROV]
    if not (0 <= sel < len(provs)):
        sel = -1
    return save(provs, sel)

