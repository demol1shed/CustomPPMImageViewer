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

// Sentinel returned by WalkP3Tokens (parse mode) on a non-numeric token.
constexpr std::size_t kWalkError = static_cast<std::size_t>(-1);

// Single source of the P3 token walk, shared by counting (pass 1) and parsing
// (pass 2) so the two passes can never disagree on which tokens a window owns.
//
// Walks the whitespace-separated tokens of base[0,N) that are OWNED by the byte
// window [cs,ce) -- a token (maximal non-whitespace run) belongs to the window
// holding its FIRST byte. If `out` is null, just counts. Otherwise from_chars
// each owned token and writes the byte to out[outStart + k] for the k-th owned
// token. Returns the owned-token count, or kWalkError on a non-numeric /
// out-of-range token (parse mode only).
std::size_t WalkP3Tokens(const char *base, std::size_t N, std::size_t cs,
                         std::size_t ce, std::uint8_t *out,
                         std::size_t outStart) {
  std::size_t p = cs;
  // If the window starts mid-token, that token's first byte is in an earlier
  // window (which owns and finishes it) -> skip to the next token boundary.
  if (p > 0 && p < N && !IsWhitespace(base[p - 1])) {
    while (p < N && !IsWhitespace(base[p]))
      ++p;
  }

  std::size_t count = 0;
  while (true) {
    while (p < N && IsWhitespace(base[p]))
      ++p;
    if (p >= N || p >= ce) // next token-start belongs to a later window
      break;
    const char *tokBegin = base + p;
    while (p < N && !IsWhitespace(base[p])) // advance past token end (may pass ce)
      ++p;
    if (out) {
      int value = 0;
      const std::from_chars_result res =
          std::from_chars(tokBegin, base + p, value);
      if (res.ec != std::errc{} || res.ptr != base + p)
        return kWalkError;
      out[outStart + count] = static_cast<std::uint8_t>(value);
    }
    ++count;
  }
  return count;
}

// P3 pass-1 worker: count the tokens owned by [cs,ce) into *outCount. Operates
// on the shared read-only buffer; never lets an exception cross the thread.
void CountP3Range(const char *base, std::size_t N, std::size_t cs,
                  std::size_t ce, std::size_t *outCount, std::uint8_t *okFlag) {
  *okFlag = 0;
  try {
    *outCount = WalkP3Tokens(base, N, cs, ce, nullptr, 0);
    *okFlag = 1;
  } catch (...) {
  }
}

// P3 pass-2 worker: parse the tokens owned by [cs,ce) and write them into the
// disjoint slice flat[startValueIndex ..). Fails (okFlag stays 0) on a
// non-numeric token, or if it did not write exactly expectedCount values (a
// guard against any pass-1/pass-2 walk divergence). No mutex: the buffer is
// read-only and each worker writes a disjoint slice.
void ParseP3Range(const char *base, std::size_t N, std::size_t cs,
                  std::size_t ce, std::size_t startValueIndex,
                  std::size_t expectedCount, std::uint8_t *flat,
                  std::uint8_t *okFlag) {
  *okFlag = 0;
  try {
    const std::size_t got = WalkP3Tokens(base, N, cs, ce, flat, startValueIndex);
    if (got == kWalkError || got != expectedCount)
      return;
    *okFlag = 1;
  } catch (...) {
  }
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

std::vector<ByteRange> Parser::ComputeByteRanges(std::size_t totalBytes,
                                                 int threadCount) {
  std::vector<ByteRange> ranges;
  if (threadCount < 1)
    threadCount = 1;
  ranges.reserve(static_cast<std::size_t>(threadCount));
  // Balanced byte split; cast i to size_t before i*totalBytes so the product
  // is computed in size_t (totalBytes can be large for a big P3 file).
  for (int i = 0; i < threadCount; ++i) {
    std::size_t start = (static_cast<std::size_t>(i) * totalBytes) / threadCount;
    std::size_t end =
        (static_cast<std::size_t>(i + 1) * totalBytes) / threadCount;
    ranges.push_back({start, end});
  }
  return ranges;
}

std::size_t Parser::CountTokensInRange(const char *base, std::size_t totalBytes,
                                       std::size_t chunkStart,
                                       std::size_t chunkEnd) {
  return WalkP3Tokens(base, totalBytes, chunkStart, chunkEnd, nullptr, 0);
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

    const int height = image.imageHeader.height;
    const unsigned effectiveThreads = EffectiveThreadCount(threadCount, height);
    const unsigned hw = std::thread::hardware_concurrency();
    if (verbose && effectiveThreads != threadCount) {
      std::cout << "Note: requested " << threadCount << " thread(s), using "
                << effectiveThreads << " (clamped to cores/rows/limits).\n";
    }
    const bool parallel = effectiveThreads >= 2 && hw >= 2 &&
                          height >= MIN_ROWS_FOR_THREADING;

    if (parallel) {
      // Split the value region into byte windows; workers share the read-only
      // buf and write disjoint slices of flat, so no mutex is needed.
      std::vector<ByteRange> ranges = ComputeByteRanges(n, effectiveThreads);

      // Pass 1: count the tokens each window owns.
      std::vector<std::size_t> counts(ranges.size(), 0);
      std::vector<std::uint8_t> ok1(ranges.size(), 0);
      {
        std::vector<std::thread> workers;
        workers.reserve(ranges.size());
        for (size_t i = 0; i < ranges.size(); ++i) {
          if (ranges[i].start >= ranges[i].end) {
            ok1[i] = 1; // empty window
            continue;
          }
          workers.emplace_back(CountP3Range, base, n, ranges[i].start,
                               ranges[i].end, &counts[i], &ok1[i]);
        }
        for (std::thread &worker : workers)
          worker.join();
      }
      for (std::uint8_t flag : ok1) {
        if (!flag) {
          std::cerr << "Error: P3 count pass failed.\n";
          return std::nullopt;
        }
      }

      // Exclusive prefix sum -> each window's first value index in flat.
      std::vector<std::size_t> starts(ranges.size(), 0);
      std::size_t total = 0;
      for (size_t i = 0; i < ranges.size(); ++i) {
        starts[i] = total;
        total += counts[i];
      }
      if (total != needed) {
        std::cerr << "Error: P3 value count does not match header.\n";
        return std::nullopt;
      }

      // Pass 2: parse each window into its disjoint slice of flat.
      std::vector<std::uint8_t> ok2(ranges.size(), 0);
      {
        std::vector<std::thread> workers;
        workers.reserve(ranges.size());
        for (size_t i = 0; i < ranges.size(); ++i) {
          if (counts[i] == 0) {
            ok2[i] = 1; // nothing to write
            continue;
          }
          workers.emplace_back(ParseP3Range, base, n, ranges[i].start,
                               ranges[i].end, starts[i], counts[i], flat,
                               &ok2[i]);
        }
        for (std::thread &worker : workers)
          worker.join();
      }
      for (std::uint8_t flag : ok2) {
        if (!flag) {
          std::cerr << "Error: malformed P3 pixel value.\n";
          return std::nullopt;
        }
      }

      if (verbose)
        std::cout << "ASCII P3 data read with " << effectiveThreads
                  << " threads.\n";
    } else {
      // Serial fallback: one walk over the whole region (stop at `needed`, then
      // require only trailing whitespace -- strict count check).
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
        const std::from_chars_result res =
            std::from_chars(tokBegin, base + p, value);
        if (res.ec != std::errc{} || res.ptr != base + p) {
          std::cerr << "Error: malformed P3 pixel value.\n";
          return std::nullopt;
        }
        flat[v++] = static_cast<std::uint8_t>(value);
      }
      while (p < n && IsWhitespace(base[p]))
        ++p;
      if (v != needed || p != n) {
        std::cerr << "Error: P3 value count does not match header.\n";
        return std::nullopt;
      }

      if (verbose)
        std::cout << "ASCII P3 data read (serial).\n";
    }
  }

  return image;
}
