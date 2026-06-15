#pragma once
#include "PPMHeader.h"
#include "Pixel.h"
#include <vector>

struct Image {
  PPMHeader imageHeader;
  std::vector<Pixel> pixelData;
};
