#include <ViewController.h>
#include <algorithm>

namespace {

float ClampAxisOffset(float offset, float contentSize, float viewportSize) {
  if (contentSize <= viewportSize)
    return (viewportSize - contentSize) * 0.5f;
  return std::clamp(offset, viewportSize - contentSize, 0.0f);
}

bool SameView(const ViewState &a, const ViewState &b) {
  return a.zoom == b.zoom && a.offsetX == b.offsetX && a.offsetY == b.offsetY;
}

} // namespace

ViewController::ViewController(int imageWidth, int imageHeight,
                               int viewportWidth, int viewportHeight,
                               float initialZoom)
    : imageWidth(static_cast<float>(std::max(imageWidth, 1))),
      imageHeight(static_cast<float>(std::max(imageHeight, 1))),
      viewportWidth(static_cast<float>(std::max(viewportWidth, 1))),
      viewportHeight(static_cast<float>(std::max(viewportHeight, 1))) {
  const float fitZoom = std::min(this->viewportWidth / this->imageWidth,
                                 this->viewportHeight / this->imageHeight);
  minZoom = fitZoom * kMinZoomFitFraction;
  maxZoom = kMaxZoom;

  // `!(x > 0)` is also true for NaN, which a plain `x <= 0` would let through.
  const bool usableInitialZoom = initialZoom > 0.0f;
  state.zoom = usableInitialZoom ? initialZoom : minZoom;
  // An explicit zoom outside the interactive range (e.g. `-z 100`) must still
  // be shown as requested, so it widens the range instead of being clamped.
  minZoom = std::min(minZoom, state.zoom);
  maxZoom = std::max(maxZoom, state.zoom);

  ClampOffsets();
  initialState = state;
}

bool ViewController::ZoomAt(float anchorX, float anchorY, float factor) {
  if (!(factor > 0.0f))
    return false;

  ViewState candidate = state;
  candidate.zoom = std::clamp(state.zoom * factor, minZoom, maxZoom);

  // The image coordinate under the anchor is u = (anchor - offset) / zoom.
  // Solving for the offset that maps the same u back to the anchor at the new
  // zoom gives offset' = anchor - u * zoom'.
  const float anchoredU = (anchorX - state.offsetX) / state.zoom;
  const float anchoredV = (anchorY - state.offsetY) / state.zoom;
  candidate.offsetX = anchorX - anchoredU * candidate.zoom;
  candidate.offsetY = anchorY - anchoredV * candidate.zoom;
  return Apply(candidate);
}

bool ViewController::PanBy(float deltaX, float deltaY) {
  ViewState candidate = state;
  candidate.offsetX += deltaX;
  candidate.offsetY += deltaY;
  return Apply(candidate);
}

bool ViewController::Reset() { return Apply(initialState); }

void ViewController::ClampOffsets() {
  state.offsetX =
      ClampAxisOffset(state.offsetX, imageWidth * state.zoom, viewportWidth);
  state.offsetY =
      ClampAxisOffset(state.offsetY, imageHeight * state.zoom, viewportHeight);
}

bool ViewController::Apply(const ViewState &candidate) {
  const ViewState previous = state;
  state = candidate;
  ClampOffsets();
  return !SameView(previous, state);
}
