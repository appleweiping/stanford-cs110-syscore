/**
 * File: pipeline.cc
 * -----------------
 * Implements pipeline() — connect argv1's stdout to argv2's stdin with a pipe,
 * exactly as a shell does for  argv1 | argv2.
 */

#include "pipeline.h"
#include "subprocess-exception.h"

#include <cstring>
#include <cerrno>
#include <string>

static const int kReadEnd = 0;
static const int kWriteEnd = 1;

void pipeline(char *argv1[], char *argv2[], pid_t pids[]) {
  int fds[2];
  if (pipe(fds) == -1)
    throw SubprocessException(std::string("pipe failed: ") + strerror(errno));

  // First child: writes into the pipe's write end as its stdout.
  pids[0] = fork();
  if (pids[0] == -1)
    throw SubprocessException(std::string("fork failed: ") + strerror(errno));
  if (pids[0] == 0) {
    dup2(fds[kWriteEnd], STDOUT_FILENO);
    close(fds[kReadEnd]);
    close(fds[kWriteEnd]);
    execvp(argv1[0], argv1);
    _exit(127);
  }

  // Second child: reads from the pipe's read end as its stdin.
  pids[1] = fork();
  if (pids[1] == -1)
    throw SubprocessException(std::string("fork failed: ") + strerror(errno));
  if (pids[1] == 0) {
    dup2(fds[kReadEnd], STDIN_FILENO);
    close(fds[kReadEnd]);
    close(fds[kWriteEnd]);
    execvp(argv2[0], argv2);
    _exit(127);
  }

  // Parent owns neither end.
  close(fds[kReadEnd]);
  close(fds[kWriteEnd]);
}
