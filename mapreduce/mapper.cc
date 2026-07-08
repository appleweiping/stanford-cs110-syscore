/**
 * File: mapper.cc
 * ---------------
 * Word-count mapper: read text from stdin, emit "word\t1" for every
 * alphanumeric token (lowercased). This is an ordinary standalone program the
 * MapReduce engine execs — swap it for any other mapper.
 */

#include <cctype>
#include <string>
#include <iostream>

int main() {
  std::string cur;
  auto flush = [&] {
    if (!cur.empty()) { std::cout << cur << "\t1\n"; cur.clear(); }
  };
  char c;
  while (std::cin.get(c)) {
    if (std::isalnum(static_cast<unsigned char>(c)))
      cur += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    else
      flush();
  }
  flush();
  return 0;
}
