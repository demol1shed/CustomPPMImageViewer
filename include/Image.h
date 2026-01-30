#include "Pixel.h"
#include <vector>

struct Image {
  int sourceWidth, sourceHeight;
  std::vector<Pixel> pixelData;
};
