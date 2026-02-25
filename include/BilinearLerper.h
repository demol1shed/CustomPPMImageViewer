#pragma once
#include <Image.h>
#include <Pixel.h>

class BilinearLerper {
private:
  Pixel Lerp(Pixel a, Pixel b, float d);
  int GiveFlattenedIndexOf(int w, int i, int j); // w being column count

public:
  Pixel SampleBilinear(const Image *source, float u, float v);
};
