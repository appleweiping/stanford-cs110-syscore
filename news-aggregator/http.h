/**
 * File: http.h
 * ------------
 * Minimal HTTP/1.x message model for the proxy: a case-insensitive header map,
 * a parsed request, a parsed response, and a small buffered socket reader. This
 * replaces the course's HTTPRequest/HTTPResponse/HTTPHeader + socket++ helpers.
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include <utility>

// Buffered reader over a socket fd: line reads for headers, byte/EOF reads for
// bodies. Keeps a leftover buffer so a header read never over-consumes the body.
class SockReader {
 public:
  explicit SockReader(int fd) : fd_(fd) {}
  bool readLine(std::string &line);            // strips trailing CRLF; false at EOF
  void readExactly(size_t n, std::string &out);// appends up to n bytes (fewer at EOF)
  void readUntilEOF(std::string &out);         // appends the rest of the stream

 private:
  bool fill();
  int fd_;
  std::string buf_;
};

// Case-insensitive, order-preserving header collection.
class HTTPHeader {
 public:
  void parse(SockReader &reader);              // reads header lines until the blank line
  void add(const std::string &name, const std::string &value);
  void remove(const std::string &name);
  bool has(const std::string &name) const;
  std::string get(const std::string &name) const;     // "" if absent
  std::string serialize() const;               // "Name: Value\r\n" ... (no blank line)

 private:
  std::vector<std::pair<std::string, std::string>> headers_;
  static std::string lower(const std::string &s);
};

struct RequestTarget {
  std::string host;
  unsigned short port = 80;
  std::string path = "/";
};

class HTTPRequest {
 public:
  bool parse(SockReader &reader);              // false on malformed/closed
  std::string serialize() const;               // request line + headers + blank + body

  std::string method;
  std::string url;                             // as received (absolute- or origin-form)
  std::string protocol = "HTTP/1.0";
  RequestTarget target;
  HTTPHeader headers;
  std::string body;
};

class HTTPResponse {
 public:
  void ingestFrom(SockReader &reader);         // parse status line + headers + body
  std::string serialize() const;

  void setProtocol(const std::string &p) { protocol_ = p; }
  void setStatus(int code, const std::string &reason);
  void setBody(const std::string &b);
  HTTPHeader &headers() { return headers_; }
  const HTTPHeader &headers() const { return headers_; }
  int code() const { return code_; }
  const std::string &body() const { return body_; }

 private:
  std::string protocol_ = "HTTP/1.0";
  int code_ = 0;
  std::string reason_;
  HTTPHeader headers_;
  std::string body_;
};
