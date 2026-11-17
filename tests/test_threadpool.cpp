// Unit tests for ThreadPool: every task runs exactly once, Run() blocks until
// all are done (results are visible afterwards), the pool is reusable across
// batches, and degenerate pools/batches behave.
#include "TestFramework.h"
#include <ThreadPool.h>
#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <thread>
#include <vector>

TEST(ThreadPool, RunsEveryTaskExactlyOnce) {
  ThreadPool pool(4);
  std::vector<int> hits(1000, 0); // one slot per task: disjoint writes
  pool.Run(hits.size(), [&](std::size_t i) { hits[i] += 1; });
  int wrong = 0;
  for (int h : hits)
    wrong += (h != 1);
  CHECK_EQ(0, wrong);
}

TEST(ThreadPool, ZeroWorkersRunsInlineOnCaller) {
  ThreadPool pool(0);
  const std::thread::id caller = std::this_thread::get_id();
  int onCaller = 0;
  pool.Run(8, [&](std::size_t) {
    onCaller += (std::this_thread::get_id() == caller);
  });
  CHECK_EQ(8, onCaller);
}

TEST(ThreadPool, EmptyBatchReturnsImmediately) {
  ThreadPool pool(2);
  bool ran = false;
  pool.Run(0, [&](std::size_t) { ran = true; });
  CHECK_FALSE(ran);
}

TEST(ThreadPool, ReusableAcrossManyBatches) {
  ThreadPool pool(3);
  long total = 0;
  for (int batch = 0; batch < 500; ++batch) {
    std::vector<long> part(7, 0);
    pool.Run(part.size(), [&](std::size_t i) { part[i] = batch + (long)i; });
    for (long v : part)
      total += v; // reading right after Run() must see every write
  }
  long expected = 0;
  for (int batch = 0; batch < 500; ++batch)
    for (int i = 0; i < 7; ++i)
      expected += batch + i;
  CHECK_EQ(expected, total);
}

TEST(ThreadPool, MoreTasksThanThreadsStillCompletes) {
  ThreadPool pool(1);
  std::atomic<int> count{0};
  pool.Run(257, [&](std::size_t) { count.fetch_add(1); });
  CHECK_EQ(257, count.load());
}

TEST(ThreadPool, EscapingExceptionDoesNotTerminateOrHang) {
  ThreadPool pool(2);
  std::atomic<int> count{0};
  pool.Run(16, [&](std::size_t i) {
    count.fetch_add(1);
    if (i % 3 == 0)
      throw std::runtime_error("boom");
  });
  CHECK_EQ(16, count.load());
}

TEST(ThreadPool, ConcurrentCallersAreSerialisedAndBothComplete) {
  ThreadPool pool(3);
  std::atomic<int> a{0}, b{0};
  std::thread other([&] {
    for (int i = 0; i < 100; ++i)
      pool.Run(5, [&](std::size_t) { b.fetch_add(1); });
  });
  for (int i = 0; i < 100; ++i)
    pool.Run(5, [&](std::size_t) { a.fetch_add(1); });
  other.join();
  CHECK_EQ(500, a.load());
  CHECK_EQ(500, b.load());
}

TEST(ThreadPool, SharedPoolIsASingleton) {
  CHECK_TRUE(&ThreadPool::Shared() == &ThreadPool::Shared());
}
