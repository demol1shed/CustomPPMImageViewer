#pragma once
#include <cstdint>

// pack by one byte alignment, no padding
// we do this to avoid portability issues using reinterpret_cast in Parser.cpp
#pragma pack(push, 1)
struct Pixel {
  uint8_t r, g, b;
};
#pragma pack(pop)
