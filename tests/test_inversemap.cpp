// Unit tests for InverseMap::ApplyInverseMap, the screen-space -> source-space
// resampler. Verifies the in-bounds/out-of-bounds (black) split and that an
// upscale routes through the bilinear sampler correctly.
#include "SampleData.h"
#include "TestFramework.h"
#include <Image.h>
#include <InverseMap.h>
#include <ViewState.h>
#include <vector>

// zoom = 1, no offset: each in-bounds screen pixel maps to its source pixel.
// The sampler guard (u < width-1, v < height-1) leaves the last column/row out
// of range, so those screen pixels are rendered black.
TEST(InverseMap, IdentityZoomCopiesInBoundsBlacksEdges) {
  Image src = sample::makeImage();
  ViewState vs;
  vs.zoom = 1.0f;
  vs.offsetX = 0.0f;
  vs.offsetY = 0.0f;

  const int w = sample::kWidth;
  const int h = sample::kHeight;
  std::vector<Pixel> screen(static_cast<size_t>(w * h));
  InverseMap::ApplyInverseMap(src, vs, screen.data(), w, h);

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const Pixel got = screen[static_cast<size_t>(y * w + x)];
      if (x < w - 1 && y < h - 1) {
        const Pixel e = sample::expectedPixel(y * w + x);
        CHECK_PIXEL(got, e.r, e.g, e.b);
      } else {
        CHECK_PIXEL(got, 0, 0, 0); // out of sampler range -> black
      }
    }
  }
}

// zoom = 2: screen (x, y) maps to source (x/2, y/2). All 16 screen pixels stay
// in range, so we get a mix of exact pixels and bilinear blends.
TEST(InverseMap, ZoomTwoUpscalesThroughBilinear) {
  Image src = sample::makeImage();
  ViewState vs;
  vs.zoom = 2.0f;
  vs.offsetX = 0.0f;
  vs.offsetY = 0.0f;

  const int w = sample::kWidth;
  const int h = sample::kHeight;
  std::vector<Pixel> screen(static_cast<size_t>(w * h));
  InverseMap::ApplyInverseMap(src, vs, screen.data(), w, h);

  CHECK_PIXEL(screen[0 * w + 0], 0, 255, 0);   // (u,v)=(0.0,0.0) -> i=0
  CHECK_PIXEL(screen[0 * w + 1], 8, 247, 4);   // (0.5,0.0) -> blend i0,i1
  CHECK_PIXEL(screen[0 * w + 2], 16, 239, 8);  // (1.0,0.0) -> i=1
  CHECK_PIXEL(screen[1 * w + 0], 32, 223, 16); // (0.0,0.5) -> blend i0,i4
}

// A tiny (< 3px) image trips the guard's width/height > 2 clause, so the whole
// screen must come back black rather than reading out of bounds.
TEST(InverseMap, DegenerateImageRendersBlack) {
  Image src;
  src.width = 2;
  src.height = 2;
  src.ppmType = "P3";
  src.maxVal = 255;
  src.pixelData = {{10, 20, 30}, {40, 50, 60}, {70, 80, 90}, {100, 110, 120}};

  ViewState vs;
  vs.zoom = 1.0f;
  vs.offsetX = 0.0f;
  vs.offsetY = 0.0f;

  const int w = 2;
  const int h = 2;
  std::vector<Pixel> screen(static_cast<size_t>(w * h));
  InverseMap::ApplyInverseMap(src, vs, screen.data(), w, h);

  for (const Pixel &p : screen) {
    CHECK_PIXEL(p, 0, 0, 0);
  }
}
