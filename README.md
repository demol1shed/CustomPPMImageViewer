# Custom PPM Image Viewer
This is a lightweight, hardware accelerated C++ application for parsing and viewing **.ppm (Portable Pixel Map)** images. This project manually parses **ASCII based** .ppm files and renders them using the **SDL2** library. Support for **raw** .ppm files is something that is currently WIP. 

Also, this program is horribly inefficient and is simply me trying to challenge myself. Not only that the program itself is inefficient; the .ppm file format is also inefficient, quoted by netpbm.

## Features
* **Custom Parser:** Manually reads .ppm files (only in the P3 format though.), handling headers, comments, empty spaces and different types of formatting data.
* **Hardware Acceleration:** Uses `SDL_Renderer` and `SDL_Texture` for efficient rendering.
* **Smart Windowing**(kind of)**:** Automatically resizes the window according to the size of the image. No zooming system though. If the image is larger than the dimensions of your screen, the image may appear warped in order to fit your screen.

## Dependencies
To build and run this project, all that you'll need is a C++ compiler and the **SDL2 Development Library**.

### Debian/Ubuntu/Mint
```bash
sudo apt install g++ make libsdl2-dev
```

### Arch
```bash
sudo pacman -S sdl2
```

### Fedora
```bash
sudo dnf install SDL2-devel
```

### Windows
For Windows systems you will need to set up SDL2 with MinGW and Visual Studio. If using MinGW ensure that ```SDL2.dll``` is in your build directory.

#### For MSYS2: 
```bash
sudo pacman -S mingw-w64-x86_64-SDL2
```

#### For Visual Studio:
Download the development libraries from the SDL GitHub and link them manually. Any other version than SDL2 will result in the program not building.

### MacOS
For macOS systems you might want to have homebrew installed for convenience.
```bash
brew install sdl2
```

## Building 
You can use the Makefile for easy compilation.

After cloning the repository, simply run:
``` make ```. This will generate an executable with the name ```view```.
If you are on an UNIX like system, then run ```chmod +x view```. Then you are ready to go.

## Usage
### Basic Usage
```./view <path-to-your-image-file.ppm>```

### Verbose Flag
```./view <path-to-your-image-file.ppm> -v```

This will display the parsed header values of the .ppm file, such as type, maxVal, width etc. It will also print every single pixel's RGB values as well. Which will severely slow down the process of viewing the image, so use at your own risk.

## Project Structure
```
├── include/          # Custom header files
│   ├── Pixel.h       # Packed struct for pixel data
│   ├── Parser.h      # Declarations of file reading and text parsing
│   └── PPMViewer.h   # Declarations of SDL2 window creation and rendering
├── src/              # Implementation files
│   ├── main.cpp      # Entry point & argument handling
│   ├── Parser.cpp    # Implementation for file reading and parsing information
│   └── PPMViewer.cpp # Implementation for window creation and rendering the image
├── Makefile          # Build configuration
└── README.md         # README file?
```

## TODO
* Supporting raw binary (P6) format.
* Add better window resizing and zooming, instead of warping the image to fit the screen.
* Implement a better way of passing files to the program.
* Support for different formats?  
