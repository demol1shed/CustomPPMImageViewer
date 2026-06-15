// Unit tests for Parser::ComputeRowRanges, the pure row-partition helper that
// will hand disjoint row ranges to the P6 worker threads. The whole point of
// parallel correctness rests on these ranges being gap-free, non-overlapping,
// and covering exactly [0, height) — so we assert those invariants directly.
#include "TestFramework.h"
#include <Parser.h>
#include <vector>

namespace {

// Asserts the partition invariants for a single (height, threadCount) pair.
void checkPartition(int height, int threadCount) {
  std::vector<RowRange> ranges = Parser::ComputeRowRanges(height, threadCount);

  CHECK_EQ(threadCount, static_cast<int>(ranges.size()));
  if (ranges.empty())
    return;

  CHECK_EQ(0, ranges.front().startRow); // first range starts at 0
  CHECK_EQ(height, ranges.back().endRow); // last range ends at height

  int covered = 0;
  for (size_t i = 0; i < ranges.size(); ++i) {
    CHECK_TRUE(ranges[i].endRow >= ranges[i].startRow); // no negative size
    if (i > 0)
      CHECK_EQ(ranges[i - 1].endRow, ranges[i].startRow); // telescoping
    covered += ranges[i].endRow - ranges[i].startRow;
  }
  CHECK_EQ(height, covered); // sizes sum to exactly height
}

} // namespace

TEST(RowRanges, InvariantsHoldAcrossManyCombinations) {
  const int heights[] = {1, 2, 4, 7, 10, 32};
  const int threads[] = {1, 2, 3, 4, 8, 16};
  for (int h : heights)
    for (int t : threads)
      checkPartition(h, t);
}

TEST(RowRanges, EmptyForNonPositiveHeight) {
  CHECK_TRUE(Parser::ComputeRowRanges(0, 4).empty());
  CHECK_TRUE(Parser::ComputeRowRanges(-5, 4).empty());
}

TEST(RowRanges, ThreadCountBelowOneTreatedAsOne) {
  std::vector<RowRange> r = Parser::ComputeRowRanges(10, 0);
  CHECK_EQ(1, static_cast<int>(r.size()));
  CHECK_EQ(0, r.front().startRow);
  CHECK_EQ(10, r.front().endRow);
}

TEST(RowRanges, MoreThreadsThanRowsYieldsEmptyTailRanges) {
  // 4 rows across 8 workers: 8 ranges, 4 of them empty, still covering [0, 4).
  std::vector<RowRange> r = Parser::ComputeRowRanges(4, 8);
  CHECK_EQ(8, static_cast<int>(r.size()));
  CHECK_EQ(0, r.front().startRow);
  CHECK_EQ(4, r.back().endRow);

  int covered = 0;
  int empties = 0;
  for (const RowRange &rr : r) {
    CHECK_TRUE(rr.endRow >= rr.startRow);
    covered += rr.endRow - rr.startRow;
    if (rr.endRow == rr.startRow)
      empties++;
  }
  CHECK_EQ(4, covered);
  CHECK_TRUE(empties > 0);
}
