#include <cstdint>
#include <string>

struct PPMHeader {
  uint width;
  uint height;
  std::string ppmType;
  uint8_t maxVal = 255;
};
