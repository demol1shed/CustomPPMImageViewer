# CLAUDE.md - Project Guidelines & AI Assistant Instructions

## 1. Project Identity & Philosophy
**Project**: Custom PPM Image Viewer
**Goal**: A lightweight, highly performant, hardware-accelerated image viewer built in C++17 using ONLY the SDL2 library.

**Core Philosophy**:
- **Clean, Modular OOP Architecture**: Follow strict Object-Oriented Programming principles. Classes should have a single responsibility (e.g., `Parser` handles file reading, `PPMViewer` handles SDL rendering). Maintain strict separation between declarations (`.h`) and implementations (`.cpp`).
- **Modern C++17 Standards**: Leverage C++17 features (e.g., `std::optional`, `constexpr`, structured bindings) to write expressive and safe code.
- **Performance-Driven Memory Management**: Use strictly manual memory management and low-level system design where performance demands it, but fallback to RAII principles for general resource safety.
- **Zero External Dependencies**: We build foundational parsing logic from scratch.
- **Maximum Performance**: Architecture must be capable of scaling, specifically targeting high-efficiency multithreaded operations.

## 2. Past Development & Current State
The project has established a custom parser for both ASCII (P3) and binary (P6) `.ppm` files. It dynamically handles complex headers, varying whitespace, and inline comments without relying on standard image-loading libraries. 
The rendering pipeline is custom-built:
- **Scaling Math**: Uses a custom Bilinear Interpolation algorithm (`BilinearLerper.cpp`) combined with Inverse Mapping (`InverseMap.cpp`) to scale images accurately. Inverse mapping is used to avoid artifacting by mapping screen pixels *back* to the source pixels.
- **Hardware Acceleration**: Relies entirely on `SDL_Renderer` and `SDL_Texture` (`PPMViewer.cpp`) for fast pixel-buffer pushing.
- **Smart Windowing**: The application automatically clamps window sizes to the user's desktop display mode while calculating the ideal initial zoom level.

## 3. Future Roadmap (Your Tasks)
When assisting with this project, prioritize the following roadmap:
1. **Multithreaded Parsing**: The primary architectural target is transitioning this to a 32-thread capable viewer. You must structure future parser optimizations around thread pools, mutexes, or lock-free data structures to drastically reduce load times for large P6 binaries.
2. **Interactive Panning and Zooming**: The `ViewState` struct currently holds static data. We need to implement real-time SDL event polling for mouse wheel zooming and drag-to-pan mechanics.
3. **Format Expansion**: Future format support must follow the same raw-byte parsing philosophy while adhering to our modular OOP design (e.g., creating base `ImageParser` interfaces if necessary to avoid monolithic classes).

## 4. Crucial Code Segments & Architectural Quirks
You must thoroughly understand these parts of the codebase before making changes:

- **`include/Pixel.h` (Memory Alignment)**: 
  The `Pixel` struct is wrapped in `#pragma pack(push, 1)`. **Do not remove or alter this.** It is absolutely critical for portability and prevents the compiler from adding padding bytes. This allows `Parser.cpp` to safely use `reinterpret_cast<char *>()` to read massive blocks of P6 binary data directly into the `std::vector<Pixel>` buffer in one operation.
  
- **`src/InverseMap.cpp` (Coordinate System)**:
  We iterate over the *screen buffer*, not the image. For every screen pixel `(x, y)`, we calculate the corresponding fractional image coordinate `(u, v)` using the `ViewState` (zoom and offset). If `(u, v)` falls out of bounds, it renders black.

- **`src/PPMViewer.cpp` (SDL Lifecycle & RAII)**:
  SDL pointers (`SDL_Window*`, `SDL_Renderer*`, `SDL_Texture*`) are managed manually. The destructor explicitly cleans these up. If adding new SDL primitives, ensure they are added to the cleanup lifecycle, or encapsulate them using smart pointers with custom deleters if it improves modularity without harming performance.

## 5. Build & Execution Rules
- **Compiler**: `c++` (C++17 standard)
- **Dependencies**: `libsdl2-dev`
- **Build Command**: `make`
- **Run Basic**: `./view <path-to.ppm>`
- **Run with Flags**: `./view <path-to.ppm> -v -z 0.5`
- **Clean**: `make clean`

## 6. Strict AI Rules for this Repository
1. **Adhere to Clean Code Practices**: Write self-documenting code with meaningful variable/method names. Keep methods short and focused on a single task to prevent monolithic functions.
2. **OOP and Modularity First**: Maintain strict encapsulation. Use interfaces or abstract base classes if the architecture needs to scale (e.g., supporting new file formats).
3. **Do not use high-level abstractions** if they sacrifice performance, but DO use them if they improve readability with zero runtime cost (e.g., templates, `auto`, `constexpr`).
4. **Always check for memory leaks** when modifying the SDL lifecycle. Use RAII principles where appropriate to guarantee resource cleanup.
5. **No external libraries**: If a math or parsing utility is needed, implement it manually.
6. When writing code, specifically **document any complex pointer arithmetic or thread-synchronization logic** directly in the source comments.
