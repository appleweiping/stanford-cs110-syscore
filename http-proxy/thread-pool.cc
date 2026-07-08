/**
 * File: thread-pool.cc
 * --------------------
 * Implementation of ThreadPool. Concurrency is coordinated with three counting
 * semaphores and a couple of small mutexes:
 *
 *   queued            - how many thunks sit in `tasks` awaiting dispatch
 *   availableWorkers  - how many workers are currently idle
 *   worker.ready      - per-worker: raised when the dispatcher assigns a thunk
 *
 * The dispatcher blocks on `queued`, then on `availableWorkers`, claims an idle
 * worker, pops the next thunk, and wakes that worker. Each worker runs its thunk,
 * becomes available again, and decrements the outstanding count; the last one to
 * reach zero wakes any thread blocked in wait().
 */

#include "thread-pool.h"
using namespace std;

ThreadPool::ThreadPool(size_t numThreads) : queued(0), availableWorkers(0) {
  wts.reserve(numThreads);
  for (size_t i = 0; i < numThreads; i++) wts.push_back(make_unique<worker_t>());
  for (size_t i = 0; i < numThreads; i++) {
    wts[i]->ts = thread([this, i] { worker(i); });
    availableWorkers.signal();   // every worker begins life idle
  }
  dt = thread([this] { dispatcher(); });
}

void ThreadPool::schedule(const function<void(void)> &thunk) {
  {
    lock_guard<mutex> lg(outstandingLock);
    outstanding++;
  }
  {
    lock_guard<mutex> lg(queueLock);
    tasks.push(thunk);
  }
  queued.signal();
}

void ThreadPool::dispatcher() {
  while (true) {
    queued.wait();
    if (done) break;
    availableWorkers.wait();
    if (done) break;

    size_t id = 0;
    {
      lock_guard<mutex> lg(availLock);
      for (size_t i = 0; i < wts.size(); i++) {
        if (wts[i]->available) { id = i; wts[i]->available = false; break; }
      }
    }

    function<void(void)> thunk;
    {
      lock_guard<mutex> lg(queueLock);
      thunk = move(tasks.front());
      tasks.pop();
    }

    wts[id]->thunk = move(thunk);
    wts[id]->ready.signal();
  }
}

void ThreadPool::worker(size_t id) {
  while (true) {
    wts[id]->ready.wait();
    if (done) break;

    wts[id]->thunk();

    {
      lock_guard<mutex> lg(availLock);
      wts[id]->available = true;
    }
    availableWorkers.signal();

    {
      lock_guard<mutex> lg(outstandingLock);
      if (--outstanding == 0) allDone.notify_all();
    }
  }
}

void ThreadPool::wait() {
  unique_lock<mutex> lk(outstandingLock);
  allDone.wait(lk, [this] { return outstanding == 0; });
}

ThreadPool::~ThreadPool() {
  wait();                 // let any in-flight work finish
  done = true;

  // Wake the dispatcher (it may be parked on either semaphore) so it sees `done`.
  queued.signal();
  availableWorkers.signal();
  dt.join();

  // Wake and reap every worker.
  for (auto &w : wts) {
    w->ready.signal();
    w->ts.join();
  }
}
