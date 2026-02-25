#include "Pixel.h"
#include <InverseMap.h>
#include <PPMViewer.h>
#include <Parser.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <ViewState.h>
#include <algorithm>
#include <iostream>
#include <ostream>
#include <vector>

int main(int argc, char *argv[]) {

  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: " << argv[0] << " <path_to_ppm_file> [-v]"
              << std::endl;
    return 1;
  }

  std::string filePath = "";
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

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "Could not initialize SDL to get screen size" << std::endl;
    return 1;
  }

  SDL_DisplayMode dm;
  if (SDL_GetDesktopDisplayMode(0, &dm) != 0) {
    std::cerr << "Could not get display mode" << std::endl;
    return 1;
  }

  if (auto image = Parser::ParseFile(filePath, verbose)) {
    const int padding = 200;

    int maxW = std::max(1, dm.w - padding);
    int maxH = std::max(1, dm.h - padding);

    float scaleX = (float)maxW / image->width;
    float scaleY = (float)maxH / image->height;
    float idealZoom = std::min({scaleX, scaleY, 1.0f});

    int windowWidth = (int)(image->width * idealZoom);
    int windowHeight = (int)(image->height * idealZoom);

    ViewState vState;
    vState.zoom = idealZoom;
    vState.offsetY = 0.0f;
    vState.offsetX = 0.0f;

    Image sourceImage;
    sourceImage.width = image->width;
    sourceImage.height = image->height;
    sourceImage.pixelData = image->pixelData;

    // allocate processed buffer for the new image
    std::vector<Pixel> processedBuffer(windowWidth * windowHeight);
    InverseMap::ApplyInverseMap(sourceImage, vState, processedBuffer.data(),
                                windowWidth, windowHeight);

    PPMViewer imageViewer(windowWidth, windowHeight);
    imageViewer.DrawData(processedBuffer);
  }

  return 0;
}
