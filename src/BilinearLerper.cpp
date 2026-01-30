#include "Pixel.h"
#include <BilinearLerper.h>
#include <cmath>
#include <vector>

BilinearLerper::BilinearLerper() {}

Pixel BilinearLerper::Lerp(Pixel a, Pixel b, float d) {
  Pixel result;
  result.r = d * (b.r - a.r) + a.r;
  result.g = d * (b.g - a.g) + a.g;
  result.b = d * (b.b - a.b) + a.b;

  return result;
}

int BilinearLerper::GiveFlattenedIndexOf(int d, int i, int j) {
  return (i * d) + j;
}

Pixel BilinearLerper::SampleBilinear(Image *source, float u, float v) {
  int x = std::floor(u);
  int y = std::floor(v);

  int deltaX = u - x;
  int deltaY = v - y;

  Pixel topLeft = source->pixelData[GiveFlattenedIndexOf(
      source->sourceWidth, y,
      x)]; // i represents the y coordinate, j represents the x coordinate
  Pixel topRight =
      source->pixelData[GiveFlattenedIndexOf(source->sourceWidth, y, x + 1)];
  Pixel bottomLeft =
      source->pixelData[GiveFlattenedIndexOf(source->sourceWidth, y + 1, x)];
  Pixel bottomRight =
      source
          ->pixelData[GiveFlattenedIndexOf(source->sourceWidth, y + 1, x + 1)];

  // formula: delta * (val2 - val1) + val1
  Pixel topColor = Lerp(topLeft, topRight, deltaX);
  Pixel bottomColor = Lerp(bottomLeft, bottomRight, deltaY);

  // final vertical interpolation
  Pixel finalColor = Lerp(topColor, bottomColor, deltaY);

  return finalColor;
}
