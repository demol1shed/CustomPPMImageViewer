#include "Pixel.h"
#include <BilinearLerper.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

uint8_t LerpChannel(uint8_t a, uint8_t b, float d) {
  const float value = static_cast<float>(a) +
                      d * (static_cast<float>(b) - static_cast<float>(a));
  const long rounded = std::lround(value);
  return static_cast<uint8_t>(std::clamp(rounded, 0L, 255L));
}

} // namespace

Pixel BilinearLerper::Lerp(Pixel a, Pixel b, float d) {
  return Pixel{
      LerpChannel(a.r, b.r, d),
      LerpChannel(a.g, b.g, d),
      LerpChannel(a.b, b.b, d),
  };
}

int BilinearLerper::GiveFlattenedIndexOf(int w, int i, int j) {
  // w = row width (column count); i = row (y), j = column (x).
  return (i * w) + j;
}

Pixel BilinearLerper::SampleBilinear(const Image &source, float u, float v) {
  const int width = source.imageHeader.width;
  const int height = source.imageHeader.height;
  if (width <= 0 || height <= 0 || source.pixelData.empty()) {
    return {0, 0, 0};
  }

  const int maxX = width - 1;
  const int maxY = height - 1;

  const int x0 = std::clamp(static_cast<int>(std::floor(u)), 0, maxX);
  const int y0 = std::clamp(static_cast<int>(std::floor(v)), 0, maxY);
  const int x1 = std::min(x0 + 1, maxX);
  const int y1 = std::min(y0 + 1, maxY);

  const float deltaX = std::clamp(u - static_cast<float>(x0), 0.0f, 1.0f);
  const float deltaY = std::clamp(v - static_cast<float>(y0), 0.0f, 1.0f);

  const Pixel topLeft = source.pixelData[GiveFlattenedIndexOf(width, y0, x0)];
  const Pixel topRight = source.pixelData[GiveFlattenedIndexOf(width, y0, x1)];
  const Pixel bottomLeft =
      source.pixelData[GiveFlattenedIndexOf(width, y1, x0)];
  const Pixel bottomRight =
      source.pixelData[GiveFlattenedIndexOf(width, y1, x1)];

  const Pixel topColor = Lerp(topLeft, topRight, deltaX);
  const Pixel bottomColor = Lerp(bottomLeft, bottomRight, deltaX);
  return Lerp(topColor, bottomColor, deltaY);
}
