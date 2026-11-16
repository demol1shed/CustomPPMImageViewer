// Unit tests for ViewController, the pure zoom/pan math behind the interactive
// viewer. The rules under test: zooming keeps the anchored image point fixed,
// the zoom stays inside its limits, and the image can never be panned out of
// the viewport (centred when smaller, edge-pinned when larger).
#include "TestFramework.h"
#include <ViewController.h>
#include <cmath>
#include <limits>

namespace {
constexpr int kImgW = 800;
constexpr int kImgH = 400;
constexpr int kViewW = 400;
constexpr int kViewH = 200;
constexpr float kTolerance = 1e-3f;

bool Near(float a, float b) { return std::fabs(a - b) <= kTolerance; }

// Image coordinate currently shown at viewport x (the InverseMap formula).
float ImageXAt(const ViewController &view, float x) {
  return (x - view.State().offsetX) / view.State().zoom;
}
float ImageYAt(const ViewController &view, float y) {
  return (y - view.State().offsetY) / view.State().zoom;
}

// A view zoomed in 2x over an 800x400 image: content is 1600x800.
ViewController ZoomedInView() {
  return ViewController(kImgW, kImgH, kViewW, kViewH, 2.0f);
}
} // namespace

TEST(ViewController, StartsAtRequestedZoomTopLeft) {
  ViewController view = ZoomedInView();
  CHECK_TRUE(Near(2.0f, view.State().zoom));
  CHECK_TRUE(Near(0.0f, view.State().offsetX));
  CHECK_TRUE(Near(0.0f, view.State().offsetY));
}

TEST(ViewController, ImageSmallerThanViewportIsCentred) {
  // 100x50 image at zoom 1 inside 400x200: margins of 150 and 75.
  ViewController view(100, 50, kViewW, kViewH, 1.0f);
  CHECK_TRUE(Near(150.0f, view.State().offsetX));
  CHECK_TRUE(Near(75.0f, view.State().offsetY));
  // A centred image has nowhere to go.
  CHECK_FALSE(view.PanBy(40.0f, -40.0f));
  CHECK_TRUE(Near(150.0f, view.State().offsetX));
}

TEST(ViewController, PanMovesImageAndReportsChange) {
  ViewController view = ZoomedInView();
  CHECK_TRUE(view.PanBy(-100.0f, -50.0f));
  CHECK_TRUE(Near(-100.0f, view.State().offsetX));
  CHECK_TRUE(Near(-50.0f, view.State().offsetY));
}

TEST(ViewController, PanStopsAtImageEdges) {
  ViewController view = ZoomedInView();
  // Already showing the top-left corner: cannot drag the image further in.
  CHECK_FALSE(view.PanBy(500.0f, 500.0f));
  // Far past the bottom-right: pinned at viewport - content.
  CHECK_TRUE(view.PanBy(-100000.0f, -100000.0f));
  CHECK_TRUE(Near(kViewW - 1600.0f, view.State().offsetX));
  CHECK_TRUE(Near(kViewH - 800.0f, view.State().offsetY));
  CHECK_FALSE(view.PanBy(-1.0f, -1.0f));
}

TEST(ViewController, ZoomKeepsAnchoredPointFixed) {
  ViewController view = ZoomedInView();
  view.PanBy(-300.0f, -200.0f);
  const float anchorX = 123.0f, anchorY = 77.0f;
  const float beforeU = ImageXAt(view, anchorX);
  const float beforeV = ImageYAt(view, anchorY);

  CHECK_TRUE(view.ZoomAt(anchorX, anchorY, 1.5f));
  CHECK_TRUE(Near(3.0f, view.State().zoom));
  CHECK_TRUE(Near(beforeU, ImageXAt(view, anchorX)));
  CHECK_TRUE(Near(beforeV, ImageYAt(view, anchorY)));
}

TEST(ViewController, ZoomInThenOutRoundTrips) {
  ViewController view = ZoomedInView();
  view.PanBy(-300.0f, -200.0f);
  const ViewState before = view.State();
  view.ZoomAt(200.0f, 100.0f, 1.1f);
  view.ZoomAt(200.0f, 100.0f, 1.0f / 1.1f);
  CHECK_TRUE(Near(before.zoom, view.State().zoom));
  CHECK_TRUE(Near(before.offsetX, view.State().offsetX));
  CHECK_TRUE(Near(before.offsetY, view.State().offsetY));
}

TEST(ViewController, ZoomIsClampedToLimits) {
  ViewController view = ZoomedInView();
  for (int i = 0; i < 200; ++i)
    view.ZoomAt(0.0f, 0.0f, 2.0f);
  CHECK_TRUE(Near(ViewController::kMaxZoom, view.State().zoom));
  CHECK_FALSE(view.ZoomAt(0.0f, 0.0f, 2.0f));

  for (int i = 0; i < 200; ++i)
    view.ZoomAt(0.0f, 0.0f, 0.5f);
  // Fit zoom is 0.5 (400/800), so the floor is 0.5 * kMinZoomFitFraction.
  CHECK_TRUE(Near(0.5f * ViewController::kMinZoomFitFraction,
                  view.State().zoom));
  CHECK_TRUE(Near(view.MinZoom(), view.State().zoom));
  CHECK_FALSE(view.ZoomAt(0.0f, 0.0f, 0.5f));
}

TEST(ViewController, ZoomingOutPastFitCentresImage) {
  ViewController view = ZoomedInView();
  view.PanBy(-700.0f, -300.0f);
  view.ZoomAt(0.0f, 0.0f, 0.125f); // 2.0 -> 0.25: content is 200x100
  CHECK_TRUE(Near(0.25f, view.State().zoom));
  CHECK_TRUE(Near(100.0f, view.State().offsetX));
  CHECK_TRUE(Near(50.0f, view.State().offsetY));
}

TEST(ViewController, ExplicitZoomOutsideRangeWidensLimits) {
  ViewController huge(kImgW, kImgH, kViewW, kViewH, 100.0f);
  CHECK_TRUE(Near(100.0f, huge.State().zoom));
  CHECK_TRUE(Near(100.0f, huge.MaxZoom()));

  ViewController tiny(kImgW, kImgH, kViewW, kViewH, 0.001f);
  CHECK_TRUE(Near(0.001f, tiny.State().zoom));
  CHECK_TRUE(Near(0.001f, tiny.MinZoom()));
}

TEST(ViewController, InvalidInitialZoomFallsBackToMinimum) {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  for (float bad : {0.0f, -3.0f, nan}) {
    ViewController view(kImgW, kImgH, kViewW, kViewH, bad);
    CHECK_TRUE(view.State().zoom > 0.0f);
    CHECK_TRUE(Near(view.MinZoom(), view.State().zoom));
  }
}

TEST(ViewController, InvalidZoomFactorIsIgnored) {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  ViewController view = ZoomedInView();
  CHECK_FALSE(view.ZoomAt(10.0f, 10.0f, 0.0f));
  CHECK_FALSE(view.ZoomAt(10.0f, 10.0f, -2.0f));
  CHECK_FALSE(view.ZoomAt(10.0f, 10.0f, nan));
  CHECK_TRUE(Near(2.0f, view.State().zoom));
}

TEST(ViewController, ResetRestoresInitialView) {
  ViewController view = ZoomedInView();
  CHECK_FALSE(view.Reset()); // nothing to undo yet
  view.PanBy(-250.0f, -90.0f);
  view.ZoomAt(50.0f, 60.0f, 3.0f);
  CHECK_TRUE(view.Reset());
  CHECK_TRUE(Near(2.0f, view.State().zoom));
  CHECK_TRUE(Near(0.0f, view.State().offsetX));
  CHECK_TRUE(Near(0.0f, view.State().offsetY));
}
