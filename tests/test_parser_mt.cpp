// Multithreading-focused tests for Parser::ParseFile on the 32x32 P6 fixture.
// 32 rows let the row-splitter exercise both even (/2,/4,/8,/16) and uneven
// (/3,/5,/7) divisions plus the "more threads than rows" clamp. The contract
// under test: a parallel decode is byte-identical to the serial one and to the
// known pixel formula, for every thread count.
#include "SampleData.h"
#include "TestFramework.h"
#include <Parser.h>
#include <string>

namespace {
// Thread counts spanning: serial (1), even/uneven splits, exactly #rows (32),
// just over #rows (33), and far over every cap (1000 -> clamped).
const uint kThreadCounts[] = {1u, 2u, 3u, 4u, 5u, 8u, 16u, 32u, 33u, 1000u};
} // namespace

TEST(ParserMT, LargeFixtureHeaderAndSize) {
  auto img = Parser::ParseFile(sample::largeBinaryPath(), 1, false);
  CHECK_TRUE(img.has_value());
  if (!img)
    return;
  CHECK_EQ(std::string("P6"), img->imageHeader.ppmType);
  CHECK_EQ(sample::kWidthLarge, img->imageHeader.width);
  CHECK_EQ(sample::kHeightLarge, img->imageHeader.height);
  CHECK_EQ(sample::kPixelCountLarge, static_cast<int>(img->pixelData.size()));
}

// The core guarantee: every thread count yields a buffer byte-identical to the
// single-threaded read.
TEST(ParserMT, ParallelMatchesSerialAcrossThreadCounts) {
  auto serial = Parser::ParseFile(sample::largeBinaryPath(), 1, false);
  CHECK_TRUE(serial.has_value());
  if (!serial)
    return;

  for (uint threads : kThreadCounts) {
    auto parallel = Parser::ParseFile(sample::largeBinaryPath(), threads, false);
    CHECK_TRUE(parallel.has_value());
    if (!parallel)
      continue;
    CHECK_EQ(static_cast<int>(serial->pixelData.size()),
             static_cast<int>(parallel->pixelData.size()));
    if (serial->pixelData.size() != parallel->pixelData.size())
      continue;
    for (size_t i = 0; i < serial->pixelData.size(); ++i)
      CHECK_PIXEL(parallel->pixelData[i], serial->pixelData[i].r,
                  serial->pixelData[i].g, serial->pixelData[i].b);
  }
}

// Independent of the serial path: a multithreaded decode must reproduce the
// known formula exactly, proving rows land at the right offsets.
TEST(ParserMT, ParallelMatchesKnownFormula) {
  auto img = Parser::ParseFile(sample::largeBinaryPath(), 8, false);
  CHECK_TRUE(img.has_value());
  if (!img)
    return;
  for (int i = 0; i < sample::kPixelCountLarge; ++i) {
    const Pixel e = sample::expectedPixel(i);
    CHECK_PIXEL(img->pixelData[i], e.r, e.g, e.b);
  }
}

// threadCount 0 is invalid input; it must clamp to 1 and still decode correctly.
TEST(ParserMT, ThreadCountZeroClampsToOne) {
  auto img = Parser::ParseFile(sample::largeBinaryPath(), 0, false);
  CHECK_TRUE(img.has_value());
  if (!img)
    return;
  CHECK_EQ(sample::kPixelCountLarge, static_cast<int>(img->pixelData.size()));
  for (int i = 0; i < sample::kPixelCountLarge; ++i) {
    const Pixel e = sample::expectedPixel(i);
    CHECK_PIXEL(img->pixelData[i], e.r, e.g, e.b);
  }
}
