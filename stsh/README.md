# assign4 — stsh, the Stanford Shell

A working Unix shell with pipelines, redirection, and full POSIX job control,
built on `fork` / `execvp` / `pipe` / `dup2` / `setpgid` / `tcsetpgrp` /
`waitpid` / `sigsuspend`. No course helper libraries — the parser and the job
table are implemented from scratch (`stsh-parse.*`, `stsh.cc`).

## Features

- **Pipelines** of arbitrary length: `sort < data | uniq -c | wc -l`
- **Redirection**: `< infile`, `> outfile`
- **Background jobs**: `sleep 5 &`
- **Job control built-ins**: `jobs`, `fg`, `bg`, `slay`, `halt`, `cont`, `quit`/`exit`
  - `slay`/`halt`/`cont` take a `<pid>` or `%<jobid>` (whole process group)
- **Signals**: `SIGCHLD` reaping with job-state tracking; **Ctrl-C** and **Ctrl-Z**
  are forwarded to the *foreground* process group (never the shell itself).

Each pipeline runs in its own process group. A foreground job is handed the
controlling terminal via `tcsetpgrp`; the shell then blocks in `sigsuspend`
until every process in the group has terminated or stopped. When stdin is not a
tty (a piped script), terminal handoff is skipped but process-group job control
and foreground waiting still work.

## Run

```bash
make            # builds ./stsh
./stsh          # interactive
make test       # deterministic functional tests (tests/run.sh)
python3 tests/jobcontrol_pty.py   # interactive job control over a pseudo-tty
```

## Verification (WSL2, Ubuntu 24.04, g++ 13.3), `-Wall -Wextra -Werror` clean

Captured in `../results/assign4_stsh.txt`.

**Functional (9/9)** — pipeline `echo|tr`, 3-stage `sort<data|uniq -c|wc -l`,
input/output redirection round-trip, `echo|rev`, background job shown in `jobs`,
`fg` reaps a finished job, parse-error rejection, exec-failure reporting.

**Interactive job control over a real pseudo-terminal (6/6)**:

| Check | Result |
|---|---|
| Ctrl-Z stops the foreground job, reported `Stopped` | ✓ |
| `jobs` lists the stopped job | ✓ |
| `bg 1` resumes it (SIGCONT) → `Running` | ✓ |
| `slay %1` SIGKILLs the whole group → job gone | ✓ |
| Ctrl-C at an idle prompt does **not** kill the shell | ✓ |
| Ctrl-C kills the foreground job and the prompt returns | ✓ |

## Notes / scope

The parser mirrors the course's simple tokenizer: it splits on whitespace and
the operators `| < > &` and does **not** implement shell quoting or globbing
(neither does the reference stsh-parser). Commands are looked up on `PATH` via
`execvp`.
