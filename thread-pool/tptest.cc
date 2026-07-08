/**
 * File: tptest.cc
 * ---------------
 * Correctness + concurrency tests for ThreadPool:
 *   1. count/reuse  - 1000 increments, then reuse the pool for 1000 more
 *   2. sum          - captured-argument thunks summing 1..100
 *   3. parallel     - 16 x 100ms tasks on 8 workers must finish well under the
 *                     1600ms a serial run would take (proves real parallelism)
 *   4. lifecycle    - 50 pools destroyed without wait(): destructor must drain
 */

#include "thread-pool.h"
#include <atomic>
#include <chrono>
#include <iostream>
using namespace std;
using namespace chrono;

static bool testCountAndReuse() {
  const size_t kNumThreads = 8, kNumTasks = 1000;
  ThreadPool pool(kNumThreads);
  atomic<int> counter{0};
  for (size_t i = 0; i < kNumTasks; i++) pool.schedule([&counter] { counter++; });
  pool.wait();
  bool ok1 = counter == (int)kNumTasks;
  cout << "  " << (ok1 ? "ok  " : "FAIL") << "1000 increments -> " << counter << " (expect 1000)\n";

  for (size_t i = 0; i < kNumTasks; i++) pool.schedule([&counter] { counter++; });
  pool.wait();
  bool ok2 = counter == (int)(2 * kNumTasks);
  cout << "  " << (ok2 ? "ok  " : "FAIL") << "reuse same pool -> " << counter << " (expect 2000)\n";
  return ok1 && ok2;
}

static bool testCapturedSum() {
  ThreadPool pool(4);
  atomic<long> sum{0};
  const int N = 100;
  for (int i = 1; i <= N; i++) pool.schedule([&sum, i] { sum += i; });
  pool.wait();
  long expect = (long)N * (N + 1) / 2;
  bool ok = sum == expect;
  cout << "  " << (ok ? "ok  " : "FAIL") << "sum 1..100 = " << sum << " (expect " << expect << ")\n";
  return ok;
}

static bool testParallelism() {
  const size_t kNumThreads = 8, kNumTasks = 16, kSleepMs = 100;
  ThreadPool pool(kNumThreads);
  atomic<int> done{0};
  auto start = steady_clock::now();
  for (size_t i = 0; i < kNumTasks; i++)
    pool.schedule([&done, kSleepMs] {
      this_thread::sleep_for(milliseconds(kSleepMs));
      done++;
    });
  pool.wait();
  long elapsed = duration_cast<milliseconds>(steady_clock::now() - start).count();
  long serial = (long)kNumTasks * kSleepMs;
  bool ok = done == (int)kNumTasks && elapsed < serial * 6 / 10;
  cout << "  " << (ok ? "ok  " : "FAIL") << "16x100ms on 8 workers took " << elapsed
       << "ms (serial would be " << serial << "ms)\n";
  return ok;
}

static bool testLifecycle() {
  atomic<int> total{0};
  const int kPools = 50, kPerPool = 20;
  for (int p = 0; p < kPools; p++) {
    ThreadPool pool(4);
    for (int i = 0; i < kPerPool; i++) pool.schedule([&total] { total++; });
    // Intentionally no wait(): the destructor must drain outstanding work.
  }
  bool ok = total == kPools * kPerPool;
  cout << "  " << (ok ? "ok  " : "FAIL") << kPools << " pools x" << kPerPool
       << " tasks, dtor drains -> " << total << " (expect " << kPools * kPerPool << ")\n";
  return ok;
}

int main() {
  cout << "== ThreadPool tests ==\n";
  bool a = testCountAndReuse();
  bool b = testCapturedSum();
  bool c = testParallelism();
  bool d = testLifecycle();
  bool all = a && b && c && d;
  cout << "\n== summary ==\n" << (all ? "ALL PASS" : "SOME FAILED") << endl;
  return all ? 0 : 1;
}
