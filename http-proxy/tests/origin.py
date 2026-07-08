#!/usr/bin/env python3
"""
A tiny local origin server for the proxy tests. It:
  - counts how many requests actually reach it (proves cache hits skip origin),
  - echoes back the x-forwarded-for header the proxy injected,
  - marks normal pages `Cache-Control: max-age=300` (cacheable),
  - serves /count as `no-store` so it always reports the true origin hit count.
Threaded so the concurrency test can hit it in parallel.
"""
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

lock = threading.Lock()
hits = 0


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def _send(self, body: bytes, cache_control: str):
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", cache_control)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        global hits
        with lock:
            hits += 1
            current = hits
        xff = self.headers.get("x-forwarded-for", "none")
        if self.path == "/count":
            self._send(f"origin-hits={current}\n".encode(), "no-store")
        else:
            body = f"path={self.path} xff={xff} origin-hits={current}\n".encode()
            self._send(body, "max-age=300")

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    port = int(sys.argv[1])
    ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
