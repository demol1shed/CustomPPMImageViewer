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
- **Run Tests**: `make test` (builds and runs the deterministic unit tests in `tests/`; non-zero exit code on any failure). Run this after changing parsing or interpolation logic to guard against regressions.
- **Smoke Test**: `make smoke` (runs the real `./view` binary headlessly on `tests/samples/` via the SDL dummy driver to catch crashes/segfaults end-to-end).
- **Clean**: `make clean`

## 6. Strict AI Rules for this Repository
1. **Adhere to Clean Code Practices**: Write self-documenting code with meaningful variable/method names. Keep methods short and focused on a single task to prevent monolithic functions.
2. **OOP and Modularity First**: Maintain strict encapsulation. Use interfaces or abstract base classes if the architecture needs to scale (e.g., supporting new file formats).
3. **Do not use high-level abstractions** if they sacrifice performance, but DO use them if they improve readability with zero runtime cost (e.g., templates, `auto`, `constexpr`).
4. **Always check for memory leaks** when modifying the SDL lifecycle. Use RAII principles where appropriate to guarantee resource cleanup.
5. **No external libraries**: If a math or parsing utility is needed, implement it manually.
6. When writing code, specifically **document any complex pointer arithmetic or thread-synchronization logic** directly in the source comments.

## 7. Git & Version Control Workflow
This repository follows a modern, conventional Git workflow. **Every commit, branch, and pull request MUST adhere to the rules below.** The existing history already follows this pattern (`feat: add argument parsing for thread count`, `chore: ...`, branches like `feature/multithreadedParsing`, PR `#6`) — stay consistent with it. All commit messages and PR descriptions are written in **English**, matching the existing history.

### 7.1 Commit Messages (Conventional Commits)
Every commit message MUST begin with a type prefix followed by a concise, imperative summary:

```
<type>(optional-scope): <short summary in the imperative mood>
```

**Allowed types:**
- **feat:** — a new feature or user-facing capability (e.g., `feat: add argument parsing for thread count`)
- **fix:** — a bug fix (e.g., `fix: parse maxVal before pixel data`)
- **chore:** — tooling, build, or housekeeping with no production-code behavior change (e.g., `chore: add CLAUDE.md for AI-assisted development`)
- **refactor:** — code restructuring that does not change observable behavior
- **perf:** — a performance improvement (highly relevant to the multithreading roadmap)
- **docs:** — documentation only (README, code comments, this file)
- **test:** — adding or fixing tests
- **build:** — Makefile, compiler, or dependency changes
- **style:** — formatting only (whitespace, clang-format), no logic change

**Rules:**
- Summary line ≤ ~72 chars, **imperative mood** ("add", not "added"/"adds"), no trailing period.
- Optional scope in parentheses to localize the change: `feat(parser): ...`, `fix(viewer): ...`.
- **A descriptive commit body is mandatory** for anything beyond a trivial one-liner. Leave a blank line after the summary, then explain **what** changed and **why** (not the line-by-line "how"). Reference the relevant roadmap item or issue where applicable.

**Example:**
```
feat(parser): parallelize P6 pixel reads across a thread pool

Split the binary pixel buffer into N row-ranges (one worker per range) so
large P6 files load without blocking. Falls back to a single thread when
hardware_concurrency() reports < 2. Addresses roadmap item #1
(Multithreaded Parsing).
```

### 7.2 Branch Naming
**Never commit directly to `master`** (the protected default branch and the base for all PRs). Create a topic branch named `<type>/<short-description>`, where `<type>` reuses the commit-type vocabulary:

- `feature/<name>` — new features (e.g., `feature/multithreadedParsing`)
- `fix/<name>` — bug fixes (e.g., `fix/maxValOrder`)
- `chore/<name>` — housekeeping (e.g., `chore/refactoring-codebase`)
- `refactor/<name>`, `perf/<name>`, `docs/<name>` — as appropriate

Keep the description short and meaningful (a few words). Match the casing of nearby branches.

### 7.3 Pull Requests
All changes reach `master` through a Pull Request — **never a direct push**. Open and manage PRs with the `gh` CLI (`gh pr create`, `gh pr view`).

- **Title**: uses the same Conventional Commit format as a commit (`feat: ...`, `fix: ...`) and summarizes the whole PR.
- **A PR description is mandatory.** Every PR body MUST explain:
  1. **What** the PR does — a short summary of the change.
  2. **Why** it is needed — motivation, the roadmap item it advances, or the bug it fixes.
  3. **How** it was verified — build passes (`make`), and which `.ppm` files / flags it was tested with.
  Prefer a brief **Summary** section followed by a **Test plan** checklist.
- Keep each PR focused on a single concern; small, reviewable diffs are strongly preferred.
- **Merge strategy**: **squash-and-merge** into `master` (matches PR `#6`, which landed as a single `(#6)` commit), keeping history linear. The resulting squash commit message must itself follow the Conventional Commit rules in §7.1.

### 7.4 Releases & Tags
Tag meaningful releases with **SemVer-style** tags (`vMAJOR.MINOR.PATCH`), consistent with existing tags (e.g., `v0.3.18`). Bump MAJOR for breaking changes, MINOR for new features, PATCH for fixes.
