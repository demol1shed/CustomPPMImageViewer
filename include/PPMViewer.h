#pragma once
#include "Pixel.h"
#include <SDL2/SDL.h>
#include <vector>

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080

class PPMViewer {
private:
  SDL_Window *pWin = nullptr;
  SDL_Renderer *pRen = nullptr;
  SDL_Texture *pTex = nullptr;
  int delay;
  std::vector<Pixel> pixelData; // at some point rename to pixelBuffer
  int width, height;

  bool PtrChecks();

public:
  PPMViewer(std::vector<Pixel> pixelData, std::vector<int> widthHeight,
            int delay);
  ~PPMViewer();
  void DrawData();
};
