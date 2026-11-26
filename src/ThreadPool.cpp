#include <ThreadPool.h>
#include <algorithm>

// Synchronisation design
// ----------------------
// One mutex guards the batch description (thunk, context, size, nextTask,
// finished). A batch is published and consumed entirely under that mutex:
//
//   caller:  lock -> publish batch -> notify workers (if > 1 task) ->
//            claim/run tasks -> wait until finished == batchSize ->
//            clear batch -> unlock
//   worker:  lock -> wait for an unclaimed task -> claim index -> UNLOCK ->
//            run task -> lock -> ++finished (notify caller if last)
//
// Memory visibility: everything a task writes happens-before its
// `++finished` under the mutex, and the caller reads `finished` under the
// same mutex before Run() returns, so the caller sees every task's results
// without any extra atomics. Likewise, the caller's writes before Run()
// (input buffers, per-task output slots) happen-before the workers' claim
// under the mutex. Tasks write disjoint memory, so they need no locking
// among themselves.

namespace {
unsigned DefaultWorkerCount() {
  const unsigned hardware = std::thread::hardware_concurrency();
  // hardware_concurrency() == 0 means unknown: stay inline (0 workers). The
  // caller is itself one participant, hence the minus one.
  return hardware > 1 ? hardware - 1 : 0;
}
} // namespace

ThreadPool::ThreadPool(unsigned workerCount) {
  workers.reserve(workerCount);
  for (unsigned i = 0; i < workerCount; ++i)
    workers.emplace_back(&ThreadPool::WorkerLoop, this);
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> guard(mutex);
    stopping = true;
  }
  workAvailable.notify_all();
  for (std::thread &worker : workers)
    worker.join();
}

ThreadPool &ThreadPool::Shared() {
  static ThreadPool pool(DefaultWorkerCount()); // thread-safe since C++11
  return pool;
}

bool ThreadPool::ClaimTask(std::size_t &index) {
  if (nextTask >= batchSize)
    return false;
  index = nextTask++;
  return true;
}

void ThreadPool::ExecuteTask(std::size_t index,
                             std::unique_lock<std::mutex> &lock) {
  // Copy the batch description while still locked: once we unlock, the caller
  // may (after the last task completes) overwrite it for the next batch.
  const TaskThunk thunk = batchThunk;
  void *const context = batchContext;
  lock.unlock();
  try {
    thunk(context, index);
  } catch (...) {
    // Tasks must handle their own errors; never let one reach std::terminate.
  }
  lock.lock();
  if (++finished == batchSize)
    batchDone.notify_all();
}

void ThreadPool::WorkerLoop() {
  std::unique_lock<std::mutex> lock(mutex);
  for (;;) {
    std::size_t index = 0;
    workAvailable.wait(lock, [&] { return stopping || ClaimTask(index); });
    // The predicate short-circuits on `stopping`, so when it is set no task was
    // claimed. The destructor only runs once no Run() is in flight, so there is
    // never unfinished work to abandon here. Otherwise ClaimTask() succeeded
    // under the lock and `index` belongs to this worker.
    if (stopping)
      return;
    ExecuteTask(index, lock);
  }
}

void ThreadPool::RunBatch(std::size_t taskCount, TaskThunk thunk,
                          void *context) {
  if (taskCount == 0)
    return;

  std::lock_guard<std::mutex> oneBatchAtATime(runMutex);
  std::unique_lock<std::mutex> lock(mutex);
  batchThunk = thunk;
  batchContext = context;
  batchSize = taskCount;
  nextTask = 0;
  finished = 0;
  // The caller runs one task itself, so wake only as many workers as can find
  // work: none for a single task, one for two. A worker that is not woken
  // cannot strand a task, because the caller drains whatever is unclaimed.
  if (taskCount == 2)
    workAvailable.notify_one();
  else if (taskCount > 2)
    workAvailable.notify_all();

  // The caller works too instead of idling until the batch completes.
  std::size_t index = 0;
  while (ClaimTask(index))
    ExecuteTask(index, lock);

  batchDone.wait(lock, [&] { return finished == batchSize; });
  batchThunk = nullptr;
  batchContext = nullptr;
  batchSize = 0;
  nextTask = 0;
}
