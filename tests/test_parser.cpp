// Regression tests for Parser::ParseFile against known on-disk fixtures.
// Covers both PPM variants (ASCII P3 / binary P6), cross-format agreement, and
// the failure path. A segfault or a header/pixel regression in Parser.cpp will
// surface here as a crash or a failed check.
#include "SampleData.h"
#include "TestFramework.h"
#include <Parser.h>
#include <string>

TEST(Parser, ParsesAsciiP3Header) {
  auto img = Parser::ParseFile(sample::asciiPath(), /*verbose=*/false);
  CHECK_TRUE(img.has_value());
  if (!img)
    return;
  CHECK_EQ(std::string("P3"), img->ppmType);
  CHECK_EQ(sample::kWidth, img->width);
  CHECK_EQ(sample::kHeight, img->height);
  CHECK_EQ(255, static_cast<int>(img->maxVal));
  CHECK_EQ(sample::kPixelCount, static_cast<int>(img->pixelData.size()));
}

TEST(Parser, ParsesAsciiP3Pixels) {
  auto img = Parser::ParseFile(sample::asciiPath(), false);
  CHECK_TRUE(img.has_value());
  if (!img)
    return;
  for (int i = 0; i < sample::kPixelCount; ++i) {
    const Pixel e = sample::expectedPixel(i);
    CHECK_PIXEL(img->pixelData[i], e.r, e.g, e.b);
  }
}

TEST(Parser, ParsesBinaryP6Header) {
  auto img = Parser::ParseFile(sample::binaryPath(), false);
  CHECK_TRUE(img.has_value());
  if (!img)
    return;
  CHECK_EQ(std::string("P6"), img->ppmType);
  CHECK_EQ(sample::kWidth, img->width);
  CHECK_EQ(sample::kHeight, img->height);
  CHECK_EQ(sample::kPixelCount, static_cast<int>(img->pixelData.size()));
}

TEST(Parser, ParsesBinaryP6Pixels) {
  auto img = Parser::ParseFile(sample::binaryPath(), false);
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
  auto ascii = Parser::ParseFile(sample::asciiPath(), false);
  auto binary = Parser::ParseFile(sample::binaryPath(), false);
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
  auto img = Parser::ParseFile("tests/samples/__does_not_exist__.ppm", false);
  CHECK_FALSE(img.has_value());
}
