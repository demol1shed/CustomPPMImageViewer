// Regression tests for Parser::ParseFile against known on-disk fixtures.
// Covers both PPM variants (ASCII P3 / binary P6), cross-format agreement, and
// the failure path. A segfault or a header/pixel regression in Parser.cpp will
// surface here as a crash or a failed check.
#include "SampleData.h"
#include "TestFramework.h"
#include <Parser.h>
#include <string>

TEST(Parser, ParsesAsciiP3Header) {
  auto img = Parser::ParseFile(sample::asciiPath(), /*threadCount=*/1,
                               /*verbose=*/false);
  CHECK_TRUE(img.has_value());
  if (!img)
    return;
  CHECK_EQ(std::string("P3"), img->imageHeader.ppmType);
  CHECK_EQ(sample::kWidth, img->imageHeader.width);
  CHECK_EQ(sample::kHeight, img->imageHeader.height);
  CHECK_EQ(255, static_cast<int>(img->imageHeader.maxVal));
  CHECK_EQ(sample::kPixelCount, static_cast<int>(img->pixelData.size()));
}

TEST(Parser, ParsesAsciiP3Pixels) {
  auto img = Parser::ParseFile(sample::asciiPath(), /*threadCount=*/1, false);
  CHECK_TRUE(img.has_value());
  if (!img)
    return;
  for (int i = 0; i < sample::kPixelCount; ++i) {
    const Pixel e = sample::expectedPixel(i);
    CHECK_PIXEL(img->pixelData[i], e.r, e.g, e.b);
  }
}

TEST(Parser, ParsesBinaryP6Header) {
  auto img = Parser::ParseFile(sample::binaryPath(), /*threadCount=*/1, false);
  CHECK_TRUE(img.has_value());
  if (!img)
    return;
  CHECK_EQ(std::string("P6"), img->imageHeader.ppmType);
  CHECK_EQ(sample::kWidth, img->imageHeader.width);
  CHECK_EQ(sample::kHeight, img->imageHeader.height);
  CHECK_EQ(sample::kPixelCount, static_cast<int>(img->pixelData.size()));
}

TEST(Parser, ParsesBinaryP6Pixels) {
  auto img = Parser::ParseFile(sample::binaryPath(), /*threadCount=*/1, false);
  CHECK_TRUE(img.has_value());
  if (!img)
    return;
  for (int i = 0; i < sample::kPixelCount; ++i) {
    const Pixel e = sample::expectedPixel(i);
    CHECK_PIXEL(img->pixelData[i], e.r, e.g, e.b);
  }
}

// The two encodings describe the same image, so their decoded buffers must be
// byte-for-byte identical.
TEST(Parser, AsciiAndBinaryDecodeToSamePixels) {
  auto ascii = Parser::ParseFile(sample::asciiPath(), /*threadCount=*/1, false);
  auto binary = Parser::ParseFile(sample::binaryPath(), /*threadCount=*/1, false);
  CHECK_TRUE(ascii.has_value());
  CHECK_TRUE(binary.has_value());
  if (!ascii || !binary)
    return;
  CHECK_EQ(static_cast<int>(ascii->pixelData.size()),
           static_cast<int>(binary->pixelData.size()));
  const size_t n = ascii->pixelData.size();
  for (size_t i = 0; i < n; ++i) {
    CHECK_PIXEL(binary->pixelData[i], static_cast<int>(ascii->pixelData[i].r),
                static_cast<int>(ascii->pixelData[i].g),
                static_cast<int>(ascii->pixelData[i].b));
  }
}

TEST(Parser, MissingFileReturnsNullopt) {
  auto img = Parser::ParseFile("tests/samples/__does_not_exist__.ppm",
                               /*threadCount=*/1, false);
  CHECK_FALSE(img.has_value());
}

// Multithreaded P6 decode must be byte-identical to the single-threaded read.
// The 4x4 sample has 4 rows, so -t 2/3/4 genuinely splits work across workers
// (3 is a non-divisible split). A broader sweep on a larger fixture lands in a
// follow-up step.
TEST(Parser, ParallelP6MatchesSerial) {
  auto serial = Parser::ParseFile(sample::binaryPath(), /*threadCount=*/1, false);
  CHECK_TRUE(serial.has_value());
  if (!serial)
    return;
  for (uint threads : {2u, 3u, 4u, 8u}) {
    auto parallel = Parser::ParseFile(sample::binaryPath(), threads, false);
    CHECK_TRUE(parallel.has_value());
    if (!parallel)
      continue;
    CHECK_EQ(static_cast<int>(serial->pixelData.size()),
             static_cast<int>(parallel->pixelData.size()));
    for (size_t i = 0; i < serial->pixelData.size(); ++i)
      CHECK_PIXEL(parallel->pixelData[i], serial->pixelData[i].r,
                  serial->pixelData[i].g, serial->pixelData[i].b);
  }
}

// Same guarantee for the ASCII (P3) byte-range parallel decode: multithreaded
// must be byte-identical to serial. The 4x4 P3 sample (with comments + varied
// whitespace) exercises the token-boundary resync on real parsing; a broader
// sweep on a larger fixture lands in a follow-up step.
TEST(Parser, ParallelP3MatchesSerial) {
  auto serial = Parser::ParseFile(sample::asciiPath(), /*threadCount=*/1, false);
  CHECK_TRUE(serial.has_value());
  if (!serial)
    return;
  for (uint threads : {2u, 3u, 4u, 8u}) {
    auto parallel = Parser::ParseFile(sample::asciiPath(), threads, false);
    CHECK_TRUE(parallel.has_value());
    if (!parallel)
      continue;
    CHECK_EQ(static_cast<int>(serial->pixelData.size()),
             static_cast<int>(parallel->pixelData.size()));
    for (size_t i = 0; i < serial->pixelData.size(); ++i)
      CHECK_PIXEL(parallel->pixelData[i], serial->pixelData[i].r,
                  serial->pixelData[i].g, serial->pixelData[i].b);
  }
}
