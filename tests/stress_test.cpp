#include "thread_pool.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

// Comments are intentionally in English.

static void require(bool cond, const char* msg) {
  if (!cond) {
    std::cerr << "FAIL: " << msg << "\n";
    std::exit(1);
  }
}

int main() {
  const std::size_t nthreads = std::max<std::size_t>(2, std::thread::hardware_concurrency());
  ThreadPool pool(nthreads);

  // Test 1: Many tasks increment a shared counter (checks concurrency works).
  std::atomic<std::int64_t> sum{0};
  const int N = 20000;

  std::vector<std::future<void>> futs;
  futs.reserve(N);
  for (int i = 0; i < N; i++) {
    futs.push_back(pool.submit([&sum]() { sum.fetch_add(1, std::memory_order_relaxed); }));
  }
  for (auto& f : futs) f.get();
  require(sum.load(std::memory_order_relaxed) == N, "counter increments incorrect");

  // Test 2: Return values via future.
  auto f1 = pool.submit([](int a, int b) { return a + b; }, 7, 35);
  require(f1.get() == 42, "future return value incorrect");

  // Test 3: Exception propagation via future.
  auto f2 = pool.submit([]() -> int { throw std::runtime_error("boom"); });
  try {
    (void)f2.get();
    require(false, "exception should have propagated");
  } catch (const std::runtime_error&) {
    // expected
  } catch (...) {
    require(false, "wrong exception type propagated");
  }

  // Test 4: Randomized stress: mix CPU work + small sleeps.
  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> work_dist(1, 200);
  std::uniform_int_distribution<int> sleep_dist(0, 50);

  std::atomic<std::int64_t> checksum{0};
  const int M = 10000;

  std::vector<std::future<std::int64_t>> futs2;
  futs2.reserve(M);

  for (int i = 0; i < M; i++) {
    int work = work_dist(rng);
    int slp = sleep_dist(rng);
    futs2.push_back(pool.submit([work, slp, &checksum]() -> std::int64_t {
      std::int64_t local = 0;
      for (int k = 1; k <= work; k++) local += k;
      if (slp > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(slp));
      }
      checksum.fetch_add(local, std::memory_order_relaxed);
      return local;
    }));
  }

  std::int64_t expected = 0;
  for (auto& f : futs2) expected += f.get();

  require(expected == checksum.load(std::memory_order_relaxed), "checksum mismatch");

  std::cout << "OK: thread pool + task queue works (futures + shutdown + stress)\n";
  return 0;
}
