/**
 * File: stsh-parse.h
 * ------------------
 * A small command-line parser for stsh. It splits a line into a pipeline of
 * commands (each an argv vector), pulling out `< infile` / `> outfile`
 * redirection and a trailing `&` for background execution. This stands in for
 * the course-supplied stsh-parser.
 */

#pragma once
#include <string>
#include <vector>

struct pipeline_t {
  std::vector<std::vector<std::string>> commands;  // one argv per stage
  std::string input;                               // "" if no `<`
  std::string output;                              // "" if no `>`
  bool background = false;
  bool valid = true;
  std::string error;                               // set when valid == false
};

/** Parse one shell line. On syntax error returns a pipeline with valid==false. */
pipeline_t parsePipeline(const std::string &line);
