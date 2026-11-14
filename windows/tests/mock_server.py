#!/usr/bin/env python3
"""LuxAI istemcisini test etmek icin sahte OpenAI uyumlu SSE sunucusu (yalniz test)."""
import json, sys
from http.server import BaseHTTPRequestHandler, HTTPServer

ROUND = {"n": 0}


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        body = json.loads(self.rfile.read(n) or b"{}")
        msgs = body.get("messages", [])
        has_tool_result = any(m.get("role") == "tool" for m in msgs)
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()

        def ev(obj):
            self.wfile.write(("data: " + json.dumps(obj) + "\n\n").encode())
            self.wfile.flush()

        def delta(**kw):
            ev({"choices": [{"index": 0, "delta": kw}]})

        if body.get("tools") and not has_tool_result:
            # 1. tur: arac cagrisi
            delta(role="assistant", content="")
            delta(tool_calls=[{"index": 0, "id": "call_test_1", "type": "function",
                               "function": {"name": "system_info", "arguments": ""}}])
            delta(tool_calls=[{"index": 0, "function": {"arguments": '{"x":1}'}}])
            ev({"choices": [{"index": 0, "delta": {}, "finish_reason": "tool_calls"}]})
        else:
            text = ("## Test yanıtı\n\nBu bir **akış** testidir. `kod` satır içi ve liste:\n\n"
                    "- birinci madde\n- ikinci madde\n\n```python\nprint('merhaba LuxAI')\n```\n\n"
                    "Sonuç: her şey çalışıyor.")
            if has_tool_result:
                text = "Araç çalıştı, sonucu aldım. " + text
            step = 7
            for i in range(0, len(text), step):
                delta(content=text[i:i + step])
                self.wfile.flush()
            ev({"choices": [{"index": 0, "delta": {}, "finish_reason": "stop"}]})
        self.wfile.write(b"data: [DONE]\n\n")
        self.wfile.flush()

    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(b'{"ok":true}')


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8777
    HTTPServer(("0.0.0.0", port), H).serve_forever()
