/**
 * File: subprocess.h
 * ------------------
 * Exports a very simple subprocess abstraction: spawn a child process running
 * an arbitrary program, optionally wiring up its standard input and/or standard
 * output to pipes the parent can write to / read from.
 *
 * CS110 assign3 ("All Things Multiprocessing"), subprocess portion.
 */

#pragma once
#include <unistd.h>   // for pid_t

typedef struct subprocess_t {
  pid_t pid;          // pid of the spawned child
  int supplyfd;       // parent writes child's stdin here (-1 if not requested)
  int ingestfd;       // parent reads child's stdout here (-1 if not requested)
} subprocess_t;

/**
 * Spawns `argv[0]` (searched on PATH) with the full `argv` argument vector.
 * If `supplyChildInput` is true, the returned struct's `supplyfd` is a writable
 * pipe end feeding the child's stdin. If `supplyChildOutput` is true, `ingestfd`
 * is a readable pipe end draining the child's stdout.
 *
 * Throws a SubprocessException (C++ std::runtime_error subclass) on failure.
 */
subprocess_t subprocess(char *argv[], bool supplyChildInput, bool supplyChildOutput);
