/**
 * File: thread-pool.h
 * -------------------
 * Exports the CS110 assign6 ThreadPool: a fixed set of worker threads driven by
 * a single dispatcher thread. Callers schedule() a thunk (a std::function taking
 * and returning void); the dispatcher hands each queued thunk to the next idle
 * worker. wait() blocks until every thunk scheduled so far has finished. The
 * pool is reusable and the destructor tears everything down cleanly.
 */

#pragma once
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>
#include <queue>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "semaphore.h"

class ThreadPool {
 public:
  explicit ThreadPool(size_t numThreads);
  void schedule(const std::function<void(void)> &thunk);
  void wait();
  ~ThreadPool();

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

 private:
  struct worker_t {
    std::thread ts;
    semaphore ready;                    // signalled when this worker has a thunk
    std::function<void(void)> thunk;
    bool available = true;
    worker_t() : ready(0) {}
  };

  std::thread dt;                       // dispatcher thread
  std::vector<std::unique_ptr<worker_t>> wts;

  std::queue<std::function<void(void)>> tasks;  // scheduled, not yet dispatched
  std::mutex queueLock;
  semaphore queued;                     // counts entries in `tasks`
  semaphore availableWorkers;           // counts idle workers

  std::mutex availLock;                 // guards worker_t::available

  size_t outstanding = 0;               // scheduled but not yet completed
  std::mutex outstandingLock;
  std::condition_variable allDone;

  std::atomic<bool> done{false};

  void dispatcher();
  void worker(size_t id);
};
