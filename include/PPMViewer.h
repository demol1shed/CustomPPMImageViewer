#pragma once
#include <Pixel.h>
#include <SDL2/SDL.h>
#include <vector>

class PPMViewer {
private:
  SDL_Window *pWin = nullptr;
  SDL_Renderer *pRen = nullptr;
  SDL_Texture *pTex = nullptr;
  int width, height; // carry to Image.h at some point

  bool PtrChecks();

public:
  PPMViewer(int width, int height);
  ~PPMViewer();
  void DrawData(const std::vector<Pixel> &pixelData);
};
