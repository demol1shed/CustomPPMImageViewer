#pragma once
#include <Image.h>
#include <Pixel.h>
#include <SDL2/SDL.h>
#include <ViewController.h>
#include <vector>

class PPMViewer {
private:
  SDL_Window *pWin = nullptr;
  SDL_Renderer *pRen = nullptr;
  SDL_Texture *pTex = nullptr;
  int width, height; // carry to Image.h at some point

  bool PtrChecks();

  // Uploads a width*height frame to the texture and presents it.
  void Present(const std::vector<Pixel> &frame);
  // Resamples `image` through the controller's current view into `frame`,
  // presents it and shows the zoom level in the window title.
  void RenderView(const Image &image, const ViewController &view,
                  std::vector<Pixel> &frame, unsigned renderWorkers);
  // Applies one SDL event to the view. Returns true if the view changed and
  // the frame must be redrawn; sets `quit` when the user asked to close.
  bool HandleEvent(const SDL_Event &event, ViewController &view, bool &quit);
  bool HandleKey(SDL_Keycode key, ViewController &view, bool &quit);

public:
  // width/height must already fit the display (e.g. via ComputeWindowSize).
  // The viewer creates a window and texture at exactly these dimensions and
  // does not clamp internally, so the upload pitch matches the frame buffer.
  PPMViewer(int width, int height);
  ~PPMViewer();
  PPMViewer(const PPMViewer &) = delete;
  PPMViewer &operator=(const PPMViewer &) = delete;

  // Shows `image` and blocks until the window is closed, letting the user
  // zoom (mouse wheel, +/-) and pan (left-drag, arrow keys) through `view`.
  // `view` must have been built for this viewer's width/height.
  void Run(const Image &image, ViewController &view, unsigned renderWorkers);
};
