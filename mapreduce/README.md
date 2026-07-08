# assign8 — MapReduce (optional / single-node)

A working MapReduce engine that reproduces the real **map → shuffle/partition →
reduce** pipeline on a single machine. The mapper and reducer are arbitrary
external programs the engine `exec`s (word-count examples are provided), so this
is a genuine harness rather than a hard-coded counter.

```bash
make
./mr <input> <M> <R> <mapper-exe> <reducer-exe> <out-dir>
# e.g.  ./mr tests/input.txt 4 3 ./mapper ./reducer tests/out
```

## Pipeline

1. **Map** — split the input into `M` line-chunks; run `<mapper>` on each as a
   real subprocess (stdin = chunk, stdout = `key\tvalue` lines). Mappers run
   concurrently on the assign6 `ThreadPool`.
2. **Shuffle** — hash each emitted key to one of `R` partitions (`hash(key) % R`).
3. **Reduce** — sort each partition by key so records are grouped, feed it to
   `<reducer>` (subprocess), and write `out-dir/part-<r>`. Reducers run
   concurrently too.

A writer thread pushes each subprocess's stdin while the parent drains its
stdout, so large chunks can't deadlock on a full pipe buffer.

## Real bug found & fixed

Running `M` mappers concurrently initially **hung**: each `fork()` inherited the
*other* in-flight mappers' pipe write-ends, so the reader never saw EOF. Fixed by
creating the subprocess pipes with **`O_CLOEXEC`** (see `subprocess.cc`) — the
one child that wants a pipe end `dup2`s it onto fd 0/1 (which clears `O_CLOEXEC`),
while every unrelated child closes the inherited copies on `exec`. After the fix,
the suite passed **5/5 consecutive runs** with no hangs.

## Verification (WSL2, Ubuntu 24.04, g++ 13.3), `-Wall -Wextra -Werror` clean

`make test` word-counts `tests/input.txt` (M=4, R=3). Captured in
`../results/assign8_mapreduce.txt` — **7/7 pass**:

| Check | Result |
|---|---|
| per-word totals (`alpha`/`beta`/`gamma`/`delta`) | 16 / 12 / 16 / 4 ✓ |
| distinct keys across all partitions | 4 ✓ |
| no key split across partitions (shuffle correct) | ✓ |
| sum of counts = input token count | 48 ✓ |

## Scope

The **graded** CS110 assign8 distributes workers across the myth cluster over
TCP using the course's private MapReduce server/worker scaffold; assign8 is
released *optional and ungraded*. That multi-machine/network layer is replaced
here by local process spawning — a concrete, documented deviation. The
map/shuffle/reduce mechanics, the mapper/reducer-as-separate-executables model,
and the concurrency (ThreadPool + subprocess) are all real and verified.
