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
#include <string>
#include <vector>

void PrintHelp() {
  const std::string red = "\033[31m";
  const std::string reset = "\033[0m";

  std::string asciiArt = R"(

            _____                    _____                    _____                    _____          
           /\    \                  /\    \                  /\    \                  /\    \         
          /::\    \                /::\    \                /::\    \                /::\____\        
         /::::\    \              /::::\    \              /::::\    \              /::::|   |        
        /::::::\    \            /::::::\    \            /::::::\    \            /:::::|   |        
       /:::/\:::\    \          /:::/\:::\    \          /:::/\:::\    \          /::::::|   |        
      /:::/  \:::\    \        /:::/__\:::\    \        /:::/__\:::\    \        /:::/|::|   |        
     /:::/    \:::\    \      /::::\   \:::\    \      /::::\   \:::\    \      /:::/ |::|   |        
    /:::/    / \:::\    \    /::::::\   \:::\    \    /::::::\   \:::\    \    /:::/  |::|___|______  
   /:::/    /   \:::\    \  /:::/\:::\   \:::\____\  /:::/\:::\   \:::\____\  /:::/   |::::::::\    \ 
  /:::/____/     \:::\____\/:::/  \:::\   \:::|    |/:::/  \:::\   \:::|    |/:::/    |:::::::::\____\
  \:::\    \      \::/    /\::/    \:::\  /:::|____|\::/    \:::\  /:::|____|\::/    / ~~~~~/:::/    /
   \:::\    \      \/____/  \/_____/\:::\/:::/    /  \/_____/\:::\/:::/    /  \/____/      /:::/    / 
    \:::\    \                       \::::::/    /            \::::::/    /               /:::/    /  
     \:::\    \                       \::::/    /              \::::/    /               /:::/    /   
      \:::\    \                       \::/____/                \::/____/               /:::/    /    
       \:::\    \                       ~~                       ~~                    /:::/    /     
        \:::\    \                                                                    /:::/    /      
         \:::\____\                                                                  /:::/    /       
          \::/    /                                                                  \::/    /        
           \/____/                                                                    \/____/         
                                                                                                    

  )";

  std::cout << red << asciiArt << reset << "\n";
  std::cout << "Usage: ./view [path_to_ppm_file] [options]\n"
            << "Options:\n"
            << "  -h, --help     Show this help message\n"
            << "  -v, --verbose  Enable verbose output\n";
}

bool GetArgs(const std::vector<std::string> &argv, bool &verbose,
             std::string &filePath) {
  if (argv.size() < 2 || argv.size() > 3) {
    PrintHelp();
    return false;
  }

  for (int i = 1; i < argv.size(); i++) {
    std::string arg = argv[i];

    if (arg == "-v" || arg == "--verbose") {
      verbose = true;
    } else if (arg == "-h" || arg == "--help") {
      PrintHelp();
      return false;
    } else {
      filePath = arg;
    }
  }

  if (filePath.empty()) {
    std::cerr << "File on " << filePath << " is empty."
              << "\n";
    return false;
  }

  return true;
}

int main(int argc, char *argv[]) {
  bool verboseFlag = false;
  std::string filePath = "";

  std::vector<std::string> args(argv, argc + argv);

  if (!GetArgs(args, verboseFlag, filePath)) {
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

  if (auto image = Parser::ParseFile(filePath, verboseFlag)) {
    const int padding = 200;

    int maxW = std::max(1, dm.w - padding);
    int maxH = std::max(1, dm.h - padding);

    // TODO: integrate interactive scaling when launching the program through
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
