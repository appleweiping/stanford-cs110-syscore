/**
 * File: http.cc
 * -------------
 * Implementation of the HTTP message model and the buffered socket reader.
 */

#include "http.h"
#include <unistd.h>
#include <cctype>
#include <cstdlib>
#include <algorithm>
#include <sstream>

// ---------------------------------------------------------------- SockReader
bool SockReader::fill() {
  char tmp[8192];
  ssize_t n = read(fd_, tmp, sizeof(tmp));
  if (n <= 0) return false;
  buf_.append(tmp, n);
  return true;
}

bool SockReader::readLine(std::string &line) {
  while (true) {
    size_t nl = buf_.find('\n');
    if (nl != std::string::npos) {
      line = buf_.substr(0, nl);
      if (!line.empty() && line.back() == '\r') line.pop_back();
      buf_.erase(0, nl + 1);
      return true;
    }
    if (!fill()) {
      if (buf_.empty()) return false;
      line = buf_;
      buf_.clear();
      return true;
    }
  }
}

void SockReader::readExactly(size_t n, std::string &out) {
  while (buf_.size() < n) {
    if (!fill()) break;
  }
  size_t take = std::min(n, buf_.size());
  out.append(buf_, 0, take);
  buf_.erase(0, take);
}

void SockReader::readUntilEOF(std::string &out) {
  out.append(buf_);
  buf_.clear();
  while (fill()) {
    out.append(buf_);
    buf_.clear();
  }
}

// ---------------------------------------------------------------- HTTPHeader
std::string HTTPHeader::lower(const std::string &s) {
  std::string r = s;
  std::transform(r.begin(), r.end(), r.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return r;
}

void HTTPHeader::parse(SockReader &reader) {
  std::string line;
  while (reader.readLine(line)) {
    if (line.empty()) break;  // blank line terminates the header block
    size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    std::string name = line.substr(0, colon);
    std::string value = line.substr(colon + 1);
    size_t start = value.find_first_not_of(" \t");
    value = (start == std::string::npos) ? "" : value.substr(start);
    add(name, value);
  }
}

void HTTPHeader::add(const std::string &name, const std::string &value) {
  headers_.emplace_back(name, value);
}

void HTTPHeader::remove(const std::string &name) {
  std::string key = lower(name);
  headers_.erase(std::remove_if(headers_.begin(), headers_.end(),
                                [&](const std::pair<std::string, std::string> &h) {
                                  return lower(h.first) == key;
                                }),
                 headers_.end());
}

bool HTTPHeader::has(const std::string &name) const {
  std::string key = lower(name);
  for (const auto &h : headers_)
    if (lower(h.first) == key) return true;
  return false;
}

std::string HTTPHeader::get(const std::string &name) const {
  std::string key = lower(name);
  for (const auto &h : headers_)
    if (lower(h.first) == key) return h.second;
  return "";
}

std::string HTTPHeader::serialize() const {
  std::string out;
  for (const auto &h : headers_) out += h.first + ": " + h.second + "\r\n";
  return out;
}

// ---------------------------------------------------------------- HTTPRequest
// Split "http://host:port/path" (or "//host/path", or origin-form "/path").
static void parseURL(const std::string &url, RequestTarget &t) {
  std::string rest = url;
  size_t scheme = rest.find("://");
  if (scheme != std::string::npos) rest = rest.substr(scheme + 3);
  else if (rest.rfind("//", 0) == 0) rest = rest.substr(2);
  else { t.path = url.empty() ? "/" : url; return; }  // origin-form

  size_t slash = rest.find('/');
  std::string authority = (slash == std::string::npos) ? rest : rest.substr(0, slash);
  t.path = (slash == std::string::npos) ? "/" : rest.substr(slash);

  size_t colon = authority.find(':');
  if (colon == std::string::npos) {
    t.host = authority;
    t.port = 80;
  } else {
    t.host = authority.substr(0, colon);
    t.port = static_cast<unsigned short>(std::atoi(authority.c_str() + colon + 1));
  }
}

bool HTTPRequest::parse(SockReader &reader) {
  std::string line;
  if (!reader.readLine(line) || line.empty()) return false;

  std::istringstream iss(line);
  if (!(iss >> method >> url >> protocol)) return false;

  parseURL(url, target);
  headers.parse(reader);

  // If the URL was origin-form, recover host/port from the Host header.
  if (target.host.empty() && headers.has("Host")) {
    std::string host = headers.get("Host");
    size_t colon = host.find(':');
    if (colon == std::string::npos) { target.host = host; target.port = 80; }
    else { target.host = host.substr(0, colon); target.port = std::atoi(host.c_str() + colon + 1); }
  }

  if (headers.has("Content-Length")) {
    size_t len = std::strtoul(headers.get("Content-Length").c_str(), nullptr, 10);
    reader.readExactly(len, body);
  }
  return true;
}

std::string HTTPRequest::serialize() const {
  std::string out = method + " " + target.path + " " + protocol + "\r\n";
  out += headers.serialize();
  out += "\r\n";
  out += body;
  return out;
}

// ---------------------------------------------------------------- HTTPResponse
void HTTPResponse::ingestFrom(SockReader &reader) {
  std::string line;
  if (reader.readLine(line)) {
    std::istringstream iss(line);
    iss >> protocol_ >> code_;
    std::getline(iss, reason_);
    if (!reason_.empty() && reason_[0] == ' ') reason_.erase(0, 1);
  }
  headers_.parse(reader);
  if (headers_.has("Content-Length")) {
    size_t len = std::strtoul(headers_.get("Content-Length").c_str(), nullptr, 10);
    reader.readExactly(len, body_);
  } else {
    reader.readUntilEOF(body_);
  }
}

void HTTPResponse::setStatus(int code, const std::string &reason) {
  code_ = code;
  reason_ = reason;
}

void HTTPResponse::setBody(const std::string &b) {
  body_ = b;
  headers_.remove("Content-Length");
  headers_.add("Content-Length", std::to_string(b.size()));
}

std::string HTTPResponse::serialize() const {
  std::ostringstream os;
  os << protocol_ << " " << code_ << " " << reason_ << "\r\n";
  os << headers_.serialize();
  os << "\r\n";
  os << body_;
  return os.str();
}
