#pragma once
#include <ViewState.h>

// Owns the interactive view (zoom + pan) of one image inside a fixed-size
// viewport and keeps it valid. Pure math: no SDL, no I/O, so every rule here is
// unit tested headlessly. The viewer translates input events into calls on
// this class and re-renders whenever a call reports a change.
//
// Coordinate convention (shared with InverseMap): a viewport pixel `x` shows
// image coordinate `u = (x - offsetX) / zoom`, so `offsetX` is the viewport
// position of the image's left edge.
class ViewController {
public:
  // Largest magnification reachable by zooming in (an explicit, larger initial
  // zoom is still honoured and becomes the limit).
  static constexpr float kMaxZoom = 64.0f;
  // Zooming out stops at this fraction of the zoom that fits the whole image
  // in the viewport, so the image can never shrink to nothing.
  static constexpr float kMinZoomFitFraction = 0.1f;

  // Dimensions are in pixels and are floored to 1. A non-positive or NaN
  // `initialZoom` falls back to the minimum zoom.
  ViewController(int imageWidth, int imageHeight, int viewportWidth,
                 int viewportHeight, float initialZoom);

  const ViewState &State() const { return state; }
  float MinZoom() const { return minZoom; }
  float MaxZoom() const { return maxZoom; }

  // Multiplies the zoom by `factor` (clamped to [MinZoom, MaxZoom]) while
  // keeping the image point under viewport position (anchorX, anchorY) fixed,
  // e.g. the mouse cursor. Returns true if the view changed.
  bool ZoomAt(float anchorX, float anchorY, float factor);

  // Moves the image by (deltaX, deltaY) viewport pixels. Returns true if the
  // view changed (false when already pinned against an edge).
  bool PanBy(float deltaX, float deltaY);

  // Restores the view the controller was constructed with. Returns true if
  // the view changed.
  bool Reset();

private:
  // Per axis: an image narrower than the viewport is centred; a wider one may
  // slide only until one of its edges meets the matching viewport edge, so
  // the user can never lose the image off-screen.
  void ClampOffsets();
  bool Apply(const ViewState &candidate);

  float imageWidth;
  float imageHeight;
  float viewportWidth;
  float viewportHeight;
  float minZoom;
  float maxZoom;
  ViewState initialState;
  ViewState state;
};
