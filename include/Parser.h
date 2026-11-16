#pragma once
#include "Image.h"
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// A half-open range of image rows [startRow, endRow) assigned to one worker
// when parsing P6 pixel data in parallel.
struct RowRange {
  int startRow;
  int endRow;
};

// A half-open range of bytes [start, end) of the ASCII (P3) value region
// assigned to one worker. P3 token offsets are data-dependent (variable-width
// decimal tokens), so unlike P6 the work is split by bytes, not rows, with
// token-boundary resync.
struct ByteRange {
  std::size_t start;
  std::size_t end;
};

// Size gates that decide when a parse is big enough to be worth threading.
// Spawning a std::thread costs tens of microseconds, so on small inputs the
// spawn cost exceeds the work saved and a "parallel" parse is slower than the
// serial one. Defaults come from size sweeps with the
// `make bench` harness; callers normally never override them.
struct ParallelThresholds {
  // P6 is I/O / memcpy bound: a worker only pays for itself on a large slice.
  std::size_t p6MinBytesPerWorker = 4u * 1024u * 1024u;
  // P3 is CPU bound (integer parsing), so much smaller slices already pay off.
  std::size_t p3MinBytesPerWorker = 32u * 1024u;
  // The parallel P3 decode walks the data twice (count, then parse), so two
  // workers do roughly the serial amount of work each. Two-worker parses only
  // win once the input is large enough to amortize that second pass.
  std::size_t p3MinBytesForTwoWorkers = 8u * 1024u * 1024u;

  // No size gating: any input with >= 2 rows may be split. For tests and
  // benchmarks that must drive the parallel paths on tiny fixtures.
  static constexpr ParallelThresholds AlwaysParallel() { return {1, 1, 0}; }
};

class Parser {
public:
  // How many workers a `workloadBytes`-sized job should use: as many as fit
  // with at least `minBytesPerWorker` bytes each, capped at `maxWorkers` and
  // never below 1 (1 == stay serial). Pure — exposed for unit testing. A
  // `minBytesPerWorker` of 0 is treated as 1 (no gating).
  static unsigned WorkersForWorkload(std::size_t workloadBytes,
                                     unsigned maxWorkers,
                                     std::size_t minBytesPerWorker);

  // Splits `height` rows across `threadCount` workers into contiguous,
  // non-overlapping ranges whose union is exactly [0, height). Pure: no
  // clamping, no I/O, no printing — exposed so the partition math is unit
  // testable. When threadCount > height some trailing ranges are empty
  // (startRow == endRow); callers are expected to skip those.
  static std::vector<RowRange> ComputeRowRanges(int height, int threadCount);

  // Splits `totalBytes` into `threadCount` contiguous, non-overlapping byte
  // windows whose union is exactly [0, totalBytes). Same balanced split as
  // ComputeRowRanges but over bytes; empties possible when threadCount >
  // totalBytes. Pure — exposed for unit testing the P3 partition.
  static std::vector<ByteRange> ComputeByteRanges(std::size_t totalBytes,
                                                  int threadCount);

  // Counts the whitespace-separated tokens in base[0, totalBytes) that are
  // OWNED by the window [chunkStart, chunkEnd) under the rule "a token belongs
  // to the window holding its first byte". Pure (no parsing/I/O) — exposed so
  // the P3 boundary-resync math is unit testable.
  static std::size_t CountTokensInRange(const char *base,
                                        std::size_t totalBytes,
                                        std::size_t chunkStart,
                                        std::size_t chunkEnd);

  // Returns nullopt if parsing fails. `threadCount` is an upper bound: the
  // parser uses fewer workers (or none) when `thresholds` says the input is
  // too small for threading to pay off.
  static std::optional<Image>
  ParseFile(const std::string &filePath, uint threadCount, bool verbose = false,
            const ParallelThresholds &thresholds = ParallelThresholds{});
};
