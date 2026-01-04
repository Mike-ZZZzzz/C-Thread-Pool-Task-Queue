#include "thread_pool.h"

#include <stdexcept>

ThreadPool::ThreadPool(std::size_t num_threads) {
  if (num_threads == 0) {
    throw std::invalid_argument("ThreadPool: num_threads must be > 0");
  }

  workers_.reserve(num_threads);
  for (std::size_t i = 0; i < num_threads; i++) {
    workers_.emplace_back([this]() { worker_loop(); });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    stop_ = true;
  }
  cv_.notify_all();

  for (auto& th : workers_) {
    if (th.joinable()) th.join();
  }
}

std::size_t ThreadPool::size() const noexcept {
  return workers_.size();
}

void ThreadPool::worker_loop() {
  while (true) {
    std::function<void()> job;

    {
      std::unique_lock<std::mutex> lk(mu_);
      cv_.wait(lk, [this]() { return stop_ || !tasks_.empty(); });

      // If we're stopping and there is no remaining work, exit.
      if (stop_ && tasks_.empty()) {
        return;
      }

      job = std::move(tasks_.front());
      tasks_.pop_front();
    }

    // Execute outside the lock to avoid blocking other submissions/workers.
    job();
  }
}
