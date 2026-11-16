#pragma once
#include <Image.h>
#include <Pixel.h>
#include <ViewState.h>

class InverseMap {
public:
  // Fills `screenBuffer` (screenWidth * screenHeight pixels) by mapping every
  // screen pixel back to the source image through `vState`; pixels that fall
  // outside the image are black.
  //
  // `workerCount` is an upper bound on the threads used: the screen rows are
  // split into contiguous bands, one per worker, and small frames use fewer
  // workers (or just the calling thread). Interactive redraws depend on this:
  // a full-screen resample is far too slow for dragging on a single thread.
  static void ApplyInverseMap(const Image &source, ViewState vState,
                              Pixel *screenBuffer, int screenWidth,
                              int screenHeight, unsigned workerCount = 1);

private:
  // Resamples screen rows [rowBegin, rowEnd) only.
  static void RenderRows(const Image &source, ViewState vState,
                         Pixel *screenBuffer, int screenWidth, int rowBegin,
                         int rowEnd);
};
