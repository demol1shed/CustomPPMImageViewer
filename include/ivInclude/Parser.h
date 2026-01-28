#pragma once
#include "Pixel.h"
#include <cstdint> // also included in Pixel.h
#include <stdint.h>
#include <string>
#include <vector>

class Parser {
private:
  bool verbose = false;
  void PreParser(std::ifstream *file, std::string &ppmTypeBuf,
                 std::vector<int> &widthHeightBuf, uint8_t &maxValBuf);
  void BodyParser(std::ifstream *file, std::vector<Pixel> &pixelBuf, int width,
                  int height);

public:
  Parser(const std::string &filePath, std::string &ppmTypeBuf,
         std::vector<int> &widthHeightBuf, uint8_t &maxValBuf,
         std::vector<Pixel> &pixelBuf, bool verbose);
  ~Parser();
};
