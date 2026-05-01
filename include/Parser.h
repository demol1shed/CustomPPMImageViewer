#pragma once
#include "Image.h"
#include <optional>
#include <string>

class Parser {
public:
  static std::optional<Image>
  ParseFile(const std::string &filePath,
            bool verbose = false); // returns nullopt if parsing fails
};
