# Changelog

All notable changes to this project are documented in this file. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the
project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
Notes for 0.4.0 and earlier are on the
[GitHub Releases](https://github.com/demol1shed/CustomPPMImageViewer/releases)
page.

## [Unreleased]

## [0.4.1] - 2026-11-27

### Added

- `make test-asan` and `make test-tsan` run the unit tests under sanitizers,
  and GitHub Actions builds and tests pull requests. (#24)

### Changed

- The default build uses `-O2 -Wall -Wextra`; `make DEBUG=1` builds for
  debugging. (#24)
- The thread pool wakes no worker for a single task and only one for two. (#24)
- `make smoke` fails on any error exit, not only on crashes. (#22)

### Fixed

- Non-numeric header dimensions no longer hang the parser, and unsupported
  formats and invalid dimensions or maximum values are rejected. (#20)
- Single-line P6 headers no longer lose leading pixel bytes. (#20)
- P3 samples outside the declared range are rejected instead of wrapping
  around, and `#` comments in P3 pixel data are skipped. (#20)
- Images with a maximum value below 255 are no longer shown too dark. (#20)
- Row partitioning no longer overflows for very tall images. (#20)
- The right and bottom edges are no longer black at 1:1 zoom, images with a
  side of two pixels or less are drawn, and interpolation rounds to nearest.
  (#21)
- The viewer falls back to a software renderer, exits non-zero with the SDL
  error when it cannot set up its window, and shuts SDL down on every exit
  path. (#22)
- `-z 1.0` is honoured; malformed `-t` and `-z` values, unknown options and
  extra file paths are rejected; `--help` exits with status 0. (#23)

[Unreleased]: https://github.com/demol1shed/CustomPPMImageViewer/compare/v0.4.1...HEAD
[0.4.1]: https://github.com/demol1shed/CustomPPMImageViewer/compare/v0.4.0...v0.4.1
