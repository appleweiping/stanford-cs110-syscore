/**
 * File: stsh.cc
 * -------------
 * stsh — the Stanford Shell (CS110 assign4). Supports:
 *   - pipelines of arbitrary length:  a | b | c
 *   - input/output redirection:       sort < in > out
 *   - background execution:           sleep 5 &
 *   - job control built-ins:          jobs, fg, bg, slay, halt, cont, quit/exit
 *   - full signal handling:           SIGCHLD reaping, and Ctrl-C / Ctrl-Z
 *                                     forwarded to the foreground process group
 *
 * Each pipeline runs in its own process group. When a job runs in the
 * foreground the shell hands it the controlling terminal (tcsetpgrp) and blocks
 * (sigsuspend) until every process in the group has terminated or stopped.
 *
 * The shell also runs correctly with a non-tty stdin (a piped script), in which
 * case the terminal-control steps are skipped but process-group job control and
 * foreground waiting still work — this is what the automated tests drive.
 */

#include "stsh-parse.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <sstream>

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <sys/wait.h>
#include <sys/types.h>

using namespace std;

// ---------------------------------------------------------------------------
// Job model
// ---------------------------------------------------------------------------
enum class ProcState { kRunning, kStopped, kTerminated };
enum class JobState  { kForeground, kBackground };

struct Process {
  pid_t pid;
  ProcState state = ProcState::kRunning;
};

struct Job {
  size_t num = 0;
  pid_t pgid = 0;
  JobState state = JobState::kBackground;
  string cmdline;
  vector<Process> processes;

  bool allTerminated() const {
    for (const Process &p : processes)
      if (p.state != ProcState::kTerminated) return false;
    return true;
  }
  bool anyRunning() const {
    for (const Process &p : processes)
      if (p.state == ProcState::kRunning) return true;
    return false;
  }
  bool allStopped() const {
    for (const Process &p : processes)
      if (p.state == ProcState::kRunning) return false;  // running => not all stopped
    return !allTerminated();
  }
};

class JobList {
 public:
  Job &addJob(pid_t pgid, const string &cmdline, JobState state) {
    size_t num = ++counter_;
    Job job;
    job.num = num;
    job.pgid = pgid;
    job.cmdline = cmdline;
    job.state = state;
    return jobs_.emplace(num, std::move(job)).first->second;
  }
  Job *getForegroundJob() {
    for (auto &kv : jobs_)
      if (kv.second.state == JobState::kForeground) return &kv.second;
    return nullptr;
  }
  Job *getJob(size_t num) {
    auto it = jobs_.find(num);
    return it == jobs_.end() ? nullptr : &it->second;
  }
  Job *getJobByPgid(pid_t pgid) {
    for (auto &kv : jobs_)
      if (kv.second.pgid == pgid) return &kv.second;
    return nullptr;
  }
  Job *getJobWithProcess(pid_t pid) {
    for (auto &kv : jobs_)
      for (const Process &p : kv.second.processes)
        if (p.pid == pid) return &kv.second;
    return nullptr;
  }
  void remove(size_t num) { jobs_.erase(num); }
  bool empty() const { return jobs_.empty(); }

  void print(ostream &os) const {
    for (const auto &kv : jobs_) {
      const Job &j = kv.second;
      const char *st = j.allStopped() ? "Stopped" : "Running";
      os << "[" << j.num << "] " << st << "    " << j.cmdline << "\n";
    }
  }
  // reset the numbering when the table empties (keeps job ids small)
  void maybeResetCounter() { if (jobs_.empty()) counter_ = 0; }

 private:
  map<size_t, Job> jobs_;
  size_t counter_ = 0;
};

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static JobList joblist;
static volatile sig_atomic_t fgPgid = 0;   // pgid of the current foreground job
static bool interactive = false;           // stdin is a tty

// ---------------------------------------------------------------------------
// Signal plumbing
// ---------------------------------------------------------------------------
static void installHandler(int sig, void (*handler)(int)) {
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = handler;
  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask, SIGCHLD);
  sigaddset(&act.sa_mask, SIGINT);
  sigaddset(&act.sa_mask, SIGTSTP);
  act.sa_flags = SA_RESTART;
  sigaction(sig, &act, nullptr);
}

static void blockSIGCHLD(sigset_t *saved) {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigprocmask(SIG_BLOCK, &mask, saved);
}
static void restoreMask(const sigset_t *saved) {
  sigprocmask(SIG_SETMASK, saved, nullptr);
}

static void handleSIGCHLD(int sig) {
  (void)sig;
  int status;
  pid_t pid;
  while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
    Job *job = joblist.getJobWithProcess(pid);
    if (job == nullptr) continue;
    for (Process &p : job->processes) {
      if (p.pid != pid) continue;
      if (WIFEXITED(status) || WIFSIGNALED(status)) p.state = ProcState::kTerminated;
      else if (WIFSTOPPED(status)) p.state = ProcState::kStopped;
      else if (WIFCONTINUED(status)) p.state = ProcState::kRunning;
    }
    if (job->allTerminated()) joblist.remove(job->num);
  }
}

// Ctrl-C / Ctrl-Z: forward to the foreground process group (never to the shell).
static void forwardToForeground(int sig) {
  if (fgPgid > 0) kill(-fgPgid, sig);
}

// ---------------------------------------------------------------------------
// Foreground waiting
// ---------------------------------------------------------------------------
static void giveTerminalTo(pid_t pgid) {
  if (interactive) tcsetpgrp(STDIN_FILENO, pgid);
}

static void waitForForegroundJob(pid_t pgid) {
  sigset_t saved, empty;
  sigemptyset(&empty);
  blockSIGCHLD(&saved);
  Job *job;
  while ((job = joblist.getJobByPgid(pgid)) != nullptr && job->anyRunning())
    sigsuspend(&empty);

  // Job either finished (removed) or stopped. If stopped, demote to background.
  job = joblist.getJobByPgid(pgid);
  if (job != nullptr && job->allStopped()) {
    job->state = JobState::kBackground;
    cout << "[" << job->num << "] Stopped    " << job->cmdline << "\n";
  }
  fgPgid = 0;
  restoreMask(&saved);
  giveTerminalTo(getpgrp());
  joblist.maybeResetCounter();
}

// ---------------------------------------------------------------------------
// Built-ins
// ---------------------------------------------------------------------------
// Resolve a slay/halt/cont argument: "%N" => whole job pgid (negated), else pid.
static bool resolveTarget(const string &arg, pid_t &target, bool &wholeGroup) {
  if (arg.empty()) return false;
  if (arg[0] == '%') {
    size_t num = strtoul(arg.c_str() + 1, nullptr, 10);
    Job *job = joblist.getJob(num);
    if (job == nullptr) return false;
    target = job->pgid;
    wholeGroup = true;
    return true;
  }
  target = static_cast<pid_t>(strtol(arg.c_str(), nullptr, 10));
  wholeGroup = false;
  return target > 0;
}

static void markGroupRunning(Job *job) {
  for (Process &p : job->processes)
    if (p.state == ProcState::kStopped) p.state = ProcState::kRunning;
}

// returns true if the token was a built-in (and it was handled).
static bool runBuiltin(const vector<string> &argv) {
  const string &cmd = argv[0];
  if (cmd == "quit" || cmd == "exit") {
    exit(0);
  }
  if (cmd == "jobs") {
    sigset_t saved; blockSIGCHLD(&saved);
    joblist.print(cout);
    restoreMask(&saved);
    return true;
  }
  if (cmd == "fg" || cmd == "bg") {
    if (argv.size() != 2) { cerr << "Usage: " << cmd << " <jobid>\n"; return true; }
    sigset_t saved; blockSIGCHLD(&saved);
    Job *job = joblist.getJob(strtoul(argv[1].c_str(), nullptr, 10));
    if (job == nullptr) { cerr << cmd << ": no such job " << argv[1] << "\n"; restoreMask(&saved); return true; }
    markGroupRunning(job);
    kill(-job->pgid, SIGCONT);
    if (cmd == "bg") {
      job->state = JobState::kBackground;
      restoreMask(&saved);
    } else {
      job->state = JobState::kForeground;
      fgPgid = job->pgid;
      pid_t pgid = job->pgid;
      restoreMask(&saved);
      giveTerminalTo(pgid);
      waitForForegroundJob(pgid);
    }
    return true;
  }
  if (cmd == "slay" || cmd == "halt" || cmd == "cont") {
    if (argv.size() != 2) { cerr << "Usage: " << cmd << " <pid | %jobid>\n"; return true; }
    int sig = (cmd == "slay") ? SIGKILL : (cmd == "halt") ? SIGTSTP : SIGCONT;
    sigset_t saved; blockSIGCHLD(&saved);
    pid_t target; bool wholeGroup;
    if (!resolveTarget(argv[1], target, wholeGroup)) {
      cerr << cmd << ": no such process/job " << argv[1] << "\n";
    } else {
      kill(wholeGroup ? -target : target, sig);
    }
    restoreMask(&saved);
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Pipeline execution
// ---------------------------------------------------------------------------
static void execChild(const vector<string> &argv) {
  vector<char *> cargv;
  for (const string &s : argv) cargv.push_back(const_cast<char *>(s.c_str()));
  cargv.push_back(nullptr);
  execvp(cargv[0], cargv.data());
  // exec failed
  fprintf(stderr, "%s: %s\n", cargv[0], strerror(errno));
  _exit(127);
}

static void runPipeline(const pipeline_t &p, const string &cmdline) {
  const size_t n = p.commands.size();

  sigset_t saved;
  blockSIGCHLD(&saved);  // build the job atomically w.r.t. SIGCHLD

  pid_t pgid = 0;
  vector<pid_t> pids;
  int prevRead = -1;  // read end of the pipe feeding the current stage

  for (size_t i = 0; i < n; i++) {
    bool hasNext = (i + 1 < n);
    int pfd[2] = {-1, -1};
    if (hasNext && pipe(pfd) == -1) { perror("pipe"); restoreMask(&saved); return; }

    pid_t pid = fork();
    if (pid == -1) { perror("fork"); restoreMask(&saved); return; }

    if (pid == 0) {
      // ---- child ----
      signal(SIGINT, SIG_DFL);
      signal(SIGTSTP, SIG_DFL);
      signal(SIGCHLD, SIG_DFL);
      signal(SIGTTOU, SIG_DFL);
      signal(SIGTTIN, SIG_DFL);
      sigset_t none; sigemptyset(&none);
      sigprocmask(SIG_SETMASK, &none, nullptr);

      pid_t desired = (pgid == 0) ? getpid() : pgid;
      setpgid(0, desired);

      if (prevRead != -1) { dup2(prevRead, STDIN_FILENO); close(prevRead); }
      if (hasNext) { dup2(pfd[1], STDOUT_FILENO); close(pfd[0]); close(pfd[1]); }

      if (i == 0 && !p.input.empty()) {
        int fd = open(p.input.c_str(), O_RDONLY);
        if (fd == -1) { fprintf(stderr, "%s: %s\n", p.input.c_str(), strerror(errno)); _exit(1); }
        dup2(fd, STDIN_FILENO); close(fd);
      }
      if (i == n - 1 && !p.output.empty()) {
        int fd = open(p.output.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) { fprintf(stderr, "%s: %s\n", p.output.c_str(), strerror(errno)); _exit(1); }
        dup2(fd, STDOUT_FILENO); close(fd);
      }
      execChild(p.commands[i]);
    }

    // ---- parent ----
    if (pgid == 0) pgid = pid;
    setpgid(pid, pgid);  // set in both parent and child to avoid a race
    pids.push_back(pid);

    if (prevRead != -1) close(prevRead);
    if (hasNext) { close(pfd[1]); prevRead = pfd[0]; }
  }
  if (prevRead != -1) close(prevRead);

  Job &job = joblist.addJob(pgid, cmdline, p.background ? JobState::kBackground : JobState::kForeground);
  for (pid_t pid : pids) job.processes.push_back({pid, ProcState::kRunning});

  if (p.background) {
    ostringstream os;
    os << "[" << job.num << "]";
    for (pid_t pid : pids) os << " " << pid;
    cout << os.str() << "\n";
    restoreMask(&saved);
  } else {
    fgPgid = pgid;
    giveTerminalTo(pgid);
    restoreMask(&saved);
    waitForForegroundJob(pgid);
  }
}

// ---------------------------------------------------------------------------
// REPL
// ---------------------------------------------------------------------------
int main() {
  interactive = isatty(STDIN_FILENO);

  // Put the shell in its own process group and take the terminal.
  setpgid(getpid(), getpid());
  if (interactive) tcsetpgrp(STDIN_FILENO, getpgrp());

  signal(SIGTTOU, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  installHandler(SIGCHLD, handleSIGCHLD);
  installHandler(SIGINT, forwardToForeground);
  installHandler(SIGTSTP, forwardToForeground);

  string line;
  while (true) {
    if (interactive) { cout << "stsh> " << flush; }
    if (!getline(cin, line)) break;  // EOF (Ctrl-D)

    pipeline_t p = parsePipeline(line);
    if (!p.valid) { cerr << "stsh: parse error: " << p.error << "\n"; continue; }
    if (p.commands.empty()) continue;

    if (p.commands.size() == 1 && runBuiltin(p.commands[0])) continue;

    runPipeline(p, line);
  }
  if (interactive) cout << "\n";
  return 0;
}
