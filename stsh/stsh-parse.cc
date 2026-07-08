/**
 * File: stsh-parse.cc
 * -------------------
 * Implementation of parsePipeline(). A tiny hand-written lexer breaks the line
 * into words and the operator tokens | < > &, then a single pass assembles the
 * pipeline_t, validating that every stage has a program and that redirection
 * operators are followed by a filename.
 */

#include "stsh-parse.h"
#include <cctype>

namespace {

// Lex into tokens; the four shell operators are always their own token even
// when written flush against a word (e.g. `ls>out`).
std::vector<std::string> lex(const std::string &line) {
  std::vector<std::string> tokens;
  std::string cur;
  auto flush = [&] {
    if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
  };
  for (char c : line) {
    if (isspace(static_cast<unsigned char>(c))) {
      flush();
    } else if (c == '|' || c == '<' || c == '>' || c == '&') {
      flush();
      tokens.emplace_back(1, c);
    } else {
      cur += c;
    }
  }
  flush();
  return tokens;
}

bool isOperator(const std::string &t) {
  return t == "|" || t == "<" || t == ">" || t == "&";
}

}  // namespace

pipeline_t parsePipeline(const std::string &line) {
  pipeline_t p;
  std::vector<std::string> tokens = lex(line);
  std::vector<std::string> current;  // argv of the stage being built

  auto fail = [&](const std::string &msg) {
    p.valid = false;
    p.error = msg;
  };

  for (size_t i = 0; i < tokens.size() && p.valid; i++) {
    const std::string &t = tokens[i];
    if (t == "|") {
      if (current.empty()) { fail("missing command before '|'"); break; }
      p.commands.push_back(current);
      current.clear();
    } else if (t == "<") {
      if (i + 1 >= tokens.size() || isOperator(tokens[i + 1])) {
        fail("expected filename after '<'"); break;
      }
      p.input = tokens[++i];
    } else if (t == ">") {
      if (i + 1 >= tokens.size() || isOperator(tokens[i + 1])) {
        fail("expected filename after '>'"); break;
      }
      p.output = tokens[++i];
    } else if (t == "&") {
      if (i + 1 != tokens.size()) { fail("'&' must be the last token"); break; }
      p.background = true;
    } else {
      current.push_back(t);
    }
  }

  if (p.valid) {
    if (!current.empty()) p.commands.push_back(current);
    if (p.commands.empty() && (!p.input.empty() || !p.output.empty()))
      fail("redirection without a command");
  }
  return p;
}
