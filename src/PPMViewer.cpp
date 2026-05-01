#include <PPMViewer.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <iostream>

PPMViewer::PPMViewer(int width, int height) {
  this->width = width;
  this->height = height;

  SDL_DisplayMode dm;

  if (SDL_GetDesktopDisplayMode(0, &dm) == 0) {
    // clamp image width if larger
    if (this->width > dm.w) {
      this->width = dm.w;
    }
    // clamp image height if larger
    if (this->height > dm.h) {
      this->height = dm.h;
    }
  } else {
    std::cerr << "Warning: Could not get display mode. Using image size.\n";
  }
  pWin = SDL_CreateWindow("PPM Image Viewer", SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED, this->width, this->height,
                          SDL_WINDOW_SHOWN);
  if (pWin) {
    pRen = SDL_CreateRenderer(pWin, -1, SDL_RENDERER_ACCELERATED);

    pTex =
        SDL_CreateTexture(pRen, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC,
                          this->width, this->height);
  }
}

PPMViewer::~PPMViewer() {
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
void PPMViewer::DrawData(const std::vector<Pixel> &pixelData) {
  if (!PtrChecks()) {
    return;
  }
  SDL_UpdateTexture(pTex, NULL, pixelData.data(), width * 3);
  SDL_RenderClear(pRen);
  SDL_RenderCopy(pRen, pTex, NULL, NULL);
  SDL_RenderPresent(pRen);

  bool quit = false;
  SDL_Event quitEvent;

  while (!quit && SDL_WaitEvent(&quitEvent)) {
    switch (quitEvent.type) {
    case SDL_QUIT:
      quit = true;
      break;
    case SDL_KEYDOWN:
      if (quitEvent.key.keysym.sym == SDLK_ESCAPE) {
        quit = true;
      }
      break;
    }
  }
}
