#include "Pixel.h"
#include <PPMViewer.h>
#include <Parser.h>
#include <RenderGeometry.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <ViewController.h>
#include <algorithm>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

constexpr float DEFAULTZOOM = 1.0f;
constexpr int DEFAULTPADDING = 200;
constexpr int DEFAULTARGCOUNT = 7;
constexpr unsigned DEFAULTTHREADCOUNT = 1;

namespace {

// Shuts SDL down on every exit path. Declared in main right after SDL_Init, so
// it outlives (and is destroyed after) any PPMViewer.
struct SdlVideoGuard {
  SdlVideoGuard() = default;
  ~SdlVideoGuard() { SDL_Quit(); }
  SdlVideoGuard(const SdlVideoGuard &) = delete;
  SdlVideoGuard &operator=(const SdlVideoGuard &) = delete;
};

} // namespace

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
      << "  -t  <uint>, --threads <uint>        Set thread count (e.g., 32)\n"
      << bold << "Controls: " << reset << "\n"
      << "  Mouse wheel, + / -                  Zoom in / out\n"
      << "  Left-drag, arrow keys               Pan\n"
      << "  0 or R                              Reset the view\n"
      << "  Esc or Q                            Quit\n";
}

bool GetArgs(const std::vector<std::string> &argv, bool &verbose,
             std::string &filePath, float &zoomAmount,
             unsigned &threadCount) {
  if (argv.size() > DEFAULTARGCOUNT) {
    PrintHelp();
    return false;
  }

  for (size_t i = 1; i < argv.size(); i++) {
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

// Computes the zoom that fits the image inside the desktop work area (display
// size minus padding), never upscaling past 1.0.
float FitZoom(const Image &image, int padding, const SDL_DisplayMode &dm) {
  int maxW = std::max(1, dm.w - padding);
  int maxH = std::max(1, dm.h - padding);

  float scaleX = (float)maxW / image.imageHeader.width;
  float scaleY = (float)maxH / image.imageHeader.height;
  return std::min({scaleX, scaleY, 1.0f});
}

// Rendering a full-screen frame is always worth threading, independent of the
// parse-oriented `-t` flag, so redraws use every reported core.
// hardware_concurrency() may return 0 when unknown -> stay on one thread.
unsigned RenderWorkerCount() {
  return std::max(1u, std::thread::hardware_concurrency());
}

// Opens a window sized by `zoom` (clamped to the display) and runs the
// interactive viewer until it is closed. Takes the source by const reference
// so the (potentially large) pixel buffer is never copied.
bool ShowInteractive(const Image &image, float zoom,
                     const SDL_DisplayMode &dm) {
  // Decide the window size once and use it for the view math and the viewer
  // (which sizes its frame buffer and texture from it), so the SDL texture
  // pitch always matches the buffer row stride (a mismatch here is what
  // sheared the image at high zoom). See RenderGeometry.h. Beyond the fit
  // point the view starts at the top-left region and can be panned.
  WinSize win = ComputeWindowSize(image.imageHeader.width,
                                  image.imageHeader.height, zoom, dm.w, dm.h);

  ViewController view(image.imageHeader.width, image.imageHeader.height, win.w,
                      win.h, zoom);
  PPMViewer imageViewer(win.w, win.h);
  return imageViewer.Run(image, view, RenderWorkerCount());
}

int main(int argc, char *argv[]) {
  bool verboseFlag = false;
  std::string filePath = "";

  float zoomAmount = DEFAULTZOOM;
  bool zoomPreference = false;
  unsigned threadCount = DEFAULTTHREADCOUNT;

  std::vector<std::string> args(argv, argv + argc);

  if (!GetArgs(args, verboseFlag, filePath, zoomAmount, threadCount)) {
    return 1;
  }

  if (zoomAmount != DEFAULTZOOM)
    zoomPreference = true;

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "Could not initialize SDL to get screen size" << "\n";
    return 1;
  }
  const SdlVideoGuard sdlGuard;
  SDL_DisplayMode dm;
  if (SDL_GetDesktopDisplayMode(0, &dm) != 0) {
    std::cerr << "Could not get display mode" << "\n";
    return 1;
  }

  auto image = Parser::ParseFile(filePath, threadCount, verboseFlag);
  if (!image) {
    std::cerr << "Error: Image failed to load" << "\n";
    return 1;
  }

  float zoom = zoomPreference ? zoomAmount : FitZoom(*image, DEFAULTPADDING, dm);
  return ShowInteractive(*image, zoom, dm) ? 0 : 1;
}
