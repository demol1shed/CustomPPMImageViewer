// Unit tests for BilinearLerper::SampleBilinear. The fixture channels are even
// numbers so a midpoint (delta = 0.5) blend lands on an exact integer, letting
// us assert precise expected pixels despite the uint8_t truncation in Lerp().
#include "SampleData.h"
#include "TestFramework.h"
#include <BilinearLerper.h>
#include <Image.h>

// At integer (u, v) the sample must reproduce the source pixel exactly, with
// u acting as the x (column) and v as the y (row) coordinate.
TEST(Bilinear, IntegerCoordinatesReturnExactPixel) {
  Image img = sample::makeImage();
  BilinearLerper lerper;
  CHECK_PIXEL(lerper.SampleBilinear(&img, 0.0f, 0.0f), 0, 255, 0);   // i = 0
  CHECK_PIXEL(lerper.SampleBilinear(&img, 1.0f, 0.0f), 16, 239, 8);  // i = 1
  CHECK_PIXEL(lerper.SampleBilinear(&img, 0.0f, 1.0f), 64, 191, 32); // i = 4
  CHECK_PIXEL(lerper.SampleBilinear(&img, 2.0f, 2.0f), 160, 95, 80); // i = 10
}

// Halfway along x blends the two horizontal neighbours i=0 and i=1.
TEST(Bilinear, HorizontalMidpointAveragesNeighbors) {
  Image img = sample::makeImage();
  BilinearLerper lerper;
  CHECK_PIXEL(lerper.SampleBilinear(&img, 0.5f, 0.0f), 8, 247, 4);
}

// Halfway along y blends the two vertical neighbours i=0 and i=4.
TEST(Bilinear, VerticalMidpointAveragesNeighbors) {
  Image img = sample::makeImage();
  BilinearLerper lerper;
  CHECK_PIXEL(lerper.SampleBilinear(&img, 0.0f, 0.5f), 32, 223, 16);
}

// The exact center blends all four neighbours i=0,1,4,5.
TEST(Bilinear, CenterBlendsFourNeighbors) {
  Image img = sample::makeImage();
  BilinearLerper lerper;
  CHECK_PIXEL(lerper.SampleBilinear(&img, 0.5f, 0.5f), 40, 215, 20);
}
