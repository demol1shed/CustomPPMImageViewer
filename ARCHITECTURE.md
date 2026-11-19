# Architecture

**Custom PPM Image Viewer**: a lightweight, highly performant, hardware-accelerated image viewer built in C++17 using only the SDL2 library.

## Design principles

- **Clean, modular OOP.** Classes have a single responsibility (e.g., `Parser` handles file reading, `PPMViewer` handles SDL rendering). Declarations (`.h`) and implementations (`.cpp`) are strictly separated.
- **Modern C++17.** Features such as `std::optional`, `constexpr` and structured bindings are used for expressive, safe code.
- **Performance-driven memory management.** Manual memory management and low-level design where performance demands it, RAII for general resource safety.
- **Zero external dependencies.** Foundational parsing logic is built from scratch; SDL2 is the only library.
- **Built to scale.** The architecture targets high-efficiency multithreaded operation.

## Current state

The project has a custom parser for both ASCII (P3) and binary (P6) `.ppm` files. It handles complex headers, varying whitespace and inline comments without any standard image-loading library.

The rendering pipeline is custom-built:

- **Scaling math.** A custom bilinear interpolation (`BilinearLerper.cpp`) combined with inverse mapping (`InverseMap.cpp`) scales images accurately. Inverse mapping avoids artifacting by mapping screen pixels *back* to the source pixels.
- **Hardware acceleration.** `SDL_Renderer` and `SDL_Texture` (`PPMViewer.cpp`) push the pixel buffer to the screen.
- **Smart windowing.** The window size is clamped to the user's desktop display mode while the ideal initial zoom level is calculated.
- **Interactive viewing.** `ViewController` owns the zoom/pan math (pure, unit tested). `PPMViewer::Run` translates SDL events (mouse wheel, left-drag, keys) into calls on it and re-renders through the row-threaded `InverseMap`.

## Key code segments and quirks

- **`include/Pixel.h` (memory alignment).** The `Pixel` struct is wrapped in `#pragma pack(push, 1)`. **Do not remove or alter this.** It is critical for portability and prevents the compiler from adding padding bytes. This lets `Parser.cpp` safely use `reinterpret_cast<char *>()` to read massive blocks of P6 binary data directly into the `std::vector<Pixel>` buffer in one operation.
- **`src/InverseMap.cpp` (coordinate system).** We iterate over the *screen buffer*, not the image (split into disjoint row bands, one per worker thread, for interactive redraws). For every screen pixel `(x, y)`, the corresponding fractional image coordinate `(u, v)` is computed from the `ViewState` (zoom and offset). If `(u, v)` is out of bounds, the pixel renders black.
- **`src/PPMViewer.cpp` (SDL lifecycle and RAII).** SDL pointers (`SDL_Window*`, `SDL_Renderer*`, `SDL_Texture*`) are managed manually and the destructor cleans them up explicitly. New SDL primitives must be added to the cleanup lifecycle, or wrapped in smart pointers with custom deleters if that improves modularity without harming performance.

## Threading

- P6 and P3 parse in parallel (size-gated via `ParallelThresholds`). Parsing and redraws dispatch onto the shared fork-join `ThreadPool` (`include/ThreadPool.h`).
- **Standing rule:** all new parallel work (parsing, rendering, future formats) MUST go through `ThreadPool::Shared().Run(...)`, never per-call `std::thread` spawning.
- Tasks write disjoint slices, so no extra locking is needed (see the synchronisation notes in `src/ThreadPool.cpp`).
- The pool scales to `hardware_concurrency()` (16 on the dev box). The original 32-thread target was an aspiration and has not been measured.

## Roadmap and status

1. **Multithreaded parsing:** implemented. Remaining ideas (no commitment): mmap I/O, SIMD integer parsing, and a cold-cache P6 benchmark (`make bench` is warm-cache only).
2. **Interactive panning and zooming:** implemented. Remaining idea: a resizable window.
3. **Format expansion** (optional, not started): future format support must follow the same raw-byte parsing philosophy and the modular OOP design. If a second format is ever added, the first step is extracting an `ImageParser` interface (with the PPM parser as its first implementation) rather than bolting the new format onto `Parser`.
