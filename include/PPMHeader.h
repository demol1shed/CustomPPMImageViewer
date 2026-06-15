#pragma once
#include <cstdint>
#include <string>

struct PPMHeader {
  // Signed and default-initialized on purpose: `uint` is a non-portable glibc
  // typedef, and unsigned dimensions make the `width - 1` bounds check in
  // InverseMap underflow when width is 0. int matches the dimension type used
  // throughout the rest of the codebase (InverseMap, BilinearLerper, main).
  int width = 0;
  int height = 0;
  std::string ppmType;
  uint8_t maxVal = 255;
};
