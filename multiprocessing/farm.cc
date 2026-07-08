/**
 * File: farm.cc
 * -------------
 * "farm" spins up a pool of self-halting worker subprocesses (one per online
 * CPU) and streams the numbers arriving on its own stdin to whichever worker is
 * idle, load-balancing the work across cores. Each worker runs ./factor.py in
 * "self-halting" mode: it reads a number, raises SIGSTOP on itself to signal it
 * is ready for the *next* number, and on resume factors the number it was given.
 *
 * The manager uses the SIGCHLD handler + waitpid(WUNTRACED|WCONTINUED) to learn
 * when a worker has stopped (=> available) so it can hand it the next number and
 * SIGCONT it. This is the classic CS110 assign3 "farm" design.
 */

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cassert>
#include <string>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

struct worker {
  worker() : available(false) {}
  worker(char *argv[]) : available(false) {
    if (pipe(supplyfds) == -1) { perror("pipe"); exit(1); }
    pid = fork();
    if (pid == -1) { perror("fork"); exit(1); }
    if (pid == 0) {
      dup2(supplyfds[0], STDIN_FILENO);
      close(supplyfds[0]);
      close(supplyfds[1]);
      execvp(argv[0], argv);
      perror("execvp");
      _exit(127);
    }
    close(supplyfds[0]);  // parent only writes
  }
  pid_t pid;
  int supplyfds[2];   // supplyfds[1] is where the manager writes numbers
  bool available;
};

static const size_t kNumCPUs = sysconf(_SC_NPROCESSORS_ONLN);
static std::vector<worker> workers(kNumCPUs);
static size_t numWorkersAvailable = 0;

// SIGCHLD handler: reaps state transitions. A stopped child => it finished the
// previous number (or just started) and is ready for the next one.
static void markWorkersAsAvailable(int sig) {
  (void) sig;
  while (true) {
    int status;
    pid_t pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
    if (pid <= 0) break;
    if (WIFSTOPPED(status)) {
      for (worker &w : workers) {
        if (w.pid == pid && !w.available) {
          w.available = true;
          numWorkersAvailable++;
          break;
        }
      }
    }
  }
}

static const char *kWorkerArguments[] = {"./factor.py", "--self-halting", NULL};

static void spawnAllWorkers() {
  std::cout << "There are this many CPUs: " << kNumCPUs
            << ", numbered 0 through " << kNumCPUs - 1 << "." << std::endl;
  for (size_t i = 0; i < workers.size(); i++) {
    workers[i] = worker(const_cast<char **>(kWorkerArguments));
    std::cout << "Worker " << workers[i].pid << " is set to run on CPU " << i << "." << std::endl;
  }
}

// Blocks (via sigsuspend) until at least one worker is available, then returns
// the index of an available worker and marks it busy.
static size_t getAvailableWorker() {
  sigset_t existing;
  sigset_t block;
  sigemptyset(&block);
  sigaddset(&block, SIGCHLD);
  sigprocmask(SIG_BLOCK, &block, &existing);
  while (numWorkersAvailable == 0) sigsuspend(&existing);  // atomically unblocks while waiting
  sigprocmask(SIG_UNBLOCK, &block, NULL);

  assert(numWorkersAvailable > 0);
  size_t i;
  for (i = 0; i < workers.size(); i++) if (workers[i].available) break;
  assert(i < workers.size());
  return i;
}

static void broadcastNumbersToWorkers() {
  std::string line;
  while (true) {
    getline(std::cin, line);
    if (std::cin.fail()) break;
    size_t endpos;
    long long num = std::stoll(line, &endpos);
    if (endpos != line.size()) break;  // reject trailing garbage
    size_t i = getAvailableWorker();
    workers[i].available = false;
    numWorkersAvailable--;
    // Hand the worker its number, then resume it so it factors and stops again.
    std::string payload = std::to_string(num) + "\n";
    if (write(workers[i].supplyfds[1], payload.c_str(), payload.size()) == -1)
      perror("write");
    kill(workers[i].pid, SIGCONT);
  }
}

static void waitForAllWorkers() {
  // Ensure every worker is idle (stopped) before we close its pipe.
  sigset_t block, existing;
  sigemptyset(&block);
  sigaddset(&block, SIGCHLD);
  sigprocmask(SIG_BLOCK, &block, &existing);
  while (numWorkersAvailable < workers.size()) sigsuspend(&existing);
  sigprocmask(SIG_UNBLOCK, &block, NULL);
}

static void closeAllWorkers() {
  // Wake each worker one last time with EOF so it exits cleanly.
  signal(SIGCHLD, SIG_DFL);
  for (worker &w : workers) {
    close(w.supplyfds[1]);
    kill(w.pid, SIGCONT);
  }
  for (worker &w : workers) waitpid(w.pid, NULL, 0);
}

int main() {
  signal(SIGCHLD, markWorkersAsAvailable);
  spawnAllWorkers();
  broadcastNumbersToWorkers();
  waitForAllWorkers();
  closeAllWorkers();
  return 0;
}
