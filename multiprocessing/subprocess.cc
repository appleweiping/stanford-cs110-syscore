/**
 * File: subprocess.cc
 * -------------------
 * Implements the subprocess abstraction declared in subprocess.h.
 *
 * The heavy lifting is careful pipe / dup2 plumbing plus disciplined closing of
 * every unused descriptor so no process holds a pipe end open longer than it
 * should (which would otherwise wedge EOF detection).
 */

#include "subprocess.h"
#include "subprocess-exception.h"

#include <cstring>
#include <cerrno>
#include <sys/wait.h>

// Symbolic names for the two ends of a pipe.
static const int kReadEnd = 0;
static const int kWriteEnd = 1;

static void closeIfOpen(int fd) {
  if (fd != -1) close(fd);
}

subprocess_t subprocess(char *argv[], bool supplyChildInput, bool supplyChildOutput) {
  // supply == parent -> child stdin;  ingest == child stdout -> parent.
  int supplyPipe[2] = {-1, -1};
  int ingestPipe[2] = {-1, -1};

  if (supplyChildInput && pipe(supplyPipe) == -1)
    throw SubprocessException(std::string("pipe(supply) failed: ") + strerror(errno));
  if (supplyChildOutput && pipe(ingestPipe) == -1)
    throw SubprocessException(std::string("pipe(ingest) failed: ") + strerror(errno));

  pid_t pid = fork();
  if (pid == -1)
    throw SubprocessException(std::string("fork failed: ") + strerror(errno));

  if (pid == 0) {
    // ---- child ----
    if (supplyChildInput) {
      // Child reads its stdin from the supply pipe's read end.
      dup2(supplyPipe[kReadEnd], STDIN_FILENO);
      close(supplyPipe[kReadEnd]);
      close(supplyPipe[kWriteEnd]);  // child never writes the supply pipe
    }
    if (supplyChildOutput) {
      // Child writes its stdout to the ingest pipe's write end.
      dup2(ingestPipe[kWriteEnd], STDOUT_FILENO);
      close(ingestPipe[kReadEnd]);   // child never reads the ingest pipe
      close(ingestPipe[kWriteEnd]);
    }
    execvp(argv[0], argv);
    // Only reached if exec failed.
    std::string msg = std::string("execvp('") + argv[0] + "') failed: " + strerror(errno);
    // Can't throw across the fork sensibly; report and exit non-zero.
    write(STDERR_FILENO, msg.c_str(), msg.size());
    _exit(127);
  }

  // ---- parent ----
  // Parent keeps the *opposite* ends and closes the ones the child owns.
  closeIfOpen(supplyPipe[kReadEnd]);   // child's stdin read end
  closeIfOpen(ingestPipe[kWriteEnd]);  // child's stdout write end

  subprocess_t process;
  process.pid = pid;
  process.supplyfd = supplyChildInput ? supplyPipe[kWriteEnd] : -1;
  process.ingestfd = supplyChildOutput ? ingestPipe[kReadEnd] : -1;
  return process;
}
