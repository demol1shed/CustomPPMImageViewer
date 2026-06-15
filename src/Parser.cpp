#include "Pixel.h"
#include <Parser.h>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr uint MAX_THREAD_COUNT = 64;
constexpr uint MIN_THREAD_COUNT = 1;

// Below this row count the threading overhead outweighs the gain; read serially.
constexpr int MIN_ROWS_FOR_THREADING = 2;

// The C-locale whitespace set that the old `operator>>` skipped; kept explicit
// so ASCII (P3) parsing stays locale-independent (matching std::from_chars).
inline bool IsWhitespace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
         c == '\f';
}

// Clamp a user-requested thread count to a sane, useful value: at least 1, no
// more than the detected core count, never above MAX_THREAD_COUNT, and never
// more than the number of rows (extra workers would only get empty ranges).
// hardware_concurrency() may return 0 if undetectable; in that case we leave
// the core cap off and let ParseFile decide (it stays serial when hw < 2).
unsigned EffectiveThreadCount(uint requested, int height) {
  unsigned hw = std::thread::hardware_concurrency();
  unsigned eff = requested < MIN_THREAD_COUNT ? MIN_THREAD_COUNT : requested;
  if (eff > MAX_THREAD_COUNT)
    eff = MAX_THREAD_COUNT;
  if (hw > 0 && eff > hw)
    eff = hw;
  if (height > 0 && eff > static_cast<unsigned>(height))
    eff = static_cast<unsigned>(height);
  return eff < 1 ? 1 : eff;
}

// Worker body: read one row range [startRow, endRow) of a P6 file into the
// matching, disjoint slice of the pixel buffer. Runs on its own std::thread.
//
// Thread-safety (no mutex needed): each worker opens its OWN ifstream (an
// independent file position) and writes ONLY pixelData[startRow*width ..
// endRow*width). ComputeRowRanges guarantees the row ranges never overlap, and
// the buffer is sized before any worker starts and is not reallocated while
// they run, so no two threads ever touch the same memory.
//
// All byte/offset arithmetic uses wide signed types (streamoff/streamsize/
// ptrdiff_t) so startRow*width*3 cannot overflow int on large images. *okFlag
// is this worker's own byte (callers use std::vector<std::uint8_t>, never
// std::vector<bool>, so flags don't share a word) and is read by the caller
// only after join().
void ReadP6Range(const std::string &filePath, std::streamoff dataStart,
                 int width, int startRow, int endRow, Pixel *base,
                 std::uint8_t *okFlag) {
  *okFlag = 0; // pessimistic until the full range is read
  try {
    const int rows = endRow - startRow;
    if (rows <= 0) {
      *okFlag = 1; // empty range: trivially done
      return;
    }

    std::ifstream in(filePath, std::ios::binary);
    if (!in)
      return;

    const std::streamoff rowBytes = static_cast<std::streamoff>(width) * 3;
    const std::streamoff offset =
        dataStart + static_cast<std::streamoff>(startRow) * rowBytes;
    in.seekg(offset, std::ios::beg);
    if (!in)
      return;

    const std::streamsize byteCount =
        static_cast<std::streamsize>(rows) * rowBytes;
    char *dst = reinterpret_cast<char *>(
        base + static_cast<std::ptrdiff_t>(startRow) * width);
    in.read(dst, byteCount);
    if (in.gcount() != byteCount)
      return; // short / truncated read

    *okFlag = 1; // full success
  } catch (...) {
    // Never let an exception escape a thread (that calls std::terminate);
    // failure is already encoded as *okFlag == 0.
  }
}
} // namespace

std::vector<RowRange> Parser::ComputeRowRanges(int height, int threadCount) {
  std::vector<RowRange> ranges;
  if (height <= 0)
    return ranges; // nothing to split
  if (threadCount < 1)
    threadCount = 1;

  ranges.reserve(threadCount);
  // Balanced split that distributes the remainder across the first workers.
  // Because endRow(i) == startRow(i+1) by construction, adjacent ranges
  // telescope: no gaps, no overlaps. startRow(0) == 0 and
  // endRow(threadCount-1) == height, so the union is exactly [0, height).
  for (int i = 0; i < threadCount; ++i) {
    int startRow = (i * height) / threadCount;
    int endRow = ((i + 1) * height) / threadCount;
    ranges.push_back({startRow, endRow});
  }
  return ranges;
}

std::optional<Image> Parser::ParseFile(const std::string &filePath,
                                       uint threadCount, bool verbose) {
  std::ifstream file(filePath, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "File at " << filePath << " could not be found or opened.\n";
    return std::nullopt;
  }

  Image image;
  std::string line;
  std::stringstream ss;

  auto GetNextValidLine = [&]() { // take all vars by reference
    while (std::getline(file, line)) {
      size_t commentPos = line.find("#");
      if (commentPos != std::string::npos) {
        line = line.substr(0, commentPos);
      }

      if (line.empty() ||
          line.find_first_not_of(" \t\r\n") == std::string::npos)
        continue;

      return true;
    }
    return false;
  };

  // getting ppm type
  if (!GetNextValidLine())
    return std::nullopt;
  ss.clear();
  ss.str(line);
  ss >> image.imageHeader.ppmType;

  // getting width and height
  int w = -1,
      h = -1; // do this to move until a valid width/height value is found
  while (w == -1 || h == -1) {
    if (ss.eof()) {
      if (!GetNextValidLine())
        return std::nullopt;
      ss.clear();
      ss.str(line);
    }
    if (w == -1)
      ss >> w;
    // width and height may be in differnt lines
    if (ss.eof() && h == -1) {
      if (!GetNextValidLine())
        return std::nullopt;
      ss.clear();
      ss.str(line);
    }
    if (h == -1)
      ss >> h;
  }

  image.imageHeader.width = w;
  image.imageHeader.height = h;

  // getting maxVal
  int maxValInt = 0;
  if (ss.eof()) {
    if (!GetNextValidLine())
      return std::nullopt;
    ss.clear();
    ss.str(line);
  }

  ss >> maxValInt;
  if (maxValInt > 255) {
    std::cerr << "Error: 16 bit ppms are not supported. MaxVal is " << maxValInt
              << "\n";
    return std::nullopt;
  }
  image.imageHeader.maxVal = static_cast<uint8_t>(maxValInt);
  if (verbose) {
    std::cout << "File pre-parsing has been completed.\nPPM image type: "
              << image.imageHeader.ppmType
              << "\nImage size: " << image.imageHeader.width << ", "
              << image.imageHeader.height << "\n"
              << "Color maximum value: " << maxValInt << "\n";
  }

  // parsing pixel data. Cast before multiplying so the product is computed in
  // size_t and cannot overflow int on very large images.
  size_t totalPixels =
      static_cast<size_t>(image.imageHeader.width) * image.imageHeader.height;
  image.pixelData.resize(totalPixels);

  if (image.imageHeader.ppmType == "P6") {
    const int width = image.imageHeader.width;
    const int height = image.imageHeader.height;
    const unsigned effectiveThreads = EffectiveThreadCount(threadCount, height);
    const unsigned hw = std::thread::hardware_concurrency();

    if (verbose && effectiveThreads != threadCount) {
      std::cout << "Note: requested " << threadCount << " thread(s), using "
                << effectiveThreads << " (clamped to cores/rows/limits).\n";
    }

    // Parallelize only when worthwhile and safe: more than one usable worker,
    // at least two reported cores (hw == 0 means unknown -> stay serial), and
    // enough rows to be worth splitting.
    const bool parallel = effectiveThreads >= 2 && hw >= 2 &&
                          height >= MIN_ROWS_FOR_THREADING;

    if (parallel) {
      // The header getline loop left the file cursor exactly at the first
      // pixel byte; workers seek relative to this offset.
      const std::streamoff dataStart = file.tellg();

      std::vector<RowRange> ranges = ComputeRowRanges(height, effectiveThreads);
      // One success byte per worker. MUST be std::uint8_t, not
      // std::vector<bool>: bit-packing would make neighbouring workers write
      // the same word and race. Read only after join() (happens-before).
      std::vector<std::uint8_t> ok(ranges.size(), 0);
      std::vector<std::thread> workers;
      workers.reserve(ranges.size());

      for (size_t i = 0; i < ranges.size(); ++i) {
        if (ranges[i].startRow >= ranges[i].endRow) {
          ok[i] = 1; // empty range (more workers than rows): nothing to read
          continue;
        }
        // filePath is decay-copied into the thread -> no dangling reference.
        workers.emplace_back(ReadP6Range, filePath, dataStart, width,
                             ranges[i].startRow, ranges[i].endRow,
                             image.pixelData.data(), &ok[i]);
      }
      for (std::thread &worker : workers)
        worker.join();

      for (std::uint8_t flag : ok) {
        if (!flag) {
          std::cerr << "Error: parallel P6 read failed (truncated file?).\n";
          return std::nullopt;
        }
      }

      if (verbose) {
        std::cout << "Binary P6 data read with " << effectiveThreads
                  << " threads.\n";
      }
    } else {
      // Serial fallback: one contiguous read from the current cursor.
      const std::streamsize want =
          static_cast<std::streamsize>(totalPixels * sizeof(Pixel));
      file.read(reinterpret_cast<char *>(image.pixelData.data()), want);
      if (file.gcount() != want) {
        std::cerr << "Error: P6 pixel data is truncated (" << file.gcount()
                  << " of " << want << " bytes).\n";
        return std::nullopt;
      }
      if (verbose) {
        std::cout << "Binary P6 data read (serial). Bytes: " << file.gcount()
                  << "\n";
      }
    }
  } else {
    // ASCII (P3): read the whole value region once, then parse the integers
    // with std::from_chars -- far faster and allocation-free vs per-value
    // operator>> extraction. Write straight into the packed pixel bytes:
    // pixelData is contiguous 3-byte Pixels, so value index v -> flat[v]
    // (r,g,b,r,g,b,...), channel grouping is automatic.
    const std::streamoff dataStart = file.tellg();
    file.seekg(0, std::ios::end);
    const std::streamoff endPos = file.tellg();
    file.seekg(dataStart, std::ios::beg);
    if (dataStart < 0 || endPos < dataStart) {
      std::cerr << "Error: could not size P3 pixel data.\n";
      return std::nullopt;
    }
    const size_t regionBytes = static_cast<size_t>(endPos - dataStart);
    std::vector<char> buf(regionBytes);
    file.read(buf.data(), static_cast<std::streamsize>(regionBytes));

    std::uint8_t *flat =
        reinterpret_cast<std::uint8_t *>(image.pixelData.data());
    const size_t needed = totalPixels * 3;
    const char *base = buf.data();
    const size_t n = buf.size();
    size_t p = 0;
    size_t v = 0;
    while (p < n && v < needed) {
      while (p < n && IsWhitespace(base[p]))
        ++p;
      if (p >= n)
        break;
      const char *tokBegin = base + p;
      while (p < n && !IsWhitespace(base[p]))
        ++p;
      int value = 0;
      auto res = std::from_chars(tokBegin, base + p, value);
      if (res.ec != std::errc{} || res.ptr != base + p) {
        std::cerr << "Error: malformed P3 pixel value.\n";
        return std::nullopt;
      }
      flat[v++] = static_cast<std::uint8_t>(value);
    }
    // Strict: exactly width*height*3 values, with nothing but whitespace left.
    while (p < n && IsWhitespace(base[p]))
      ++p;
    if (v != needed || p != n) {
      std::cerr << "Error: P3 value count does not match header.\n";
      return std::nullopt;
    }

    if (verbose)
      std::cout << "ASCII P3 data read.\n";
  }

  return image;
}
