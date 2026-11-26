#pragma once
#include <Image.h>
#include <Pixel.h>

class BilinearLerper {
private:
  static Pixel Lerp(Pixel a, Pixel b, float d);
  static int GiveFlattenedIndexOf(int w, int i, int j); // w being column count

public:
  static Pixel SampleBilinear(const Image &source, float u, float v);
};
