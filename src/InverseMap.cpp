#include <BilinearLerper.h>
#include <InverseMap.h>

void InverseMap::ApplyInverseMap(const Image &source, ViewState vState,
                                 Pixel *screenBuf, int screenWidth,
                                 int screenHeight) {
  BilinearLerper lerper;
  for (int y = 0; y < screenHeight; y++) {
    for (int x = 0; x < screenWidth; x++) {
      float u = (x - vState.offsetX) / vState.zoom;
      float v = (y - vState.offsetY) / vState.zoom;
      if (u >= 0 && u < source.imageHeader.width - 1 && v >= 0 &&
          v < source.imageHeader.height - 1 && source.imageHeader.width > 2 &&
          source.imageHeader.height > 2) {
        screenBuf[y * screenWidth + x] = lerper.SampleBilinear(&source, u, v);

      } else {
        screenBuf[y * screenWidth + x] = {0, 0, 0};
        // draw background black if out of bounds
      }
    }
  }
}
