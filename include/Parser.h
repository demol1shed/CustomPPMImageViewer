#pragma once
#include "Pixel.h"
#include <cstdint> // also included in Pixel.h
#include <optional>
#include <string>
#include <vector>

struct RawImage {
  int width = 0;
  int height = 0;
  std::string ppmType;
  uint8_t maxVal = 255;
  std::vector<Pixel> pixelData;
};

class Parser {
public:
  static std::optional<RawImage>
  ParseFile(const std::string &filePath,
            bool verbose = false); // returns nullopt if parsing fails
};
