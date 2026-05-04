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
#include <stdexcept>
#include <string>
#include <vector>

constexpr float DEFAULTZOOM = 1.0f;
constexpr int DEFAULTPADDING = 200;
constexpr int DEFAULTARGCOUNT = 7;
constexpr uint DEFAULTTHREADCOUNT = 1;

void PrintHelp() {
  const std::string red = "\033[31m";
  const std::string reset = "\033[0m";
  const std::string bold = "\033[1m";

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

  std::cout << red << asciiArt << reset << "\n\n";
  std::cout
      << bold << "Usage: " << reset << "./view [path_to_ppm_file] [options]\n"
      << bold << "Options: " << reset << "\n"
      << "  -h,         --help                  Show this help message\n"
      << "  -v,         --verbose               Enable verbose output\n"
      << "  -z <float>, --zoom <float>          Set the initial zoom "
         "level (e.g., 2.0)\n"
      << "  -t  <uint>, --threads <uint>        Set thread count (e.g., 32)\n";
}

bool GetArgs(const std::vector<std::string> &argv, bool &verbose,
             std::string &filePath, float &zoomAmount, uint &threadCount) {
  if (argv.size() > DEFAULTARGCOUNT) {
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
    } else if (arg == "-z" || arg == "--zoom") {
      if (i + 1 < argv.size()) {
        std::string val = argv[i + 1];
        try {
          zoomAmount = std::stof(val);
        } catch (const std::invalid_argument &e) {
          std::cerr << "Error: Invalid zoom value. " << val
                    << " is not a number" << "\n";
          return false;
        } catch (const std::out_of_range &e) {
          std::cerr << "Error: Zoom value too large" << "\n";
          return false;
        }

        i++;
      } else {
        std::cerr << "Error: Zoom requires a numeric value" << "\n";
        return false;
      }
    } else if (arg == "-t" || arg == "--threads") {
      if (i + 1 < argv.size()) {
        std::string tVal = argv[i + 1];
        try {
          threadCount = std::stoi(tVal);
        } catch (const std::invalid_argument &e) {
          std::cerr << "Error: Invalid thread count value. " << "\n";
          std::cerr << tVal << " is not a number" << "\n";
          return false;
        } catch (const std::out_of_range &e) {
          std::cerr << "Error: Thread count too large" << "\n";
          return false;
        }
        i++;
      } else {
        std::cerr
            << "Error: Thread count requires a numeric value between 1 to 64"
            << "\n";
        return false;
      }
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

// after parsing
void ProcessImage(std::optional<Image> &image, const int padding,
                  const SDL_DisplayMode &dm) {
  int maxW = std::max(1, dm.w - padding);
  int maxH = std::max(1, dm.h - padding);

  float scaleX = (float)maxW / image->imageHeader.width;
  float scaleY = (float)maxH / image->imageHeader.height;
  float idealZoom = std::min({scaleX, scaleY, 1.0f});
  int winHeight = (int)(image->imageHeader.height * idealZoom);
  int winWidth = (int)(image->imageHeader.width * idealZoom);

  ViewState vState;
  vState.zoom = idealZoom;
  vState.offsetX = 0.0f;
  vState.offsetY = 0.0f;

  Image sourceImage;
  sourceImage.imageHeader.width = image->imageHeader.width;
  sourceImage.imageHeader.height = image->imageHeader.height;
  sourceImage.pixelData = image->pixelData;

  std::vector<Pixel> processedBuffer(winWidth * winHeight);
  InverseMap::ApplyInverseMap(sourceImage, vState, processedBuffer.data(),
                              winWidth, winHeight);

  PPMViewer imageViewer(winWidth, winHeight);
  imageViewer.DrawData(processedBuffer);
}

void ProcessImage(std::optional<Image> &image, const SDL_DisplayMode &dm,
                  float zoom) {
  int winHeight = (int)(image->imageHeader.height * zoom);
  int winWidth = (int)(image->imageHeader.width * zoom);

  ViewState vState;
  vState.zoom = zoom;
  vState.offsetX = 0.0f;
  vState.offsetY = 0.0f;

  Image sourceImage;
  sourceImage.imageHeader.width = image->imageHeader.width;
  sourceImage.imageHeader.height = image->imageHeader.height;
  sourceImage.pixelData = image->pixelData;

  std::vector<Pixel> processedBuffer(winWidth * winHeight);
  InverseMap::ApplyInverseMap(sourceImage, vState, processedBuffer.data(),
                              winWidth, winHeight);

  PPMViewer imageViewer(winWidth, winHeight);
  imageViewer.DrawData(processedBuffer);
}

int main(int argc, char *argv[]) {
  bool verboseFlag = false;
  std::string filePath = "";

  float zoomAmount = DEFAULTZOOM;
  bool zoomPreference = false;
  uint threadCount = DEFAULTTHREADCOUNT;

  std::vector<std::string> args(argv, argc + argv);

  if (!GetArgs(args, verboseFlag, filePath, zoomAmount, threadCount)) {
    return 1;
  }

  if (zoomAmount != DEFAULTZOOM)
    zoomPreference = true;

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "Could not initialize SDL to get screen size" << "\n";
    return 1;
  }
  SDL_DisplayMode dm;
  if (SDL_GetDesktopDisplayMode(0, &dm) != 0) {
    std::cerr << "Could not get display mode" << "\n";
    return 1;
  }

  if (auto image = Parser::ParseFile(filePath, threadCount, verboseFlag)) {
    if (image.has_value()) {
      if (zoomPreference) {
        ProcessImage(image, dm, zoomAmount);
      } else {
        ProcessImage(image, DEFAULTPADDING, dm);
      }
    } else {
      std::cerr << "Error: Image failed to load" << "\n";
    }
  }

  return 0;
}
