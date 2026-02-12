#include "Pixel.h"
#include <Parser.h>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
std::optional<RawImage> Parser::ParseFile(const std::string &filePath,
                                          bool verbose) {
  std::ifstream file(filePath, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "File at" << filePath << " could not be found or opened.\n";
    return std::nullopt;
  }

  RawImage image;
  std::string line;
  std::stringstream ss;
  auto GetNextValidLine = [&]() { // take all vars by reference
    while (std::getline(file, line)) {
      if (line.empty())
        continue;
      if (line[0] == '#')
        continue;
      return true;
    }
    return false;
  };

  // getting ppm type
  if (!GetNextValidLine())
    return std::nullopt;
  ss.clear();
  ss.str(line);
  ss >> image.ppmType;

  // getting width and height
  int w = -1, h = -1;
  while (w == -1 || h == -1) {
    if (ss.eof()) {
      if (!GetNextValidLine())
        return std::nullopt;
      ss.clear();
      ss.str(line);
    }
    if (w == -1)
      ss >> w;
    // width and height may be in differnt lines
    if (ss.eof() && h == -1) {
      if (!GetNextValidLine())
        return std::nullopt;
      ss.clear();
      ss.str(line);
    }
    if (h == -1)
      ss >> h;
  }

  image.width = w;
  image.height = h;

  // getting maxVal
  int maxValInt = 0;
  if (ss.eof()) {
    if (!GetNextValidLine())
      return std::nullopt;
    ss.clear();
    ss.str(line);
  }
  ss >> maxValInt;
  image.maxVal = static_cast<uint8_t>(maxValInt);
  if (verbose) {
    std::cout << "File pre-parsing has been completed.\nPPM image type: "
              << image.ppmType << "\nImage size: " << image.width << ","
              << image.height << "\n"
              << "Color maximum value: " << image.maxVal << "\n";
  }

  // parsing pixel data
  size_t totalPixels = image.width * image.height;
  image.pixelData.resize(totalPixels);

  if (image.ppmType == "P6") {
    // raw ppm
    file.read(reinterpret_cast<char *>(image.pixelData.data()),
              totalPixels * sizeof(Pixel));

    if (verbose) {
      std::cout << "Binary P6 data read. Bytes: " << file.gcount() << "\n";
    }
  } else {
    // ascii mode
    int r, g, b;
    for (size_t i = 0; i < totalPixels; i++) {
      file >> r >> g >> b;
      image.pixelData[i] = {static_cast<uint8_t>(r), static_cast<uint8_t>(g),
                            static_cast<uint8_t>(b)};
    }

    if (verbose) {
      std::cout << "ASCII P3 data read.\n";
    }
  }

  return image;
}
