#include "Pixel.h"
#include <Parser.h>
#include <fstream>
#include <iostream>
#include <sstream>

void Parser::PreParser(std::ifstream *file, std::string &ppmTypeBuf,
                       std::vector<int> &widthHeightBuf, uint8_t &maxValBuf) {
  std::string line;
  std::stringstream ss;

  auto GetNextValidLine = [&]() { // take all vars by reference
    while (std::getline(*file, line)) {
      if (line.empty())
        continue;
      if (line[0] == '#')
        continue;
      return;
    }
  }; // if line is empty or is a comment, skip

  // line 1 -> ppmType
  GetNextValidLine();
  ss.clear();
  ss.str(line);
  ss >> ppmTypeBuf;

  // line 2 -> width and height
  int w = -1, h = -1;
  while (w == -1 || h == -1) { // width and height may be in different lines?
    if (ss.eof()) {
      GetNextValidLine();
      ss.clear();
      ss.str(line);
    }
    // try and read vals
    if (w == -1)
      ss >> w;
    if (h == -1)
      ss >> h;
  }

  if (widthHeightBuf.size() < 2)
    widthHeightBuf.resize(2);
  widthHeightBuf[0] = w;
  widthHeightBuf[1] = h;

  // line 3 -> max val
  int maxValInt;
  if (ss.eof()) {
    GetNextValidLine();
    ss.clear();
    ss.str(line);
  }
  ss >> maxValInt;
  maxValBuf = (uint8_t)maxValInt;
  if (verbose) {
    std::cout << "File pre-parsing has been completed.\nPPM image type: "
              << ppmTypeBuf << "\nImage size: " << widthHeightBuf[0] << ","
              << widthHeightBuf[1] << "\n"
              << "Color maximum value: " << maxValInt << "\n";
  }
}

void Parser::BodyParser(std::ifstream *file, std::vector<Pixel> &pixelBuf,
                        const std::string &ppmType, int width, int height) {
  pixelBuf.resize(width * height);
  int totalPixelCount = width * height;

  if (ppmType == "P6") { // binary data
    // what if whitespace byte after header data?
    file->get();
    file->read(reinterpret_cast<char *>(pixelBuf.data()),
               totalPixelCount *
                   sizeof(Pixel)); // reinterpret the first Pixel type to a
                                   // char* for read() to work

    if (verbose) {
      std::cout << "Raw type .ppm file found, bytes read: " << file->gcount()
                << "\n";
    }

  } else {
    int r, g, b;

    if (verbose) {
      std::cout << "ASCII type .ppm file found, read pixel count: "
                << totalPixelCount << "\n";
    }

    for (int i = 0; i < totalPixelCount; i++) {
      *file >> r >> g >> b;

      pixelBuf[i].r = r;
      pixelBuf[i].g = g;
      pixelBuf[i].b = b;
      if (verbose) {
        printf("pixelData of pixel %d: [%d, %d, %d]\n", i, pixelBuf[i].r,
               pixelBuf[i].g, pixelBuf[i].b);
      }
    }
  }

  if (verbose) {
    std::cout << "Body of the file has been loaded\nLoaded pixel count: "
              << totalPixelCount << "  pixels.\n";
  }
}

Parser::Parser(const std::string &filePath, std::string &ppmTypeBuf,
               std::vector<int> &widthHeightBuf, uint8_t &maxValBuf,
               std::vector<Pixel> &pixelBuf, bool verbose) {
  this->verbose = verbose;
  std::ifstream file(filePath, std::ios::binary);

  if (!file.is_open()) {
    std::cerr << "File at" << filePath << " could not be found or opened.\n";
    return;
  }

  PreParser(&file, ppmTypeBuf, widthHeightBuf, maxValBuf);

  BodyParser(&file, pixelBuf, ppmTypeBuf, widthHeightBuf[0], widthHeightBuf[1]);
}

Parser::~Parser() {}
