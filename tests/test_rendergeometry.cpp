// Unit tests for ComputeWindowSize, the clamp that keeps the render buffer the
// same size as the SDL texture (so texture pitch == buffer row stride). This
// dimension test is the headless regression guard for the diagonal-shear bug:
// the shear itself can't be observed under the dummy video driver, but it only
// happened when the zoomed width exceeded the display, which is exactly what
// this clamp prevents.
#include "TestFramework.h"
#include <RenderGeometry.h>

// Reference image and display used throughout (the real test.ppm is 740x463,
// the display where -z 2.6 first sheared is 1920x1080).
namespace {
constexpr int kImgW = 740;
constexpr int kImgH = 463;
constexpr int kDispW = 1920;
constexpr int kDispH = 1080;
} // namespace

TEST(RenderGeometry, BothAxesFitNoClamp) {
  WinSize ws = ComputeWindowSize(kImgW, kImgH, 2.0f, kDispW, kDispH);
  CHECK_EQ(1480, ws.w); // 740 * 2.0
  CHECK_EQ(926, ws.h);  // 463 * 2.0
}

TEST(RenderGeometry, HeightOnlyClamp) {
  // 740*2.5 = 1850 (<= 1920, fits); 463*2.5 = 1157.5 -> 1157 (> 1080, clamps).
  WinSize ws = ComputeWindowSize(kImgW, kImgH, 2.5f, kDispW, kDispH);
  CHECK_EQ(1850, ws.w);
  CHECK_EQ(1080, ws.h);
}

TEST(RenderGeometry, WidthThresholdClamps) {
  // 740*2.6 = 1924 > 1920 -> the case that used to shear; both axes clamp.
  WinSize ws = ComputeWindowSize(kImgW, kImgH, 2.6f, kDispW, kDispH);
  CHECK_EQ(1920, ws.w);
  CHECK_EQ(1080, ws.h);
}

TEST(RenderGeometry, IdentityZoomFits) {
  WinSize ws = ComputeWindowSize(kImgW, kImgH, 1.0f, kDispW, kDispH);
  CHECK_EQ(kImgW, ws.w);
  CHECK_EQ(kImgH, ws.h);
}

TEST(RenderGeometry, NeverExceedsDisplay) {
  const float zooms[] = {0.5f, 1.0f, 2.5f, 2.6f, 10.0f, 1000.0f};
  for (float z : zooms) {
    WinSize ws = ComputeWindowSize(kImgW, kImgH, z, kDispW, kDispH);
    CHECK_TRUE(ws.w <= kDispW && ws.h <= kDispH);
    CHECK_TRUE(ws.w >= 1 && ws.h >= 1);
  }
}

TEST(RenderGeometry, FloorsNotRounds) {
  // Large display so nothing clamps: 463*2.5 = 1157.5 must floor to 1157.
  WinSize ws = ComputeWindowSize(kImgW, kImgH, 2.5f, 99999, 99999);
  CHECK_EQ(1850, ws.w);
  CHECK_EQ(1157, ws.h);
}

TEST(RenderGeometry, HugeZoomDoesNotOverflow) {
  WinSize ws = ComputeWindowSize(kImgW, kImgH, 1e9f, kDispW, kDispH);
  CHECK_EQ(kDispW, ws.w);
  CHECK_EQ(kDispH, ws.h);
}

TEST(RenderGeometry, DegenerateZoomFloorsToOne) {
  WinSize ws = ComputeWindowSize(kImgW, kImgH, 0.0f, kDispW, kDispH);
  CHECK_EQ(1, ws.w);
  CHECK_EQ(1, ws.h);
}

TEST(RenderGeometry, SmallDisplayAtZoomOne) {
  // Image wider than the display even at 1.0 -> width clamps, height fits.
  WinSize ws = ComputeWindowSize(kImgW, kImgH, 1.0f, 640, 480);
  CHECK_EQ(640, ws.w);
  CHECK_EQ(463, ws.h);
}
