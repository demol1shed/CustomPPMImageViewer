#include <BilinearLerper.h>
#include <InverseMap.h>

InverseMap::InverseMap(Image source, ViewState vState, Pixel *screenBuf,
                       int screenWidth, int screenHeight) {
  BilinearLerper lerper;
  for (int y = 0; y < screenHeight; y++) {
    for (int x = 0; x < screenWidth; x++) {
      float u = (x - vState.offsetX) / vState.zoom;
      float v = (y - vState.offsetY) / vState.zoom;
      if (u >= 0 && u < source.width - 1 && v >= 0 && v < source.height - 1) {
        screenBuf[y * screenWidth + x] = lerper.SampleBilinear(&source, u, v);
      } else {
        screenBuf[y * screenWidth + x] = {0, 0, 0};
        // draw background black if out of bounds
      }
    }
  }
}

InverseMap::~InverseMap() {}
