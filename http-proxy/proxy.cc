/**
 * File: proxy.cc
 * --------------
 * stproxy — a concurrent HTTP/1.x forward proxy with an in-memory cache and a
 * regex blocklist (CS110 assign7). Each accepted connection is serviced on the
 * ThreadPool. Built on raw BSD sockets + the HTTP model in http.{h,cc}; no
 * course helper libraries.
 *
 *   ./proxy <port> [blocklist-file]
 *
 * Behavior per request:
 *   - blocked host           -> 403 Forbidden
 *   - fresh cache hit (GET)  -> served from cache          (logs [HIT])
 *   - otherwise              -> forwarded to origin        (logs [MISS])
 *                               cacheable GET/200 responses are cached per
 *                               Cache-Control: max-age
 *   - an "x-forwarded-for: <client ip>" header is appended before forwarding.
 */

#include "http.h"
#include "thread-pool.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <regex>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <fstream>
#include <iostream>

#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace std;
using namespace std::chrono;

// --------------------------------------------------------------- logging
static mutex logMutex;
static void logLine(const string &s) {
  lock_guard<mutex> lg(logMutex);
  cerr << s << "\n";
  cerr.flush();
}

// --------------------------------------------------------------- sockets
static int createServerSocket(unsigned short port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;
  int on = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) { close(fd); return -1; }
  if (listen(fd, 128) < 0) { close(fd); return -1; }
  return fd;
}

static int createClientSocket(const string &host, unsigned short port) {
  addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo *res = nullptr;
  string portStr = to_string(port);
  if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0) return -1;
  int fd = -1;
  for (addrinfo *p = res; p != nullptr; p = p->ai_next) {
    fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd < 0) continue;
    if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
    close(fd);
    fd = -1;
  }
  freeaddrinfo(res);
  return fd;
}

static bool writeAll(int fd, const string &data) {
  size_t sent = 0;
  while (sent < data.size()) {
    ssize_t n = write(fd, data.data() + sent, data.size() - sent);
    if (n <= 0) return false;
    sent += n;
  }
  return true;
}

// --------------------------------------------------------------- cache
class HTTPCache {
 public:
  bool lookup(const string &key, string &serialized) {
    lock_guard<mutex> lg(m_);
    auto it = entries_.find(key);
    if (it == entries_.end()) return false;
    if (steady_clock::now() >= it->second.expires) { entries_.erase(it); return false; }
    serialized = it->second.serialized;
    return true;
  }
  void store(const string &key, const string &serialized, int ttlSeconds) {
    lock_guard<mutex> lg(m_);
    entries_[key] = {serialized, steady_clock::now() + seconds(ttlSeconds)};
  }

 private:
  struct Entry { string serialized; steady_clock::time_point expires; };
  unordered_map<string, Entry> entries_;
  mutex m_;
};

// --------------------------------------------------------------- blocklist
class Blocklist {
 public:
  bool load(const string &path) {
    ifstream in(path);
    if (!in) return false;
    string line;
    while (getline(in, line)) {
      if (line.empty() || line[0] == '#') continue;
      try { patterns_.emplace_back(line); } catch (const regex_error &) {}
    }
    return true;
  }
  bool blocks(const string &host) const {
    for (const regex &r : patterns_)
      if (regex_search(host, r)) return true;
    return false;
  }

 private:
  vector<regex> patterns_;
};

// --------------------------------------------------------------- helpers
static string cacheKey(const HTTPRequest &req) {
  return req.method + " " + req.target.host + ":" + to_string(req.target.port) + req.target.path;
}

// Decide cacheability + TTL from a GET/200 response's Cache-Control header.
static int cacheTTL(const HTTPRequest &req, const HTTPResponse &resp) {
  if (req.method != "GET" || resp.code() != 200) return 0;
  string cc = resp.headers().get("Cache-Control");
  if (cc.find("no-store") != string::npos || cc.find("no-cache") != string::npos ||
      cc.find("private") != string::npos)
    return 0;
  size_t p = cc.find("max-age=");
  if (p == string::npos) return 0;
  int ttl = atoi(cc.c_str() + p + 8);
  return ttl > 0 ? ttl : 0;
}

static void sendSimple(int fd, int code, const string &reason, const string &body) {
  HTTPResponse resp;
  resp.setProtocol("HTTP/1.0");
  resp.setStatus(code, reason);
  resp.headers().add("Content-Type", "text/plain");
  resp.headers().add("Connection", "close");
  resp.setBody(body);
  writeAll(fd, resp.serialize());
}

// --------------------------------------------------------------- per-connection
static HTTPCache cache;
static Blocklist blocklist;

static void serviceConnection(int clientfd, string clientIP) {
  SockReader reader(clientfd);
  HTTPRequest req;
  if (!req.parse(reader) || req.target.host.empty()) {
    close(clientfd);
    return;
  }

  string label = req.method + " " + req.target.host + ":" + to_string(req.target.port) + req.target.path;

  if (blocklist.blocks(req.target.host)) {
    logLine("[BLOCK] " + label);
    sendSimple(clientfd, 403, "Forbidden", "Forbidden: host is blocked by the proxy.\n");
    close(clientfd);
    return;
  }

  string key = cacheKey(req);
  string cached;
  if (req.method == "GET" && cache.lookup(key, cached)) {
    logLine("[HIT]   " + label);
    writeAll(clientfd, cached);
    close(clientfd);
    return;
  }

  // Prepare the forwarded request.
  req.headers.remove("Connection");
  req.headers.remove("Proxy-Connection");
  req.headers.add("Connection", "close");
  req.headers.add("x-forwarded-for", clientIP);

  int originfd = createClientSocket(req.target.host, req.target.port);
  if (originfd < 0) {
    logLine("[ERR]   " + label + "  (origin unreachable)");
    sendSimple(clientfd, 502, "Bad Gateway", "Bad Gateway: could not reach origin.\n");
    close(clientfd);
    return;
  }

  if (!writeAll(originfd, req.serialize())) {
    close(originfd);
    sendSimple(clientfd, 502, "Bad Gateway", "Bad Gateway: write to origin failed.\n");
    close(clientfd);
    return;
  }

  SockReader originReader(originfd);
  HTTPResponse resp;
  resp.ingestFrom(originReader);
  close(originfd);

  int ttl = cacheTTL(req, resp);
  string serialized = resp.serialize();
  if (ttl > 0) {
    cache.store(key, serialized, ttl);
    logLine("[MISS]  " + label + "  (cached " + to_string(ttl) + "s)");
  } else {
    logLine("[MISS]  " + label);
  }

  writeAll(clientfd, serialized);
  close(clientfd);
}

// --------------------------------------------------------------- main
int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <port> [blocklist-file]\n", argv[0]);
    return 1;
  }
  unsigned short port = static_cast<unsigned short>(atoi(argv[1]));
  if (argc >= 3) {
    if (blocklist.load(argv[2]))
      logLine(string("loaded blocklist from ") + argv[2]);
    else
      logLine(string("warning: could not load blocklist ") + argv[2]);
  }

  int serverfd = createServerSocket(port);
  if (serverfd < 0) {
    fprintf(stderr, "fatal: could not listen on port %u\n", port);
    return 1;
  }
  logLine("stproxy listening on port " + to_string(port));

  ThreadPool pool(16);
  while (true) {
    sockaddr_in caddr;
    socklen_t clen = sizeof(caddr);
    int clientfd = accept(serverfd, reinterpret_cast<sockaddr *>(&caddr), &clen);
    if (clientfd < 0) continue;
    char ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &caddr.sin_addr, ip, sizeof(ip));
    string clientIP(ip);
    pool.schedule([clientfd, clientIP] { serviceConnection(clientfd, clientIP); });
  }
  return 0;
}
