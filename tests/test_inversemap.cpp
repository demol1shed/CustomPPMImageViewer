// Unit tests for InverseMap::ApplyInverseMap, the screen-space -> source-space
// resampler. Verifies the in-bounds/out-of-bounds (black) split and that an
// upscale routes through the bilinear sampler correctly.
#include "SampleData.h"
#include "TestFramework.h"
#include <Image.h>
#include <InverseMap.h>
#include <ViewState.h>
#include <vector>

// zoom = 1, no offset: every screen pixel maps to its source pixel, including
// the last column and row.
TEST(InverseMap, IdentityZoomCopiesWholeImage) {
  Image src = sample::makeImage();
  ViewState vs;
  vs.zoom = 1.0f;
  vs.offsetX = 0.0f;
  vs.offsetY = 0.0f;

  const int w = sample::kWidth;
  const int h = sample::kHeight;
  std::vector<Pixel> screen(static_cast<size_t>(w * h));
  InverseMap::ApplyInverseMap(src, vs, screen.data(), w, h);

  for (int i = 0; i < w * h; ++i) {
    const Pixel e = sample::expectedPixel(i);
    CHECK_PIXEL(screen[static_cast<size_t>(i)], e.r, e.g, e.b);
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

// Images smaller than 3px are valid and must be drawn, not blacked out.
TEST(InverseMap, TwoByTwoImageRendersFully) {
  Image src;
  src.imageHeader.width = 2;
  src.imageHeader.height = 2;
  src.imageHeader.ppmType = "P3";
  src.imageHeader.maxVal = 255;
  src.pixelData = {{10, 20, 30}, {40, 50, 60}, {70, 80, 90}, {100, 110, 120}};

  ViewState vs;
  vs.zoom = 1.0f;
  vs.offsetX = 0.0f;
  vs.offsetY = 0.0f;

  const int w = 2;
  const int h = 2;
  std::vector<Pixel> screen(static_cast<size_t>(w * h));
  InverseMap::ApplyInverseMap(src, vs, screen.data(), w, h);

  for (size_t i = 0; i < screen.size(); ++i) {
    const Pixel e = src.pixelData[i];
    CHECK_PIXEL(screen[i], e.r, e.g, e.b);
  }
}

TEST(InverseMap, OneByOneImageFillsItsScaledArea) {
  Image src;
  src.imageHeader.width = 1;
  src.imageHeader.height = 1;
  src.pixelData = {{10, 20, 30}};

  for (int zoom : {1, 3}) {
    ViewState vs;
    vs.zoom = static_cast<float>(zoom);
    vs.offsetX = 0.0f;
    vs.offsetY = 0.0f;

    std::vector<Pixel> screen(static_cast<size_t>(zoom * zoom));
    InverseMap::ApplyInverseMap(src, vs, screen.data(), zoom, zoom);

    for (const Pixel &p : screen) {
      CHECK_PIXEL(p, 10, 20, 30);
    }
  }
}

TEST(InverseMap, EmptyImageRendersBlack) {
  const Image src; // 0x0, no pixel data
  ViewState vs;
  vs.zoom = 1.0f;
  vs.offsetX = 0.0f;
  vs.offsetY = 0.0f;

  // Sentinel pre-fill so an unwritten pixel cannot pass as black.
  std::vector<Pixel> screen(4, Pixel{1, 2, 3});
  InverseMap::ApplyInverseMap(src, vs, screen.data(), 2, 2);

  for (const Pixel &p : screen) {
    CHECK_PIXEL(p, 0, 0, 0);
  }
}

// The threaded resampler splits the screen rows into bands; whatever the
// worker count (including more workers than the frame can use), the result
// must be byte-identical to the single-threaded render.
TEST(InverseMap, ThreadedRenderMatchesSerial) {
  Image src = sample::makeImage();
  ViewState vs;
  vs.zoom = 37.5f;
  vs.offsetX = -11.0f;
  vs.offsetY = 9.0f; // leaves a black band at the top to cover that branch

  const int w = 97;
  const int h = 203; // not divisible by the worker counts below
  std::vector<Pixel> serial(static_cast<size_t>(w * h));
  InverseMap::ApplyInverseMap(src, vs, serial.data(), w, h, 1);

  for (unsigned workers : {2u, 3u, 5u, 16u, 1000u}) {
    // Pre-fill with a sentinel so an unwritten band cannot pass by accident.
    std::vector<Pixel> threaded(static_cast<size_t>(w * h), Pixel{1, 2, 3});
    InverseMap::ApplyInverseMap(src, vs, threaded.data(), w, h, workers);
    bool identical = true;
    for (size_t i = 0; i < serial.size(); ++i) {
      identical = identical && serial[i].r == threaded[i].r &&
                  serial[i].g == threaded[i].g && serial[i].b == threaded[i].b;
    }
    CHECK_TRUE(identical);
  }
}
