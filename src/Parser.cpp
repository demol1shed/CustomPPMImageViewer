#include "Pixel.h"
#include <Parser.h>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

namespace {
constexpr uint MAX_THREAD_COUNT = 64;
constexpr uint MIN_THREAD_COUNT = 1;
} // namespace

void Parser::RowsPerThread(uint threadCount, int rows, bool verbose) {
  if (threadCount < MIN_THREAD_COUNT) {
    std::cout << "Warning: Minimum usable thread count is " << MIN_THREAD_COUNT
              << "." << "\n"
              << "You've entered: " << threadCount << " threads.\n"
              << "Defaulting to 1 thread..." << "\n";
    threadCount = MIN_THREAD_COUNT;
  } else if (threadCount > std::thread::hardware_concurrency()) {
    std::cerr << "Warning: Avaliable thread count is: "
              << std::thread::hardware_concurrency() << "\n";
    std::cerr << "Defaulting to " << std::thread::hardware_concurrency()
              << " threads..." << "\n";

    threadCount = std::thread::hardware_concurrency();
  } else if (threadCount > MAX_THREAD_COUNT) {
    std::cout << "Warning: Maximum usable thread count is " << MAX_THREAD_COUNT
              << "." << "\n"
              << "You've entered: " << threadCount << " threads.\n"
              << "Defaulting to 64 threads..." << "\n";
    threadCount = MAX_THREAD_COUNT;
  }
  for (int i = 0; i < threadCount - 1; i++) {
    int startRow = (i * rows) / threadCount;
    int endRow = ((i + 1) * rows) / threadCount;
    if (verbose) {
      std::cout << "Thread ID: " << i << " rows: " << startRow << ", " << endRow
                << "\n";
      std::cout << "Thread ID: " << i
                << " has row count of: " << endRow - startRow << "\n";
    }
  }
}

std::optional<Image> Parser::ParseFile(const std::string &filePath,
                                       uint threadCount, bool verbose) {
  std::ifstream file(filePath, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "File at " << filePath << " could not be found or opened.\n";
    return std::nullopt;
  }

  Image image;
  std::string line;
  std::stringstream ss;

  auto GetNextValidLine = [&]() { // take all vars by reference
    while (std::getline(file, line)) {
      size_t commentPos = line.find("#");
      if (commentPos != std::string::npos) {
        line = line.substr(0, commentPos);
      }

      if (line.empty() ||
          line.find_first_not_of(" \t\r\n") == std::string::npos)
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
  ss >> image.imageHeader.ppmType;

  // getting width and height
  int w = -1,
      h = -1; // do this to move until a valid width/height value is found
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

  image.imageHeader.width = w;
  image.imageHeader.height = h;

  // getting maxVal
  int maxValInt = 0;
  if (ss.eof()) {
    if (!GetNextValidLine())
      return std::nullopt;
    ss.clear();
    ss.str(line);
  }

  ss >> maxValInt;
  if (maxValInt > 255) {
    std::cerr << "Error: 16 bit ppms are not supported. MaxVal is " << maxValInt
              << "\n";
    return std::nullopt;
  }
  image.imageHeader.maxVal = static_cast<uint8_t>(maxValInt);
  if (verbose) {
    std::cout << "File pre-parsing has been completed.\nPPM image type: "
              << image.imageHeader.ppmType
              << "\nImage size: " << image.imageHeader.width << ", "
              << image.imageHeader.height << "\n"
              << "Color maximum value: " << maxValInt << "\n";
  }

  RowsPerThread(MAX_THREAD_COUNT, image.imageHeader.width, verbose);

  // parsing pixel data
  size_t totalPixels = image.imageHeader.width * image.imageHeader.height;
  image.pixelData.resize(totalPixels);

  if (image.imageHeader.ppmType == "P6") {
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
