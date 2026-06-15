#pragma once
#include "Image.h"
#include <optional>
#include <string>
#include <vector>

// A half-open range of image rows [startRow, endRow) assigned to one worker
// when parsing P6 pixel data in parallel.
struct RowRange {
  int startRow;
  int endRow;
};

class Parser {
public:
  // Splits `height` rows across `threadCount` workers into contiguous,
  // non-overlapping ranges whose union is exactly [0, height). Pure: no
  // clamping, no I/O, no printing — exposed so the partition math is unit
  // testable. When threadCount > height some trailing ranges are empty
  // (startRow == endRow); callers are expected to skip those.
  static std::vector<RowRange> ComputeRowRanges(int height, int threadCount);

  static std::optional<Image>
  ParseFile(const std::string &filePath, uint threadCount,
            bool verbose = false); // returns nullopt if parsing fails
};
