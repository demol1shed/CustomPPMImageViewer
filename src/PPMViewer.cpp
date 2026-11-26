#include <InverseMap.h>
#include <PPMViewer.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <cmath>
#include <string>

namespace {

// One wheel notch / key press scales the zoom by this factor (and its inverse
// when zooming out), so equal steps in and out return to the same zoom.
constexpr float kZoomStep = 1.1f;
// Arrow keys move the image by this many screen pixels per press.
constexpr float kKeyPanStep = 60.0f;

} // namespace

PPMViewer::PPMViewer(int width, int height) {
  // The caller passes display-fitting dimensions (see ComputeWindowSize); the
  // viewer renders at exactly this size, so the texture pitch matches the
  // source buffer's row stride. No internal clamping (a second, independent
  // clamp here was the source of the high-zoom diagonal-shear bug).
  this->width = width;
  this->height = height;

  pWin = SDL_CreateWindow("PPM Image Viewer", SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED, this->width, this->height,
                          SDL_WINDOW_SHOWN);
  if (!pWin)
    return;

  pRen = SDL_CreateRenderer(pWin, -1, SDL_RENDERER_ACCELERATED);
  if (!pRen)
    pRen = SDL_CreateRenderer(pWin, -1, SDL_RENDERER_SOFTWARE);
  if (!pRen)
    return;

  // STREAMING: the texture is re-uploaded on every pan/zoom redraw.
  pTex = SDL_CreateTexture(pRen, SDL_PIXELFORMAT_RGB24,
                           SDL_TEXTUREACCESS_STREAMING, this->width,
                           this->height);
}

PPMViewer::~PPMViewer() {
  // SDL_Destroy* reject null, and a failed renderer/texture must not leak the
  // window that was created before it, so each handle is released on its own.
  if (pTex)
    SDL_DestroyTexture(pTex);
  if (pRen)
    SDL_DestroyRenderer(pRen);
  if (pWin)
    SDL_DestroyWindow(pWin);
  SDL_Quit();
}
bool PPMViewer::PtrChecks() {
  if (!pRen || !pTex) {
    return false;
  }
  return true;
}

void PPMViewer::Present(const std::vector<Pixel> &frame) {
  SDL_UpdateTexture(pTex, NULL, frame.data(), width * 3);
  SDL_RenderClear(pRen);
  SDL_RenderCopy(pRen, pTex, NULL, NULL);
  SDL_RenderPresent(pRen);
}

void PPMViewer::RenderView(const Image &image, const ViewController &view,
                           std::vector<Pixel> &frame, unsigned renderWorkers) {
  InverseMap::ApplyInverseMap(image, view.State(), frame.data(), width, height,
                              renderWorkers);
  Present(frame);

  const long zoomPercent = std::lround(view.State().zoom * 100.0f);
  const std::string title =
      "PPM Image Viewer - " + std::to_string(zoomPercent) + "%";
  SDL_SetWindowTitle(pWin, title.c_str());
}

bool PPMViewer::HandleKey(SDL_Keycode key, ViewController &view, bool &quit) {
  const float centerX = width * 0.5f;
  const float centerY = height * 0.5f;
  switch (key) {
  case SDLK_ESCAPE:
  case SDLK_q:
    quit = true;
    return false;
  case SDLK_PLUS:
  case SDLK_EQUALS:
  case SDLK_KP_PLUS:
    return view.ZoomAt(centerX, centerY, kZoomStep);
  case SDLK_MINUS:
  case SDLK_KP_MINUS:
    return view.ZoomAt(centerX, centerY, 1.0f / kZoomStep);
  case SDLK_0:
  case SDLK_KP_0:
  case SDLK_r:
    return view.Reset();
  // Arrow keys move the viewport over the image, so the image itself slides
  // the opposite way.
  case SDLK_LEFT:
    return view.PanBy(kKeyPanStep, 0.0f);
  case SDLK_RIGHT:
    return view.PanBy(-kKeyPanStep, 0.0f);
  case SDLK_UP:
    return view.PanBy(0.0f, kKeyPanStep);
  case SDLK_DOWN:
    return view.PanBy(0.0f, -kKeyPanStep);
  default:
    return false;
  }
}

bool PPMViewer::HandleEvent(const SDL_Event &event, ViewController &view,
                            bool &quit) {
  switch (event.type) {
  case SDL_QUIT:
    quit = true;
    return false;
  case SDL_KEYDOWN:
    return HandleKey(event.key.keysym.sym, view, quit);
  case SDL_MOUSEWHEEL: {
    if (event.wheel.y == 0)
      return false;
    // Zoom around the cursor so the pixel under it stays put. `wheel.y` is
    // the notch count, so a fast scroll applies several steps at once.
    int mouseX = 0, mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);
    const float factor =
        std::pow(kZoomStep, static_cast<float>(event.wheel.y));
    return view.ZoomAt(static_cast<float>(mouseX), static_cast<float>(mouseY),
                       factor);
  }
  case SDL_MOUSEMOTION:
    if (event.motion.state & SDL_BUTTON_LMASK) {
      return view.PanBy(static_cast<float>(event.motion.xrel),
                        static_cast<float>(event.motion.yrel));
    }
    return false;
  case SDL_WINDOWEVENT:
    // The window content is lost when it is uncovered; ask for a redraw.
    return event.window.event == SDL_WINDOWEVENT_EXPOSED;
  default:
    return false;
  }
}

void PPMViewer::Run(const Image &image, ViewController &view,
                    unsigned renderWorkers) {
  if (!PtrChecks()) {
    return;
  }
  std::vector<Pixel> frame(static_cast<size_t>(width) * height);
  RenderView(image, view, frame, renderWorkers);

  bool quit = false;
  SDL_Event event;
  while (!quit && SDL_WaitEvent(&event)) {
    // Drain everything already queued before redrawing: a drag produces many
    // motion events per frame, and resampling once per batch (instead of once
    // per event) keeps the view glued to the cursor.
    bool needsRedraw = false;
    do {
      needsRedraw |= HandleEvent(event, view, quit);
    } while (!quit && SDL_PollEvent(&event));

    if (needsRedraw && !quit)
      RenderView(image, view, frame, renderWorkers);
  }
}
