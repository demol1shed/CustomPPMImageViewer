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
- Never commit or push directly to `master`. Use a topic branch `<type>/<short-description>` (e.g. `feature/multithreadedParsing`, `fix/maxValOrder`) and open a PR with `gh pr create`.
- PR body is mandatory: Summary and a Test plan (`make`, `make test`, which `.ppm` files/flags). Keep each PR to one concern.
- Merge is squash-and-merge; the squash commit message must follow the commit message rules below.

### Commit messages

Conventional Commits 1.0, written in English:

- Subject: `<type>(<optional-scope>): <imperative summary>`. Imperative mood ("add", not "added"), lowercase after the colon, at most 72 chars, no trailing period. The scope names the module or area (e.g. `ui`, `parser`).
- Types: `feat`, `fix`, `perf`, `refactor`, `docs`, `test`, `build`, `ci`, `chore`, `style`, `revert`.
- Body (after a blank line, wrapped at ~72 cols): explains what and why, not how. Mandatory beyond trivial one-liners.
- Breaking change: `!` after the type/scope plus a `BREAKING CHANGE:` footer. Reference issues in footers (`Fixes #123`, `Refs #123`).
- Every commit is one logical change, builds, and passes the tests. Do not leave WIP or fixup commits in a PR; amend them before pushing.
- Never commit secrets, build output, or unrelated formatting churn.
- The PR title follows the same format; the PR body has a Summary (what and why) and a Test plan.

### Releases

- Versioning: SemVer 2.0 `MAJOR.MINOR.PATCH`. A `feat` bumps MINOR, a `fix`/`perf` bumps PATCH, a breaking change (`!` / `BREAKING CHANGE:`) bumps MAJOR. While below 1.0.0, breaking changes bump MINOR. Pre-releases use `-rc.1`, `-beta.1` suffixes.
- Tags: annotated and prefixed with `v` (`git tag -a v1.2.0 -m "v1.2.0"`), created only on the default branch after the release PR is merged. Never move or delete a published tag; ship a new patch instead.
- Changelog: keep `CHANGELOG.md` in Keep a Changelog format (`Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`, `Security`, newest first, `Unreleased` on top), derived from the Conventional Commits since the last tag.
- Release PR: a dedicated PR titled `chore(release): vX.Y.Z` that only bumps the version (where the project defines one) and moves `Unreleased` into the new version section.
- GitHub Release: `gh release create vX.Y.Z --title "vX.Y.Z" --notes-file <notes>`, notes taken from the changelog section; mark pre-releases with `--prerelease`.
- Tagging, pushing tags and publishing releases are outward-facing: do them only when the user explicitly asks.
