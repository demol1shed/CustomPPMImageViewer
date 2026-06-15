#pragma once
#include <algorithm>

// Window/buffer dimensions, in pixels.
struct WinSize {
  int w;
  int h;
};

// Computes the final on-screen window size for an image shown at `zoom`,
// clamped to the display. The viewer renders a buffer of exactly this size and
// hands it to a same-sized SDL texture, so the upload pitch always equals the
// buffer's row stride. (When the buffer was sized to the *unclamped* zoomed
// width while the texture was clamped to the desktop, the pitch underran the
// stride and every row drifted sideways -> diagonal shear.)
//
// The comparison is done in float before the cast so an absurd `-z` value can't
// overflow int, and each axis is floored to >= 1 so SDL never receives a zero
// or negative texture size. With zoom beyond the fit point the window saturates
// at the display size and shows the top-left region of the zoomed image (a
// viewport/crop; panning is a separate roadmap item).
inline WinSize ComputeWindowSize(int imageW, int imageH, float zoom, int dispW,
                                 int dispH) {
  float wf = imageW * zoom;
  float hf = imageH * zoom;
  int w = wf >= static_cast<float>(dispW) ? dispW : static_cast<int>(wf);
  int h = hf >= static_cast<float>(dispH) ? dispH : static_cast<int>(hf);
  return {std::max(w, 1), std::max(h, 1)};
}
