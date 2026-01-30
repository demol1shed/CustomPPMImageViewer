#include <InverseMap.h>

InverseMap::InverseMap(Image source, ViewState vState, Pixel *screenBuf,
                       int screenWidth, int screenHeight) {
  for (int y = 0; y < screenHeight; y++) {
    for (int x = 0; x < screenWidth; x++) {
      float u = (x - vState.offsetX) / vState.zoom;
      float v = (y - vState.offsetY) / vState.zoom;
    }
  }
}

InverseMap::~InverseMap() {}
