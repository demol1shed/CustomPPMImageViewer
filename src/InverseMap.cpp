#include <BilinearLerper.h>
#include <InverseMap.h>
#include <ThreadPool.h>
#include <algorithm>
#include <vector>

namespace {

// A band smaller than this is not worth a task of its own: waking a pool
// worker costs about as much as resampling a few short rows.
constexpr int kMinRowsPerWorker = 32;

unsigned WorkersForRows(int screenHeight, unsigned requestedWorkers) {
  const int affordable = screenHeight / kMinRowsPerWorker;
  if (affordable < 2 || requestedWorkers < 2)
    return 1;
  return std::min(requestedWorkers, static_cast<unsigned>(affordable));
}

} // namespace

void InverseMap::RenderRows(const Image &source, ViewState vState,
                            Pixel *screenBuf, int screenWidth, int rowBegin,
                            int rowEnd) {
  const int imgW = source.imageHeader.width;
  const int imgH = source.imageHeader.height;
  const bool validImage =
      imgW > 0 && imgH > 0 &&
      source.pixelData.size() >=
          static_cast<size_t>(imgW) * static_cast<size_t>(imgH);

  BilinearLerper lerper;
  for (int y = rowBegin; y < rowEnd; y++) {
    for (int x = 0; x < screenWidth; x++) {
      const float u = (x - vState.offsetX) / vState.zoom;
      const float v = (y - vState.offsetY) / vState.zoom;
      if (validImage && u >= 0.0f && u < static_cast<float>(imgW) &&
          v >= 0.0f && v < static_cast<float>(imgH)) {
        screenBuf[y * screenWidth + x] = lerper.SampleBilinear(&source, u, v);
      } else {
        screenBuf[y * screenWidth + x] = {0, 0, 0};
      }
    }
  }
}

void InverseMap::ApplyInverseMap(const Image &source, ViewState vState,
                                 Pixel *screenBuf, int screenWidth,
                                 int screenHeight, unsigned workerCount) {
  const unsigned workers = WorkersForRows(screenHeight, workerCount);
  if (workers < 2) {
    RenderRows(source, vState, screenBuf, screenWidth, 0, screenHeight);
    return;
  }

  // Thread safety without locks: worker i owns screen rows
  // [height*i/workers, height*(i+1)/workers). The bands are contiguous and
  // disjoint, so each worker writes only its own slice of screenBuf, and the
  // source image and view state are only read. The 64-bit product keeps the
  // band boundary from overflowing int on very tall frames.
  const auto bandStart = [&](unsigned band) {
    return static_cast<int>(static_cast<long long>(screenHeight) * band /
                            workers);
  };

  // The pool also runs tasks on the calling thread, and Run() returns only
  // after every band is rendered, so the captured locals outlive the tasks.
  ThreadPool::Shared().Run(workers, [&](std::size_t band) {
    RenderRows(source, vState, screenBuf, screenWidth, bandStart(band),
               bandStart(band + 1));
  });
}
