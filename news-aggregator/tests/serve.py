#!/usr/bin/env python3
"""Threaded static file server for the aggregator tests: serve.py <port> <dir>."""
import sys
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

if __name__ == "__main__":
    port = int(sys.argv[1])
    directory = sys.argv[2]
    handler = partial(SimpleHTTPRequestHandler, directory=directory)
    ThreadingHTTPServer(("127.0.0.1", port), handler).serve_forever()
