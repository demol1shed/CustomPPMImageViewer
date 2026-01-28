#include <SDL2/SDL.h>
#include <iostream>

int main(int argc, char *argv[]) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cout << "SDL could not be inited\n";
    return -1;
  }
  std::cout << "SDL inited succesfully\n";
  SDL_Delay(5000);
  SDL_Quit();
  return 0;
}
