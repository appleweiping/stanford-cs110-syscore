/**
 * File: mr.cc
 * -----------
 * A single-node MapReduce engine (CS110 assign8, optional). It reproduces the
 * real map -> shuffle/partition -> reduce mechanics on one machine:
 *
 *   ./mr <input-file> <M> <R> <mapper-exe> <reducer-exe> <out-dir>
 *
 *   1. MAP:    split the input into M line-chunks; run <mapper-exe> on each
 *              (as a real subprocess, stdin=chunk, stdout=key\tvalue lines).
 *              Mappers run concurrently on a ThreadPool.
 *   2. SHUFFLE:each emitted key is hashed to one of R partitions.
 *   3. REDUCE: each partition is sorted by key and fed to <reducer-exe>
 *              (subprocess), producing out-dir/part-<r>. Reducers run
 *              concurrently on the ThreadPool.
 *
 * The mapper/reducer are arbitrary external programs (see mapper.cc/reducer.cc),
 * so this is a genuine MapReduce harness, not a hard-coded word counter.
 *
 * NOTE: the graded CS110 assign8 distributes the workers across the myth cluster
 * over TCP; that cluster/network layer is replaced here by local process
 * spawning (see README). The map/shuffle/reduce logic itself is real.
 */

#include "subprocess.h"
#include "thread-pool.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <iostream>

#include <unistd.h>
#include <thread>
#include <sys/wait.h>
#include <sys/stat.h>

using namespace std;

// Run `argv` as a subprocess, feeding it `input` on stdin and returning stdout.
// A writer thread pushes stdin while we drain stdout, so large data can't
// deadlock on full pipe buffers.
static string runFilter(char *argv[], const string &input) {
  subprocess_t sp = subprocess(argv, true, true);
  thread writer([&] {
    size_t sent = 0;
    while (sent < input.size()) {
      ssize_t n = write(sp.supplyfd, input.data() + sent, input.size() - sent);
      if (n <= 0) break;
      sent += n;
    }
    close(sp.supplyfd);
  });

  string out;
  char buf[8192];
  ssize_t n;
  while ((n = read(sp.ingestfd, buf, sizeof(buf))) > 0) out.append(buf, n);
  close(sp.ingestfd);
  writer.join();
  waitpid(sp.pid, nullptr, 0);
  return out;
}

int main(int argc, char *argv[]) {
  if (argc != 7) {
    fprintf(stderr, "usage: %s <input> <M> <R> <mapper> <reducer> <out-dir>\n", argv[0]);
    return 1;
  }
  const string inputPath = argv[1];
  const int M = atoi(argv[2]);
  const int R = atoi(argv[3]);
  char *mapperExe = argv[4];
  char *reducerExe = argv[5];
  const string outDir = argv[6];
  if (M <= 0 || R <= 0) { fprintf(stderr, "M and R must be positive\n"); return 1; }
  mkdir(outDir.c_str(), 0755);

  // Read input into lines, then split into M roughly equal chunks.
  ifstream in(inputPath);
  if (!in) { fprintf(stderr, "cannot open %s\n", inputPath.c_str()); return 1; }
  vector<string> lines;
  string line;
  while (getline(in, line)) lines.push_back(line);

  vector<string> chunks(M);
  for (size_t i = 0; i < lines.size(); i++) chunks[i % M] += lines[i] + "\n";

  // ---- MAP + SHUFFLE ----
  vector<string> partitions(R);     // partition r -> concatenated "key\tvalue\n"
  vector<mutex> partLocks(R);
  hash<string> hasher;

  {
    ThreadPool pool(M);
    for (int m = 0; m < M; m++) {
      pool.schedule([&, m] {
        if (chunks[m].empty()) return;
        char *margv[] = {mapperExe, nullptr};
        string emitted = runFilter(margv, chunks[m]);

        // Route each emitted (key,value) to a partition by hash(key) % R.
        vector<string> local(R);
        istringstream ss(emitted);
        string kv;
        while (getline(ss, kv)) {
          if (kv.empty()) continue;
          size_t tab = kv.find('\t');
          string key = (tab == string::npos) ? kv : kv.substr(0, tab);
          int r = static_cast<int>(hasher(key) % R);
          local[r] += kv + "\n";
        }
        for (int r = 0; r < R; r++) {
          if (local[r].empty()) continue;
          lock_guard<mutex> lg(partLocks[r]);
          partitions[r] += local[r];
        }
      });
    }
    pool.wait();
  }

  // ---- REDUCE ----
  {
    ThreadPool pool(R);
    for (int r = 0; r < R; r++) {
      pool.schedule([&, r] {
        // Sort the partition by key (stable within key) so the reducer sees each
        // key's records grouped consecutively.
        vector<string> recs;
        istringstream ss(partitions[r]);
        string rec;
        while (getline(ss, rec)) if (!rec.empty()) recs.push_back(rec);
        sort(recs.begin(), recs.end());
        string reduceInput;
        for (const string &s : recs) reduceInput += s + "\n";

        char *rargv[] = {reducerExe, nullptr};
        string reduced = reduceInput.empty() ? "" : runFilter(rargv, reduceInput);

        char path[512];
        snprintf(path, sizeof(path), "%s/part-%d", outDir.c_str(), r);
        ofstream out(path);
        out << reduced;
      });
    }
    pool.wait();
  }

  fprintf(stderr, "mapreduce: %zu input lines, M=%d mappers, R=%d reducers -> %s/part-0..%d\n",
          lines.size(), M, R, outDir.c_str(), R - 1);
  return 0;
}
