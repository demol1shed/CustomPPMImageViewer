# CLAUDE.md

Guidance for Claude Code (claude.ai/code) in this repository.

Architecture, code quirks, threading rules and the roadmap live in `ARCHITECTURE.md`. Read it before changing code and do not violate it.

## Build / test / run

- Compiler `c++` (C++17); dependency `libsdl2-dev`.
- `make` builds, `make clean` cleans.
- `./view <path-to.ppm>`, with flags e.g. `./view <path-to.ppm> -v -z 0.5`.
- `make test`: deterministic unit tests in `tests/` (non-zero exit on failure). Run after changing parsing or interpolation logic.
- `make smoke`: runs the real `./view` headlessly on `tests/samples/` via the SDL dummy driver to catch crashes end-to-end.

## Code style

- Self-documenting names, short single-purpose methods, strict encapsulation, `.h`/`.cpp` separation.
- Prefer abstractions only when they cost nothing at runtime (templates, `auto`, `constexpr`); never at the expense of performance.
- Document complex pointer arithmetic and thread-synchronization logic in source comments.

## Red lines

- No external libraries; implement math/parsing utilities manually.
- Never remove or alter `#pragma pack(push, 1)` on `Pixel` (`include/Pixel.h`).
- All parallel work goes through `ThreadPool::Shared().Run(...)`, never per-call `std::thread`.
- Check for leaks when touching the SDL lifecycle (`PPMViewer.cpp`); every SDL primitive must be cleaned up.

## Git workflow

- Keep the commit history clean: small, descriptive commits at logical points instead of huge ones. One commit is one logical change; refactors, behavior changes and tests go in separate commits, so a reviewer can digest the code step by step.
- Commit messages are English Conventional Commits: `<type>(optional-scope): <imperative summary>`, at most ~72 chars, no trailing period. Types: `feat`, `fix`, `chore`, `refactor`, `perf`, `docs`, `test`, `build`, `style`. A body explaining what and why is mandatory beyond trivial one-liners.
- Never commit or push directly to `master`. Use a topic branch `<type>/<short-description>` (e.g. `feature/multithreadedParsing`, `fix/maxValOrder`) and open a PR with `gh pr create`.
- PR title uses the Conventional Commit format. The body is mandatory: a Summary (what and why) and a Test plan (`make`, `make test`, which `.ppm` files/flags). Keep each PR to one concern.
- Merge is squash-and-merge; the squash commit message must follow the rules above.
- Release tags are SemVer-style `vMAJOR.MINOR.PATCH` (breaking, feature, fix).
