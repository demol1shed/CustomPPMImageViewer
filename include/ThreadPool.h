#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <type_traits>
#include <thread>
#include <vector>

// A fixed set of long-lived worker threads that run fork-join batches: Run()
// hands out `taskCount` indexed tasks, helps execute them on the calling
// thread, and returns only once every task has finished. This replaces
// spawning (and joining) a fresh std::thread per worker on every parse or
// redraw, which costs tens of microseconds per thread.
//
// Rules for tasks:
//  - A task must not throw (callers keep their try/catch inside the task and
//    report failure through their own per-task flag). As a last line of
//    defence the pool swallows an escaping exception so it can never reach
//    std::terminate, but the task is then simply treated as finished.
//  - A task must not call Run() on the same pool: batches are serialised, so a
//    nested Run() would wait forever on the batch it is part of.
//
// Thread safety: Run() may be called from several threads; concurrent calls
// are serialised (one batch at a time). Results written by tasks are visible
// to the caller after Run() returns (see the synchronisation notes in
// ThreadPool.cpp).
class ThreadPool {
public:
  // Starts `workerCount` background threads. The calling thread of Run() also
  // executes tasks, so a pool of N workers gives N + 1 way parallelism and a
  // pool of 0 workers simply runs every task inline.
  explicit ThreadPool(unsigned workerCount);
  ~ThreadPool();

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  // Process-wide pool sized to the machine (hardware_concurrency() - 1
  // background threads, because the caller is the last participant). Created
  // on first use, joined at exit.
  static ThreadPool &Shared();

  unsigned WorkerCount() const { return static_cast<unsigned>(workers.size()); }

  // Runs task(0) .. task(taskCount - 1) in any order across the pool and the
  // calling thread, and blocks until all have returned.
  template <typename Task> void Run(std::size_t taskCount, Task &&task) {
    RunBatch(
        taskCount,
        [](void *context, std::size_t index) {
          (*static_cast<std::remove_reference_t<Task> *>(context))(index);
        },
        const_cast<void *>(static_cast<const void *>(&task)));
  }

private:
  using TaskThunk = void (*)(void *context, std::size_t index);

  void RunBatch(std::size_t taskCount, TaskThunk thunk, void *context);
  void WorkerLoop();
  // Claims the next unclaimed task of the active batch; requires `lock` held.
  bool ClaimTask(std::size_t &index);
  // Runs one claimed task without holding the lock, then records completion.
  void ExecuteTask(std::size_t index, std::unique_lock<std::mutex> &lock);

  std::vector<std::thread> workers;

  std::mutex runMutex; // serialises Run() callers: one batch at a time
  std::mutex mutex;    // guards every field below
  std::condition_variable workAvailable;
  std::condition_variable batchDone;

  TaskThunk batchThunk = nullptr;
  void *batchContext = nullptr;
  std::size_t batchSize = 0;
  std::size_t nextTask = 0;  // next unclaimed index in [0, batchSize)
  std::size_t finished = 0;  // tasks completed so far
  bool stopping = false;
};

#endif
