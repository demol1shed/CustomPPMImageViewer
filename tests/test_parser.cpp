// Regression tests for Parser::ParseFile against known on-disk fixtures.
// Covers both PPM variants (ASCII P3 / binary P6), cross-format agreement, and
// the failure path. A segfault or a header/pixel regression in Parser.cpp will
// surface here as a crash or a failed check.
#include "SampleData.h"
#include "TestFramework.h"
#include <Parser.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

// A PPM written to the temp directory for one test and removed on scope exit.
struct TempPpm {
  std::string path;

  TempPpm(const std::string &name, const std::string &bytes)
      // The pid keeps parallel runs (make -j test test-asan) apart.
      : path((std::filesystem::temp_directory_path() /
              ("ppmviewer_" + std::to_string(getpid()) + "_" + name + ".ppm"))
                 .string()) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  }
  ~TempPpm() { std::remove(path.c_str()); }
  TempPpm(const TempPpm &) = delete;
  TempPpm &operator=(const TempPpm &) = delete;
};

// Parses `bytes` serially (1 thread) or through the forced-parallel path.
std::optional<Image> parseBytes(const std::string &bytes,
                                unsigned threads = 1) {
  const TempPpm file("case", bytes);
  return Parser::ParseFile(file.path, threads, false,
                           ParallelThresholds::AlwaysParallel());
}

const unsigned kSerialAndParallel[] = {1u, 2u, 3u, 4u, 8u};

// An 8-row, 1-column P3 body (enough rows for 8 workers) in which every sample
// of row i equals i, except that `first` replaces the very first sample.
std::string eightRowP3(const std::string &first = "0", int maxVal = 255) {
  std::string ppm = "P3\n1 8\n" + std::to_string(maxVal) + "\n" + first + " 0 0\n";
  for (int i = 1; i < 8; ++i) {
    const std::string v = std::to_string(i);
    ppm += v + " " + v + " " + v + "\n";
  }
  return ppm;
}

} // namespace

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
  for (unsigned threads : {2u, 3u, 4u, 8u}) {
    auto parallel = Parser::ParseFile(sample::binaryPath(), threads, false,
                                     ParallelThresholds::AlwaysParallel());
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
  for (unsigned threads : {2u, 3u, 4u, 8u}) {
    auto parallel = Parser::ParseFile(sample::asciiPath(), threads, false,
                                     ParallelThresholds::AlwaysParallel());
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

TEST(ParserHeader, NonNumericDimensionsAreRejected) {
  CHECK_FALSE(parseBytes("P3\nabc def\n255\n").has_value());
}

TEST(ParserHeader, NonPositiveDimensionsAreRejected) {
  CHECK_FALSE(parseBytes("P3\n0 2\n255\n").has_value());
  CHECK_FALSE(parseBytes("P3\n2 0\n255\n").has_value());
  CHECK_FALSE(parseBytes("P3\n-1 2\n255\n").has_value());
}

TEST(ParserHeader, UnsupportedFormatIsRejected) {
  CHECK_FALSE(parseBytes("P5\n1 1\n255\nabc").has_value());
}

TEST(ParserHeader, OutOfRangeMaxValIsRejected) {
  for (const char *maxVal : {"0", "-1", "65535"}) {
    CHECK_FALSE(
        parseBytes(std::string("P3\n1 1\n") + maxVal + "\n0 0 0\n")
            .has_value());
  }
}

// Only one separator follows maxVal: a first pixel byte that happens to be
// whitespace belongs to the image.
TEST(ParserHeader, SingleLineP6KeepsWhitespacePixelBytes) {
  for (char first : {' ', '\n'}) {
    std::string ppm = "P6 2 2 255 ";
    ppm += first;
    ppm += "ABCDEFGHIJK";
    for (unsigned threads : kSerialAndParallel) {
      auto img = parseBytes(ppm, threads);
      CHECK_TRUE(img.has_value());
      if (!img)
        continue;
      CHECK_PIXEL(img->pixelData[0], first, 'A', 'B');
      CHECK_PIXEL(img->pixelData[3], 'I', 'J', 'K');
    }
  }
}

TEST(ParserMaxVal, LowMaxValIsScaledToFullRange) {
  const std::string p3 = "P3\n2 1\n15\n0 7 8 15 15 0\n";
  std::string p6 = "P6\n2 1\n15\n";
  for (int v : {0, 7, 8, 15, 15, 0})
    p6 += static_cast<char>(v);

  for (const std::string &ppm : {p3, p6}) {
    auto img = parseBytes(ppm);
    CHECK_TRUE(img.has_value());
    if (!img)
      continue;
    CHECK_EQ(15, static_cast<int>(img->imageHeader.maxVal));
    CHECK_PIXEL(img->pixelData[0], 0, 119, 136);
    CHECK_PIXEL(img->pixelData[1], 255, 255, 0);
  }
}

TEST(ParserMaxVal, P6BytesAboveMaxValSaturate) {
  auto img = parseBytes(std::string("P6 1 1 15 ") + "\x10\x80\xff");
  CHECK_TRUE(img.has_value());
  if (img)
    CHECK_PIXEL(img->pixelData[0], 255, 255, 255);
}

// Comments inline, on their own line, containing digits, and long enough to
// straddle byte-window boundaries must not change the decoded pixels.
TEST(ParserP3Body, CommentsAreSkippedSeriallyAndInParallel) {
  std::string ppm = "P3\n1 8\n255\n";
  for (int i = 0; i < 8; ++i) {
    const std::string v = std::to_string(i);
    ppm += v + "# 9 9 9\n" + v + " " + v + " # trailing 255\n";
    if (i == 3)
      ppm += "# " + std::string(120, '7') + "\n";
  }
  ppm += "# no newline at end 1 2 3";

  for (unsigned threads : kSerialAndParallel) {
    auto img = parseBytes(ppm, threads);
    CHECK_TRUE(img.has_value());
    if (!img)
      continue;
    for (int i = 0; i < 8; ++i)
      CHECK_PIXEL(img->pixelData[i], i, i, i);
  }
}

TEST(ParserP3Body, OutOfRangeSamplesAreRejected) {
  for (unsigned threads : kSerialAndParallel) {
    CHECK_TRUE(parseBytes(eightRowP3(), threads).has_value());
    CHECK_FALSE(parseBytes(eightRowP3("-5"), threads).has_value());
    CHECK_FALSE(parseBytes(eightRowP3("300"), threads).has_value());
    CHECK_FALSE(parseBytes(eightRowP3("16", 15), threads).has_value());
  }
}
