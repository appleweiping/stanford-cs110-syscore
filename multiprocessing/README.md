# assign3 — All Things Multiprocessing

Three self-contained multiprocessing programs built on `fork` / `execvp` / `pipe`
/ `dup2` and (for `farm`) job-control signals. No course helper libraries — the
descriptor plumbing and signal choreography are implemented from scratch.

| Program | Shell analogue | What it exercises |
|---|---|---|
| `subprocess` | `prog` with plumbed stdin+stdout | one pipe each direction, exact fd discipline |
| `pipeline` | `argv1 \| argv2` | two children joined by an anonymous pipe |
| `farm` | self-throttling worker pool | `SIGSTOP`/`SIGCONT` job control + `SIGCHLD`/`sigsuspend` |

## subprocess

`subprocess(argv, supplyChildInput, supplyChildOutput)` forks `argv[0]`, optionally
handing back a writable pipe to the child's stdin (`supplyfd`) and/or a readable
pipe from its stdout (`ingestfd`). The parent closes every descriptor the child
owns so EOF propagates correctly. `subprocess-test` pipes a shuffled word list
through `sort` and checks the returned stream is sorted.

## pipeline

`pipeline(argv1, argv2, pids)` is the moral equivalent of `argv1 | argv2`:
argv1's stdout feeds argv2's stdin through one pipe, while argv1 inherits our
stdin and argv2 inherits our stdout. `pipeline-test` runs `sort | uniq -c`.

## farm

`farm` spawns one worker per online CPU, each running `factor.py --self-halting`.
A worker `SIGSTOP`s itself to announce "ready", the manager learns of the stop via
a `SIGCHLD` handler calling `waitpid(WUNTRACED|WNOHANG)`, then writes the next
number down the worker's pipe and `SIGCONT`s it. `sigsuspend` is used to block
race-free until a worker is available. This is the classic CS110 self-throttling
farm — work is load-balanced, so output arrives in completion order.

## Run

```bash
make            # builds subprocess-test, pipeline-test, farm
make test       # runs all three (see ../results/assign3_multiprocessing.txt)
printf '12\n360\n1048576\n1000000007\n' | ./farm
```

Verified in WSL2 (Ubuntu 24.04, g++ 13.3, 16 CPUs), `-Wall -Wextra -Werror` clean:
subprocess-test PASS, pipeline emits correct counts, farm factors every number
(e.g. `1048576 = 2^20`, `1000000007` reported prime).
