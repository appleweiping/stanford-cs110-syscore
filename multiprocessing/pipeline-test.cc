/**
 * File: pipeline-test.cc
 * ----------------------
 * Demonstrates pipeline() by building the shell equivalent of
 *
 *     sort | uniq -c
 *
 * The two children inherit this process's stdin (feeding `sort`) and stdout
 * (drained from `uniq -c`); pipeline() wires sort's stdout to uniq's stdin.
 * The parent then reaps both children.
 */

#include "pipeline.h"

#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>

int main() {
  char sortArg[] = "sort";
  char *argv1[] = {sortArg, nullptr};

  char uniqArg[] = "uniq";
  char cArg[] = "-c";
  char *argv2[] = {uniqArg, cArg, nullptr};

  pid_t pids[2];
  pipeline(argv1, argv2, pids);

  int status1, status2;
  waitpid(pids[0], &status1, 0);
  waitpid(pids[1], &status2, 0);
  return (WIFEXITED(status1) && WIFEXITED(status2)) ? 0 : 1;
}
