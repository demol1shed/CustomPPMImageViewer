#pragma once
#include "Pixel.h"
#include <cstdint>
#include <string>
#include <vector>

struct Image {
  int width = 0;
  int height = 0;
  std::string ppmType;
  uint16_t maxVal = 255;
  std::vector<Pixel> pixelData;
};
