/**
 * File: fetch.cc
 * --------------
 * Implementation of the tiny HTTP GET client (see fetch.h).
 */

#include "fetch.h"
#include "http.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>

URL parseHttpURL(const std::string &url) {
  URL u;
  std::string rest = url;
  size_t scheme = rest.find("://");
  if (scheme != std::string::npos) rest = rest.substr(scheme + 3);

  size_t slash = rest.find('/');
  std::string authority = (slash == std::string::npos) ? rest : rest.substr(0, slash);
  u.path = (slash == std::string::npos) ? "/" : rest.substr(slash);

  size_t colon = authority.find(':');
  if (colon == std::string::npos) {
    u.host = authority;
    u.port = 80;
  } else {
    u.host = authority.substr(0, colon);
    u.port = static_cast<unsigned short>(std::atoi(authority.c_str() + colon + 1));
  }
  return u;
}

static int connectTo(const std::string &host, unsigned short port) {
  addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo *res = nullptr;
  if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0) return -1;
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

bool fetchURL(const std::string &url, std::string &body) {
  if (url.rfind("http://", 0) != 0 && url.find("://") != std::string::npos) return false;
  URL u = parseHttpURL(url);
  if (u.host.empty()) return false;

  int fd = connectTo(u.host, u.port);
  if (fd < 0) return false;

  std::string req = "GET " + u.path + " HTTP/1.0\r\n" +
                    "Host: " + u.host + "\r\n" +
                    "User-Agent: stnewsaggregator/1.0\r\n" +
                    "Connection: close\r\n\r\n";
  size_t sent = 0;
  while (sent < req.size()) {
    ssize_t n = write(fd, req.data() + sent, req.size() - sent);
    if (n <= 0) { close(fd); return false; }
    sent += n;
  }

  SockReader reader(fd);
  HTTPResponse resp;
  resp.ingestFrom(reader);
  close(fd);

  if (resp.code() != 200) return false;
  body = resp.body();
  return true;
}
