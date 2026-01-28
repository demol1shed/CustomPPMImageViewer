#include <PPMViewer.h>
#include <Parser.h>
#include <SDL2/SDL_rect.h>
#include <cstdint>
#include <iostream>
#include <vector>

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define DELAYMS 5000

int main(int argc, char *argv[]) {

  if (argc < 2 && argc > 3) {
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
  PPMViewer imageViewer(
      pixelBuffer, widthHeight,
      DELAYMS); // lives on the stack, i don't leave main for now.
  // SDL_Rect pixel = (SDL_Rect){0, 0, 1, 1};
  // imageViewer.DrawPixels(&pixel, 0, 0, 0);
  imageViewer.DrawData();
  return 0;
}
