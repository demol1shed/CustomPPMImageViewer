#include <PPMViewer.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <strings.h>
#include <vector>

PPMViewer::PPMViewer(std::vector<Pixel> pixelData, std::vector<int> widthHeight,
                     int delay) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL could not be initialized\nError: " << SDL_GetError()
              << "\n";
  }
  this->delay = delay;
  this->pixelData = pixelData;
  this->width = widthHeight[0];
  this->height = widthHeight[1];

  SDL_DisplayMode dm;
  int windowW = this->width;
  int windowH = this->height;

  if (SDL_GetDesktopDisplayMode(0, &dm) == 0) {
    // clamp image width if larger
    if (windowW > dm.w) {
      windowW = dm.w;
    }
    // clamp image height if larger
    if (windowH > dm.h) {
      windowH = dm.h;
    }
  } else {
    std::cerr << "Warning: Could not get display mode. Using image size.\n";
  }
  pWin = SDL_CreateWindow("PPM Image Viewer", SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED, windowW, windowH,
                          SDL_WINDOW_SHOWN);
  if (pWin) {
    pRen = SDL_CreateRenderer(pWin, -1, SDL_RENDERER_ACCELERATED);

    pTex =
        SDL_CreateTexture(pRen, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC,
                          this->width, this->height);
  }
}

PPMViewer::~PPMViewer() {
  SDL_Delay(this->delay);
  if (PtrChecks()) {
    SDL_DestroyTexture(pTex);
    SDL_DestroyRenderer(pRen);
    SDL_DestroyWindow(pWin);
  }
  SDL_Quit();
}

bool PPMViewer::PtrChecks() {
  if (!pRen || !pTex) {
    return false;
  }
  return true;
}

void PPMViewer::DrawData() {
  if (!PtrChecks()) {
    return;
  }
  SDL_UpdateTexture(pTex, NULL, pixelData.data(), width * 3);
  SDL_SetRenderDrawColor(pRen, 0, 0, 0, 255);
  SDL_RenderClear(pRen);
  SDL_RenderCopy(pRen, pTex, NULL, NULL);
  SDL_RenderPresent(pRen);
}
