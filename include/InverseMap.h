#pragma once
#include <Image.h>
#include <Pixel.h>
#include <ViewState.h>

class InverseMap {
public:
  InverseMap(Image source, ViewState vState, Pixel *screenBuffer,
             int screenWidth, int screenHeight);
  ~InverseMap();
};
