/**
 * File: fetch.h
 * -------------
 * A one-shot HTTP/1.0 GET client used by the news aggregator to download feed
 * indexes, RSS documents, and article HTML. Reuses the proxy's HTTP model.
 */

#pragma once
#include <string>

struct URL {
  std::string host;
  unsigned short port = 80;
  std::string path = "/";
  std::string server() const { return host + ":" + std::to_string(port); }
};

URL parseHttpURL(const std::string &url);

/**
 * GETs `url` and, on a 200 response, fills `body` and returns true. Returns
 * false on connection error, non-200 status, or a non-http scheme.
 */
bool fetchURL(const std::string &url, std::string &body);
