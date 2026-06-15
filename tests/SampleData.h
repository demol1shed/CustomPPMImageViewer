#pragma once
#include "Image.h"
#include "Pixel.h"
#include <string>

// Shared definition of the canonical 4x4 fixture used across the test suite.
//
// Both the on-disk samples (tests/samples/4x4.ppm and 4x4_p6.ppm) and the
// in-memory image built here follow one deterministic formula. Pixel i, laid
// out row-major (i = y * width + x), is:
//
//     r = 16 * i,   g = 255 - 16 * i,   b = 8 * i
//
// The channels are intentionally asymmetric so that a transpose, a row/column
// swap, or an off-by-one in parsing or sampling changes at least one value and
// trips a test. If you edit the formula, regenerate both sample files to match
// (see tests/README.md).
namespace sample {

constexpr int kWidth = 4;
constexpr int kHeight = 4;
constexpr int kPixelCount = kWidth * kHeight;

// A larger P6 fixture (tests/samples/32x32_p6.ppm) following the same formula,
// used to exercise multithreaded row-splitting across many thread counts —
// 32 rows divide evenly by 2/4/8/16 and unevenly by 3/5/7.
constexpr int kWidthLarge = 32;
constexpr int kHeightLarge = 32;
constexpr int kPixelCountLarge = kWidthLarge * kHeightLarge;

inline Pixel expectedPixel(int i) {
  return Pixel{static_cast<uint8_t>(16 * i),
               static_cast<uint8_t>(255 - 16 * i),
               static_cast<uint8_t>(8 * i)};
}

// In-memory twin of the on-disk fixtures, for the pure-algorithm tests that
// exercise sampling/mapping without touching the filesystem.
inline Image makeImage() {
  Image img;
  img.imageHeader.width = kWidth;
  img.imageHeader.height = kHeight;
  img.imageHeader.ppmType = "P3";
  img.imageHeader.maxVal = 255;
  img.pixelData.resize(kPixelCount);
  for (int i = 0; i < kPixelCount; ++i) {
    img.pixelData[i] = expectedPixel(i);
  }
  return img;
}

inline const std::string &asciiPath() {
  static const std::string path = "tests/samples/4x4.ppm";
  return path;
}

inline const std::string &binaryPath() {
  static const std::string path = "tests/samples/4x4_p6.ppm";
  return path;
}

inline const std::string &largeBinaryPath() {
  static const std::string path = "tests/samples/32x32_p6.ppm";
  return path;
}

// The 32x32 image in ASCII (P3), for the parallel-P3 thread sweep.
inline const std::string &asciiLargePath() {
  static const std::string path = "tests/samples/32x32.ppm";
  return path;
}

} // namespace sample
