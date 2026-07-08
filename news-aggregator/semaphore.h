/**
 * File: semaphore.h
 * -----------------
 * A minimal counting semaphore built on std::mutex + std::condition_variable,
 * standing in for the CS110 course's `semaphore` helper. wait() is P (decrement,
 * blocking while the count is zero); signal() is V (increment, wake one waiter).
 */

#pragma once
#include <mutex>
#include <condition_variable>

class semaphore {
 public:
  explicit semaphore(int value = 0) : value(value) {}
  semaphore(const semaphore &) = delete;
  semaphore &operator=(const semaphore &) = delete;

  void wait() {
    std::unique_lock<std::mutex> lock(m);
    cv.wait(lock, [this] { return value > 0; });
    value--;
  }

  void signal() {
    std::lock_guard<std::mutex> lock(m);
    value++;
    cv.notify_one();
  }

 private:
  int value;
  std::mutex m;
  std::condition_variable cv;
};
