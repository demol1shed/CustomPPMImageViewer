# Verification & Test Suite

A deterministic, zero-dependency sandbox for catching regressions in the
algorithmic core (parsing + scaling math) without needing a display. Graphics
code can compile cleanly yet still produce visually wrong output, so these
tests assert on exact pixel values instead.

## Layout

| Path | Purpose |
|------|---------|
| `TestFramework.h` | Tiny header-only test harness (`TEST`, `CHECK_*`, runner). No external libs. |
| `SampleData.h` | Shared 4x4 fixture definition + in-memory image builder. |
| `test_main.cpp` | Runner entry point (`testing::runAll()`). |
| `test_parser.cpp` | `Parser::ParseFile` — P3/P6 headers, pixels, cross-format agreement, missing file. |
| `test_bilinear.cpp` | `BilinearLerper::SampleBilinear` — exact pixels and blended midpoints. |
| `test_inversemap.cpp` | `InverseMap::ApplyInverseMap` — bounds/black edges, upscaling, degenerate image. |
| `samples/4x4.ppm` | Known 4x4 ASCII (P3) fixture (with comments, to exercise the parser). |
| `samples/4x4_p6.ppm` | The same image in raw binary (P6). |

## Running

```bash
make test     # build + run the unit tests (exit code 0 = all passed)
make smoke    # run the real ./view binary headlessly on the samples
make clean    # remove build artifacts incl. run_tests
```

`make test` is the primary regression gate: it links `Parser`, `BilinearLerper`
and `InverseMap` directly (no SDL), runs every registered case, and returns a
non-zero exit code if any check fails. Run it after changing parsing or
interpolation logic — e.g. when adding multithreaded parsing — to confirm the
core still decodes the fixtures correctly.

`make smoke` exercises the fully linked executable end-to-end using SDL's dummy
video driver (`SDL_VIDEODRIVER=dummy`), so it needs no real display. It fails
if the binary crashes (segfault/abort) or if verbose output does not report the
expected parsed dimensions.

## The fixture

Both sample files encode the identical image. Pixel `i`, laid out row-major
(`i = y*4 + x`), is defined by a deterministic formula:

```
r = 16 * i      g = 255 - 16 * i      b = 8 * i
```

The channels are deliberately asymmetric so that a transpose, a row/column
swap, or an off-by-one shifts at least one value and trips a test. This formula
is the single source of truth (`sample::expectedPixel` in `SampleData.h`); if
you change it, regenerate the binary sample so the two stay in sync:

```bash
python3 - <<'PY'
data = bytearray(b"P6\n4 4\n255\n")
for i in range(16):
    data += bytes([(16*i) & 0xff, (255-16*i) & 0xff, (8*i) & 0xff])
open("tests/samples/4x4_p6.ppm", "wb").write(data)
PY
```

## Adding a test

```cpp
#include "SampleData.h"
#include "TestFramework.h"

TEST(Suite, DescriptiveName) {
  CHECK_TRUE(condition);
  CHECK_EQ(expected, actual);
  CHECK_PIXEL(somePixel, r, g, b);
}
```

Any `tests/*.cpp` file is picked up automatically by the Makefile; tests
self-register via the `TEST` macro, so no manual wiring is needed.
