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
  // width/height must already fit the display (e.g. via ComputeWindowSize).
  // The viewer creates a window and texture at exactly these dimensions and
  // does not clamp internally, so DrawData's upload pitch matches the buffer.
  PPMViewer(int width, int height);
  ~PPMViewer();
  void DrawData(const std::vector<Pixel> &pixelData);
};
