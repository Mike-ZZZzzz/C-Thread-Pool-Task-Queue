#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// ThreadPool: fixed-size worker threads + a synchronized task queue.
// Comments are intentionally in English.

class ThreadPool {
 public:
  explicit ThreadPool(std::size_t num_threads);
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  // Submit a callable and its args, returns a future of the result.
  template <class F, class... Args>
  auto submit(F&& f, Args&&... args)
      -> std::future<std::invoke_result_t<F, Args...>>;

  std::size_t size() const noexcept;

 private:
  void worker_loop();

  mutable std::mutex mu_;
  std::condition_variable cv_;
  bool stop_{false};

  std::deque<std::function<void()>> tasks_;
  std::vector<std::thread> workers_;
};

template <class F, class... Args>
auto ThreadPool::submit(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>> {
  using R = std::invoke_result_t<F, Args...>;

  // Bind callable + args into a nullary function R().
  auto bound = [func = std::forward<F>(f),
                tup = std::make_tuple(std::forward<Args>(args)...)]() mutable -> R {
    return std::apply(std::move(func), std::move(tup));
  };

  // Pack the task into a packaged_task so we can return a future<R>.
  auto task_ptr = std::make_shared<std::packaged_task<R()>>(std::move(bound));
  std::future<R> fut = task_ptr->get_future();

  {
    std::lock_guard<std::mutex> lk(mu_);
    if (stop_) {
      throw std::runtime_error("submit() on stopped ThreadPool");
    }
    tasks_.emplace_back([task_ptr]() { (*task_ptr)(); });
  }
  cv_.notify_one();
  return fut;
}
