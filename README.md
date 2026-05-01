# Custom PPM Image Viewer
This is a lightweight, hardware accelerated C++ application for parsing and viewing **.ppm (Portable Pixel Map)** images. This project manually parses **ASCII based** and **raw** .ppm files and renders them using the **SDL2** library.

## Features
* **Custom Parser:** Manually reads .ppm files, handling headers, comments, empty spaces and different types of formatting data.
* **Hardware Acceleration:** Uses `SDL_Renderer` and `SDL_Texture` for efficient rendering.
* **Smart Windowing:** Automatically resizes the window and the image according to the size of the image.

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

### MacOS
For macOS systems you might want to have homebrew installed for convenience.
```bash
brew install sdl2
```

## Building 
You can use the Makefile for easy compilation.

After cloning the repository, simply run
``` make ```. This will generate an executable with the name ```view```.
If you are on an UNIX like system, then run ```chmod +x view```. Then you are ready to go.

## Usage
### Basic Usage
```./view <path-to-your-image-file.ppm>```

### Help Flag
For further information you can use the `-h` flag.

```./view <path-to-your-image-file.ppm> [-h/--help]```


## Project Structure
```
├── include/               # Custom header files
|   ├── BilinearLerper.h    # Declarations of the bilinear interpolation algorithm
│   ├── Image.h             # Struct to hold image data
│   ├── InverseMap.h        # Declarations of the inverse mapping algorithm
│   ├── ViewState.h         # Struct to hold zooming and offsetting data
│   ├── Pixel.h             # Packed struct for pixel data
│   ├── Parser.h            # Declarations of file reading and text parsing
│   └── PPMViewer.h         # Declarations of SDL2 window creation and rendering
├── src/                   # Implementation files
│   ├── main.cpp            # Entry point & argument handling
│   ├── Parser.cpp          # Implementations for file reading and parsing information
│   ├── BilinearLerper.cpp  # Implementations for the bilinear interpolation algorithm
│   ├── InverseMap.cpp      # Implementations for the inverse mapping algorithm
│   └── PPMViewer.cpp       # Implementations for window creation and rendering the image
├── Makefile              # Build configuration file
└── README.md             # README file?
```

## TODO:
* Multithreaded parsing
* Interactive panning and zooming system
* Implementation of a better way of passing files to the program
* Support for different formats
