// Unit tests for Parser::WorkersForWorkload, the pure size gate that decides
// how many workers a parse may use. It must never return 0, never exceed the
// caller's cap, and keep small inputs serial (1 worker).
#include "TestFramework.h"
#include <Parser.h>
#include <cstddef>

namespace {
constexpr std::size_t kMiB = 1024u * 1024u;
} // namespace

TEST(WorkersForWorkload, SmallInputStaysSerial) {
  CHECK_EQ(1u, Parser::WorkersForWorkload(0, 16, kMiB));
  CHECK_EQ(1u, Parser::WorkersForWorkload(kMiB - 1, 16, kMiB));
  // Exactly one worker's worth is still a serial job.
  CHECK_EQ(1u, Parser::WorkersForWorkload(kMiB, 16, kMiB));
}

TEST(WorkersForWorkload, ScalesWithInputSize) {
  CHECK_EQ(2u, Parser::WorkersForWorkload(2 * kMiB, 16, kMiB));
  CHECK_EQ(3u, Parser::WorkersForWorkload(4 * kMiB - 1, 16, kMiB));
  CHECK_EQ(8u, Parser::WorkersForWorkload(8 * kMiB, 16, kMiB));
}

TEST(WorkersForWorkload, NeverExceedsCap) {
  CHECK_EQ(16u, Parser::WorkersForWorkload(1000 * kMiB, 16, kMiB));
  CHECK_EQ(1u, Parser::WorkersForWorkload(1000 * kMiB, 1, kMiB));
}

TEST(WorkersForWorkload, DegenerateArgumentsClampToOne) {
  CHECK_EQ(1u, Parser::WorkersForWorkload(1000 * kMiB, 0, kMiB)); // no cap
  // minBytesPerWorker == 0 means "no gating", not a division by zero.
  CHECK_EQ(4u, Parser::WorkersForWorkload(100, 4, 0));
  CHECK_EQ(1u, Parser::WorkersForWorkload(0, 4, 0));
}

TEST(WorkersForWorkload, DefaultThresholdsKeepSmallFilesSerial) {
  const ParallelThresholds defaults;
  // ~1 MB P6 (the size `make bench` showed regressing when threaded).
  CHECK_EQ(1u, Parser::WorkersForWorkload(kMiB, 16,
                                          defaults.p6MinBytesPerWorker));
  // The 32x32 ASCII fixture is nowhere near a P3 worker's minimum.
  CHECK_EQ(1u, Parser::WorkersForWorkload(11 * 1024, 16,
                                          defaults.p3MinBytesPerWorker));
  // A multi-megabyte P3 file still gets the full requested fan-out.
  CHECK_EQ(16u, Parser::WorkersForWorkload(4 * kMiB, 16,
                                           defaults.p3MinBytesPerWorker));
}
