// Unit tests for the pure P3 partition helpers: Parser::ComputeByteRanges and
// Parser::CountTokensInRange. Parallel P3 correctness rests on the byte windows
// tiling the value region exactly and on the token-ownership rule counting each
// token in exactly one window — so we assert those invariants directly,
// including the proof made executable (Σ counts over a full partition == the
// true token count) and the tricky boundary-inside-a-number case.
#include "TestFramework.h"
#include <Parser.h>
#include <string>
#include <vector>

namespace {

// True token count = number of maximal non-whitespace runs.
std::size_t trueTokens(const std::string &s) {
  std::size_t count = 0;
  bool inTok = false;
  for (char c : s) {
    const bool ws = (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
                     c == '\v' || c == '\f');
    if (ws) {
      inTok = false;
    } else if (!inTok) {
      ++count;
      inTok = true;
    }
  }
  return count;
}

// Sum of owned-token counts across a full N-way partition of the buffer.
std::size_t sumOverPartition(const std::string &s, int parts) {
  const char *base = s.data();
  const std::size_t n = s.size();
  std::size_t total = 0;
  for (const ByteRange &r : Parser::ComputeByteRanges(n, parts))
    total += Parser::CountTokensInRange(base, n, r.start, r.end);
  return total;
}

} // namespace

TEST(ByteRanges, PartitionInvariants) {
  const std::size_t sizes[] = {0, 1, 2, 5, 16, 100, 1000};
  const int threads[] = {1, 2, 3, 4, 8, 16};
  for (std::size_t n : sizes) {
    for (int t : threads) {
      std::vector<ByteRange> r = Parser::ComputeByteRanges(n, t);
      CHECK_EQ(t, static_cast<int>(r.size()));
      CHECK_EQ(static_cast<std::size_t>(0), r.front().start);
      CHECK_EQ(n, r.back().end);
      std::size_t covered = 0;
      for (std::size_t i = 0; i < r.size(); ++i) {
        CHECK_TRUE(r[i].end >= r[i].start);
        if (i > 0)
          CHECK_EQ(r[i - 1].end, r[i].start); // telescoping, no gap/overlap
        covered += r[i].end - r[i].start;
      }
      CHECK_EQ(n, covered); // sizes sum to exactly totalBytes
    }
  }
}

TEST(ByteRanges, ThreadCountBelowOneTreatedAsOne) {
  std::vector<ByteRange> r = Parser::ComputeByteRanges(10, 0);
  CHECK_EQ(1, static_cast<int>(r.size()));
  CHECK_EQ(static_cast<std::size_t>(0), r.front().start);
  CHECK_EQ(static_cast<std::size_t>(10), r.front().end);
}

TEST(TokenCount, SumOverPartitionEqualsTrueCount) {
  const std::string cases[] = {
      "1 2 3",
      "  10   20  30   ",
      "0 255 0 16 239 8 32 223 16",
      "\n\n 1\t2\n3 \n",
      "123",                 // single token
      "   ",                 // whitespace only
      "1 22 333 4444 55555", // varied widths
  };
  for (const std::string &s : cases) {
    const std::size_t expected = trueTokens(s);
    for (int parts : {1, 2, 3, 4, 5, 8, 16, 32})
      CHECK_EQ(expected, sumOverPartition(s, parts));
  }
}

TEST(TokenCount, WholeRangeCountsEveryToken) {
  std::string s = "10 20 30 40";
  CHECK_EQ(static_cast<std::size_t>(4),
           Parser::CountTokensInRange(s.data(), s.size(), 0, s.size()));
}

TEST(TokenCount, BoundaryInsideNumberOwnedByFirstByteWindow) {
  // "12 345 6": bytes — 0:'1' 1:'2' 2:' ' 3:'3' 4:'4' 5:'5' 6:' ' 7:'6'.
  // Window A=[0,4) owns "12" (start 0) and "345" (start 3 < 4). Window B=[4,8)
  // starts inside "345" (base[3] non-ws) so it skips that token and owns "6".
  std::string s = "12 345 6";
  const std::size_t n = s.size();
  const std::size_t a = Parser::CountTokensInRange(s.data(), n, 0, 4);
  const std::size_t b = Parser::CountTokensInRange(s.data(), n, 4, 8);
  CHECK_EQ(static_cast<std::size_t>(2), a);
  CHECK_EQ(static_cast<std::size_t>(1), b);
  CHECK_EQ(static_cast<std::size_t>(3), a + b);
}
