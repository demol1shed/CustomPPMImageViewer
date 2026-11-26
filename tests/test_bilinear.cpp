// Unit tests for BilinearLerper::SampleBilinear. The fixture channels are even
// numbers so a midpoint (delta = 0.5) blend lands on an exact integer, letting
// us assert precise expected pixels independently of how Lerp() rounds.
#include "SampleData.h"
#include "TestFramework.h"
#include <BilinearLerper.h>
#include <Image.h>

// At integer (u, v) the sample must reproduce the source pixel exactly, with
// u acting as the x (column) and v as the y (row) coordinate.
TEST(Bilinear, IntegerCoordinatesReturnExactPixel) {
  Image img = sample::makeImage();
  CHECK_PIXEL(BilinearLerper::SampleBilinear(img, 0.0f, 0.0f),
              0, 255, 0); // i = 0
  CHECK_PIXEL(BilinearLerper::SampleBilinear(img, 1.0f, 0.0f),
              16, 239, 8);  // i = 1
  CHECK_PIXEL(BilinearLerper::SampleBilinear(img, 0.0f, 1.0f),
              64, 191, 32); // i = 4
  CHECK_PIXEL(BilinearLerper::SampleBilinear(img, 2.0f, 2.0f),
              160, 95, 80); // i = 10
}

// Halfway along x blends the two horizontal neighbours i=0 and i=1.
TEST(Bilinear, HorizontalMidpointAveragesNeighbors) {
  Image img = sample::makeImage();
  CHECK_PIXEL(BilinearLerper::SampleBilinear(img, 0.5f, 0.0f), 8, 247, 4);
}

// Halfway along y blends the two vertical neighbours i=0 and i=4.
TEST(Bilinear, VerticalMidpointAveragesNeighbors) {
  Image img = sample::makeImage();
  CHECK_PIXEL(BilinearLerper::SampleBilinear(img, 0.0f, 0.5f), 32, 223, 16);
}

// The exact center blends all four neighbours i=0,1,4,5.
TEST(Bilinear, CenterBlendsFourNeighbors) {
  Image img = sample::makeImage();
  CHECK_PIXEL(BilinearLerper::SampleBilinear(img, 0.5f, 0.5f), 40, 215, 20);
}

// The last column/row has no right/bottom neighbour: it samples itself.
TEST(Bilinear, LastColumnAndRowReturnEdgePixel) {
  Image img = sample::makeImage();
  CHECK_PIXEL(BilinearLerper::SampleBilinear(img, 3.0f, 0.0f),
              48, 207, 24); // i = 3
  CHECK_PIXEL(BilinearLerper::SampleBilinear(img, 0.0f, 3.0f),
              192, 63, 96); // i = 12
  CHECK_PIXEL(BilinearLerper::SampleBilinear(img, 3.0f, 3.0f),
              240, 15, 120);  // i = 15
}

TEST(Bilinear, OutOfRangeCoordinatesClampToEdge) {
  Image img = sample::makeImage();
  CHECK_PIXEL(BilinearLerper::SampleBilinear(img, 3.5f, 3.5f),
              240, 15, 120);  // i = 15
  CHECK_PIXEL(BilinearLerper::SampleBilinear(img, 6.3f, 0.0f),
              48, 207, 24); // i = 3
  CHECK_PIXEL(BilinearLerper::SampleBilinear(img, 0.0f, 6.3f),
              192, 63, 96); // i = 12
  CHECK_PIXEL(BilinearLerper::SampleBilinear(img, -0.7f, -0.7f),
              0, 255, 0); // i = 0
}

TEST(Bilinear, OneByOneImageAlwaysReturnsItsPixel) {
  Image img;
  img.imageHeader.width = 1;
  img.imageHeader.height = 1;
  img.pixelData = {{10, 20, 30}};
  for (float c : {-2.0f, 0.0f, 0.5f, 0.99f, 5.0f}) {
    CHECK_PIXEL(BilinearLerper::SampleBilinear(img, c, c), 10, 20, 30);
  }
}

// 127.5 rounds to 128 (truncation would give 127).
TEST(Bilinear, LerpRoundsToNearest) {
  Image img;
  img.imageHeader.width = 2;
  img.imageHeader.height = 1;
  img.pixelData = {{0, 0, 0}, {255, 255, 255}};
  CHECK_PIXEL(BilinearLerper::SampleBilinear(img, 0.5f, 0.0f), 128, 128, 128);
}
