#pragma once
#include "Image.h"
#include "Pixel.h"
#include <cstdint> // also included in Pixel.h
#include <optional>
#include <string>
#include <vector>

class Parser {
public:
  static std::optional<Image>
  ParseFile(const std::string &filePath,
            bool verbose = false); // returns nullopt if parsing fails
};
