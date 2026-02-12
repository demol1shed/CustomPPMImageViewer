#pragma once
#include <cstdint>

// pack by one byte alignment, no padding
#pragma pack(push, 1)
struct Pixel {
  uint8_t r, g, b;
};
#pragma pack(pop)
