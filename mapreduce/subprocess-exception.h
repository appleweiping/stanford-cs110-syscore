/**
 * File: subprocess-exception.h
 * ----------------------------
 * Exception type thrown by subprocess() and friends on system-call failure.
 */

#pragma once
#include <stdexcept>
#include <string>

class SubprocessException : public std::runtime_error {
 public:
  explicit SubprocessException(const std::string &message)
      : std::runtime_error(message) {}
};
