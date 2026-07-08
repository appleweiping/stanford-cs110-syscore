# assign6 — ThreadPool

A reusable thread pool with the CS110 interface, built from scratch on
`std::thread`, `std::mutex`, `std::condition_variable`, and a hand-rolled
counting `semaphore` (the course's `semaphore` helper is not used).

```cpp
ThreadPool pool(8);
pool.schedule([] { /* work */ });   // enqueue a void() thunk
pool.wait();                        // block until all scheduled thunks finish
// destructor drains any remaining work, then joins all threads
```

## Design

One **dispatcher** thread plus N **worker** threads, coordinated by three
counting semaphores:

- `queued` — number of thunks waiting in the task queue.
- `availableWorkers` — number of idle workers.
- per-worker `ready` — raised when the dispatcher assigns that worker a thunk.

The dispatcher blocks on `queued`, then on `availableWorkers`, claims an idle
worker, pops the next thunk, and wakes it. Each worker runs its thunk, marks
itself available again, and decrements an `outstanding` counter; the worker that
drives it to zero notifies `wait()`. Shutdown flips an `atomic<bool> done` and
signals every parked thread so they observe it and exit.

## Verification (WSL2, Ubuntu 24.04, g++ 13.3, 16 CPUs)

```bash
make test        # runs ./tptest
```

All five checks pass — see `../results/assign6_threadpool.txt`:

| Test | Result |
|---|---|
| 1000 increments + reuse for 1000 more | 2000 ✓ |
| captured-argument sum 1..100 | 5050 ✓ |
| 16×100ms tasks on 8 workers | **202 ms** (serial would be 1600 ms) — real parallelism |
| 50 pools destroyed without `wait()` | 1000 tasks drained by destructor ✓ |

**ThreadSanitizer clean:** built with `-fsanitize=thread` and run under
`setarch -R` (ASLR disabled to work around a WSL2 TSan mmap quirk) — exit 0,
zero data-race reports.
