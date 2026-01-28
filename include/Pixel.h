#pragma once
#include <cstdint>

// pack by one byte alignment, no padding
struct __attribute__((packed)) Pixel {
  uint8_t r, g, b;
};
// empty line for pragma pack operation
