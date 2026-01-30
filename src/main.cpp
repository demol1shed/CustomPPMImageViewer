#include <PPMViewer.h>
#include <Parser.h>
#include <cstdint>
#include <iostream>
#include <vector>

int main(int argc, char *argv[]) {

  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: " << argv[0] << " <path_to_ppm_file> [-v]"
              << std::endl;
    return 1;
  }

  std::string filePath = argv[1];
  bool verbose = false;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "-v") {
      verbose = true;
    } else {
      // If it's not a flag, assume it's the filename
      filePath = arg;
    }
  }

  if (filePath.empty()) {
    std::cerr << "Usage: " << argv[0] << " <path_to_ppm_file> [-v]"
              << std::endl;
    return 1;
  }

  std::string ppmType;
  std::vector<int> widthHeight;
  uint8_t maxVal;
  std::vector<Pixel> pixelBuffer;

  Parser dataParser(filePath, ppmType, widthHeight, maxVal, pixelBuffer,
                    verbose);
  PPMViewer imageViewer(pixelBuffer, widthHeight);
  imageViewer.DrawData();
  return 0;
}
