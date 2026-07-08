/**
 * File: reducer.cc
 * ----------------
 * Word-count reducer: read "word\tcount" lines that arrive grouped by word
 * (the engine sorts each partition before invoking the reducer), sum the counts
 * per word, and emit "word\t<total>".
 */

#include <string>
#include <sstream>
#include <iostream>

int main() {
  std::string line, currentKey;
  long total = 0;
  bool have = false;

  while (std::getline(std::cin, line)) {
    if (line.empty()) continue;
    size_t tab = line.find('\t');
    std::string key = (tab == std::string::npos) ? line : line.substr(0, tab);
    long value = (tab == std::string::npos) ? 1 : std::stol(line.substr(tab + 1));

    if (have && key != currentKey) {
      std::cout << currentKey << "\t" << total << "\n";
      total = 0;
    }
    currentKey = key;
    total += value;
    have = true;
  }
  if (have) std::cout << currentKey << "\t" << total << "\n";
  return 0;
}
