#pragma once
#include "Image.h"
#include <optional>
#include <string>

class Parser {
public:
  static void RowsPerThread(uint threadCount, int rows, bool verbose = false);
  static std::optional<Image>
  ParseFile(const std::string &filePath, uint threadCount,
            bool verbose = false); // returns nullopt if parsing fails
};
