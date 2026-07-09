# Stanford CS110 — Principles of Computer Systems

> From-scratch C/C++ implementations of the CS110 systems-programming
> assignments — filesystems, multiprocessing, a shell with job control,
> concurrency, and networking — an independent, from-scratch build of
> **CS110 — Principles of Computer Systems** (Stanford), part of a
> [csdiy.wiki](https://csdiy.wiki/) full-catalog build.

![status](https://img.shields.io/badge/status-complete-brightgreen)
![language](https://img.shields.io/badge/C++17-informational)
![license](https://img.shields.io/badge/license-MIT-blue)

## Overview

CS110 teaches how real systems are built out of the Unix system-call layer:
filesystems, processes and pipes, signals and job control, threads and
synchronization, and networking. This repo implements **all 8 assignments** in
idiomatic C++17 on raw POSIX APIs — no course helper libraries; where the course
ships a private library (threadpool, socket++, HTTP classes) or a proprietary
dataset, a minimal working equivalent is implemented from scratch and a builder
generates real spec-conformant data. Everything compiles `-Wall -Wextra -Werror`
clean and **runs end-to-end** in WSL2, each with an automated test that captures
real output into [`results/`](results/).

## Results (measured on WSL2, Ubuntu 24.04, g++ 13.3, 16-core CPU, libxml2 2.9.14)

| # | Assignment | What it does | Result (measured) |
|---|---|---|---|
| 1 | **Amazon Reviews Search** | mmap'd binary DB + keyword index, pointer arithmetic, STL binary search | **10/10** tests, **valgrind-clean**; phrase `"works great"`→3 vs AND `works great`→4 |
| 2 | **Unix v6 Filesystem** | layered read-only V6 fs: inode → file → directory → pathname | **20/20** assertions; `ILARG` indirect addressing verified on a 20-block file |
| 3 | **Multiprocessing** | `subprocess`, `pipeline`, self-throttling `farm` | subprocess PASS, `sort\|uniq -c` correct, farm across **16 CPUs** (`1048576 = 2^20`) |
| 4 | **stsh (Stanford Shell)** | pipelines, redirection, job control, signals | **9/9** functional + **6/6** pty job-control (Ctrl-Z/Ctrl-C forwarding) |
| 5 | **RSS News Aggregator** | multithreaded fetch + libxml2 parse + inverted index | **7/7**; de-dup proven by access log (shared article fetched **once**) |
| 6 | **ThreadPool** | dispatcher + workers, hand-rolled semaphore | **5/5**; 16×100 ms tasks on 8 workers in **202 ms** (vs 1600 ms serial); **TSan-clean** |
| 7 | **HTTP Web Proxy + Cache** | concurrent forward proxy, cache, blocklist | **9/9**; cache causal (origin hit **once**), **25-way** concurrency, blocklist → 403 |
| 8 | **MapReduce** *(optional)* | single-node map → hash-shuffle → reduce | **7/7**; counts exact, fixed a real concurrent-`fork` fd leak (5/5 stable) |

## Implemented assignments

- [x] **assign1 — Amazon Reviews Search** — keyword search over mmap'd binary review DB + sorted keyword index (`lower_bound`, phrases, `set_intersection`)
- [x] **assign2 — Reading Unix v6 Filesystems** — bottom-up V6 reader with a self-contained `mkfs_v6` image builder
- [x] **assign3 — All Things Multiprocessing** — `subprocess`/`pipeline`/`farm` on fork/exec/pipe + `SIGSTOP`/`SIGCONT` throttling
- [x] **assign4 — stsh, the Stanford Shell** — arbitrary pipelines, `<`/`>`, `&`, `jobs`/`fg`/`bg`/`slay`/`halt`/`cont`, terminal + signal job control
- [x] **assign5 — RSS News Feed Aggregator** — two-stage ThreadPool crawler, libxml2 RSS/HTML parsing, inverted index, per-server throttle, de-dup
- [x] **assign6 — ThreadPool** — reusable dispatcher+worker pool, hand-rolled counting semaphore, ThreadSanitizer-clean
- [x] **assign7 — HTTP Web Proxy and Cache** — concurrent forward proxy on raw sockets, in-memory cache (Cache-Control), regex blocklist, `x-forwarded-for`
- [x] **assign8 — MapReduce** *(optional/ungraded)* — real map/shuffle/reduce mechanics with exec'd mapper/reducer; cluster layer replaced by local processes

## Project structure

```
stanford-cs110-syscore/
├── amazon-reviews/      # assign1: mmap binary DB + keyword search + mkamazondb builder
├── unix-v6-filesystem/  # assign2: V6 fs reader (inode/file/directory/pathname) + mkfs_v6
├── multiprocessing/     # assign3: subprocess, pipeline, farm (+ factor.py worker)
├── stsh/                # assign4: shell; tests/ has functional + pty job-control tests
├── news-aggregator/     # assign5: RSS aggregator (fetch, xml-utils, aggregate)
├── thread-pool/         # assign6: ThreadPool + semaphore + tptest
├── http-proxy/          # assign7: proxy, http model, cache, blocklist + local-origin tests
├── mapreduce/           # assign8: mr engine + mapper/reducer
├── results/             # captured real run output for every assignment
├── LICENSE              # MIT (covers this repo's own code)
└── README.md
```

## How to run

Everything is Linux-only (POSIX). Build and run inside **WSL2 Ubuntu**:

```bash
sudo apt-get install -y build-essential libxml2-dev valgrind   # libxml2 only needed by assign5
cd stanford-cs110-syscore

# each assignment builds and self-tests independently:
for d in amazon-reviews unix-v6-filesystem multiprocessing stsh \
         news-aggregator thread-pool http-proxy mapreduce; do
  ( cd "$d" && make test )
done
```

Each directory has its own `README.md` with design notes and its exact test
command.

## Verification

Every assignment ships an automated test whose real output is captured under
[`results/`](results/):

- `results/assign1_amazon.txt` … `results/assign8_mapreduce.txt`.
- Tests are self-checking (assert exact values) and cover the substantive
  behavior: V6 indirect-block addressing, farm load-balancing across all CPUs,
  pty-driven Ctrl-Z/Ctrl-C job control, causal proof of proxy caching and
  aggregator de-duplication via origin access logs, ThreadSanitizer- and
  valgrind-clean runs.

## Tech stack

C++17 · POSIX system calls (`fork`/`execvp`/`pipe`/`dup2`/`mmap`/`setpgid`/
`tcsetpgrp`/`waitpid`/`sigsuspend`, BSD sockets) · `std::thread`/`mutex`/
`condition_variable` · libxml2 (RSS/HTML) · GNU Make · GCC 13.3 · WSL2 Ubuntu ·
tested with ThreadSanitizer and Valgrind.

## Key ideas / what I learned

- **Descriptor discipline** is everything in multiprocessing: a single leaked
  pipe write-end wedges EOF detection (assign3, and the `O_CLOEXEC` fix in
  assign8's concurrent forks).
- **Job control** is process groups + `tcsetpgrp` + forwarding terminal signals
  to the foreground group, with `sigsuspend` to wait race-free (assign4).
- **Concurrency correctness**: coordinating a pool with counting semaphores and
  proving it race-free under TSan (assign6), and throttling per-server
  connections while sharing an index safely (assign5).
- **Binary formats & mmap**: reading offset-indexed records with pure pointer
  arithmetic, and binary-searching a sorted keyword table (assign1, assign2).
- **Networking**: a forward proxy is HTTP parsing + socket plumbing + a cache
  keyed on the request, made causal-testable against a local origin (assign7).

## Credits & license

Based on the assignments of **CS110 — Principles of Computer Systems** by Chris
Gregg, Jerry Cain, and Nick Troccoli (Stanford). This repository is an
independent educational reimplementation; all course materials and
specifications belong to their original authors. Original code in this repo is
released under the [MIT License](LICENSE).
