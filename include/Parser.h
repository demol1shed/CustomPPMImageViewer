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

class Parser {
public:
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

  static std::optional<Image>
  ParseFile(const std::string &filePath, uint threadCount,
            bool verbose = false); // returns nullopt if parsing fails
};
