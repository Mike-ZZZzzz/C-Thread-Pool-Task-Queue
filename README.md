# C++ Thread Pool + Task Queue

A small, from-scratch C++17 thread pool that executes submitted tasks on a fixed set of worker threads.  
Implements a synchronized task queue with `std::mutex` + `std::condition_variable`, supports returning values and propagating exceptions via `std::future`.

## Features
- **Fixed-size worker pool**: create `N` worker threads once; reuse them for many tasks.
- **Task queue**: thread-safe FIFO queue (`std::deque<std::function<void()>>`) protected by a mutex.
- **Blocking workers**: workers sleep when no tasks are available (no busy-waiting).
- **`submit()` API**:
  - accepts any callable + arguments
  - returns `std::future<R>` for the task result
  - propagates task exceptions through `future.get()`
- **Safe shutdown**:
  - destructor sets a stop flag
  - wakes all workers
  - joins all threads (prevents use-after-free on internal state)

## Design Overview

### High-level architecture
- `ThreadPool` owns:
  - `workers_`: `std::vector<std::thread>` — the worker threads
  - `tasks_`: `std::deque<std::function<void()>>` — the pending tasks
  - `mu_`: `std::mutex` — protects `tasks_` and `stop_`
  - `cv_`: `std::condition_variable` — wakes workers when tasks arrive or shutdown begins
  - `stop_`: `bool` — signals workers to stop once the queue is drained

### Worker loop
Each worker runs a loop:
1. Acquire the mutex and **wait** on the condition variable until either:
   - `tasks_` is not empty (work is available), or
   - `stop_` is set (shutdown requested)
2. If `stop_ && tasks_.empty()`, exit the loop (thread terminates).
3. Pop one task from the queue.
4. **Release the lock**.
5. Execute the task **outside the lock** to avoid blocking other workers and `submit()` calls.

### Task submission and futures
`submit()` wraps the callable into a `std::packaged_task<R()>`, obtains a `std::future<R>`, then enqueues a `void()` wrapper that invokes the packaged task.  
This provides:
- a uniform `void()` type for the internal queue, and
- a typed result channel (`future`) to the caller.

## Build & Run

### Build (Debug)
```bash
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j
