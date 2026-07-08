# assign7 — HTTP Web Proxy and Cache

`stproxy` — a concurrent HTTP/1.x **forward proxy** with an in-memory response
cache and a regex blocklist. Built on raw BSD sockets and a from-scratch HTTP
message model (`http.{h,cc}`); each connection is serviced on the assign6
`ThreadPool`. No course helper libraries (no socket++, no HTTPRequest/-Response).

```bash
make
./proxy <port> [blocklist-file]
# e.g.  ./proxy 8110 blocklist.txt
curl -x http://localhost:8110 http://example.com/
```

## What it does

- **Forwards** any method to the origin named in the (absolute- or origin-form)
  request URL and relays the response back to the client.
- **Concurrency:** a 16-thread `ThreadPool` handles many clients at once.
- **Caching:** a `GET`/`200` response with `Cache-Control: max-age=N` is cached
  for `N` seconds (respecting `no-store`/`no-cache`/`private`); a fresh hit is
  served without contacting the origin.
- **Blocklist:** hosts matching any regex in the blocklist file get `403`.
- **Header injection:** appends `x-forwarded-for: <client ip>` before forwarding.
- Origin unreachable / write failure → `502 Bad Gateway`.

## HTTP model (`http.{h,cc}`)

`SockReader` buffers a socket fd and offers `readLine` (headers) plus byte- and
EOF-bounded body reads, so a header parse never over-consumes the body.
`HTTPHeader` is a case-insensitive, order-preserving header list. `HTTPRequest`
splits `http://host:port/path` (falling back to the `Host` header for
origin-form requests) and reads any `Content-Length` body; `HTTPResponse`
parses the status line and reads the body by `Content-Length` or until EOF.

## Verification (WSL2, Ubuntu 24.04, g++ 13.3), `-Wall -Wextra -Werror` clean

`make test` starts `tests/origin.py` (a threaded local origin) and the proxy,
then drives `curl` through the proxy. Captured in `../results/assign7_proxy.txt`:

| Test | Result |
|---|---|
| forward: GET returns origin content | ✓ |
| `x-forwarded-for` injected (`127.0.0.1`) | ✓ |
| cache: 2nd GET is byte-identical (served from cache) | ✓ |
| cache: origin contacted **once** for `/page` (verified via a `no-store` counter) | ✓ |
| blocklist: blocked host → `403` | ✓ |
| concurrency: 25 parallel requests all served | ✓ |
| log records `[HIT]`, `[MISS]`, `[BLOCK]` | ✓ |

**9/9 pass.** The origin (`tests/origin.py`) counts real hits, so the cache
proof is causal: after two `/page` requests the `no-store` `/count` endpoint
reports the origin was hit only once.

## Notes / scope

The cache is in memory (the reference proxy caches to disk); TTL comes from
`Cache-Control: max-age`. HTTPS `CONNECT` tunneling is out of scope — this is a
plaintext HTTP forward proxy, exercised against a local origin (no external
network required).
