// Standalone microbenchmark for Parser::ParseFile load time across thread
// counts. A developer tool, NOT a CI gate: these are warm-cache, single-machine
// timings that swing with the CPU governor, turbo, and background load.
//
// It lives outside tests/ on purpose. The Makefile builds run_tests from
// $(wildcard tests/*.cpp); a second main() under tests/ would be linked into
// that binary and break `make test` with a duplicate-symbol error.
//
// How to read the numbers:
//   * P6 (binary) decode is one big read into a pre-sized buffer -> it is
//     I/O / memcpy bound. Parallel gains are modest and can even regress at
//     high thread counts on small files (thread-spawn overhead > work saved).
//   * P3 (ASCII) decode is CPU bound. The biggest single lever is the
//     single-thread parsing strategy (iostream >> vs from_chars); threading is
//     a secondary multiplier on top. Compare a P3 row's 1-thread time before
//     and after the P3 rewrite to see the serial win separately.
#include "Pixel.h"
#include <Parser.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kRepeats = 7;
constexpr int kSynthW = 1024;
constexpr int kSynthH = 1024;

bool fileExists(const std::string &path) {
  std::ifstream f(path);
  return f.good();
}

// Canonical fixture formula, shared with tests/SampleData.h.
void synthPixel(int i, int &r, int &g, int &b) {
  r = (16 * i) & 0xff;
  g = (255 - 16 * i) & 0xff;
  b = (8 * i) & 0xff;
}

// Deterministic kSynthW*kSynthH P6 (binary) fixture written to `path`.
void writeSynthP6(const std::string &path) {
  std::ofstream out(path, std::ios::binary);
  out << "P6\n" << kSynthW << " " << kSynthH << "\n255\n";
  const int n = kSynthW * kSynthH;
  std::vector<unsigned char> bytes;
  bytes.reserve(static_cast<size_t>(n) * 3);
  for (int i = 0; i < n; ++i) {
    int r, g, b;
    synthPixel(i, r, g, b);
    bytes.push_back(static_cast<unsigned char>(r));
    bytes.push_back(static_cast<unsigned char>(g));
    bytes.push_back(static_cast<unsigned char>(b));
  }
  out.write(reinterpret_cast<const char *>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

// Deterministic kSynthW*kSynthH P3 (ASCII) fixture written to `path`. Values
// are wrapped across lines so chunk boundaries land at varied offsets.
void writeSynthP3(const std::string &path) {
  std::ofstream out(path);
  out << "P3\n" << kSynthW << " " << kSynthH << "\n255\n";
  const int n = kSynthW * kSynthH;
  std::string buf;
  buf.reserve(static_cast<size_t>(n) * 12);
  for (int i = 0; i < n; ++i) {
    int r, g, b;
    synthPixel(i, r, g, b);
    buf += std::to_string(r);
    buf += ' ';
    buf += std::to_string(g);
    buf += ' ';
    buf += std::to_string(b);
    buf += ((i % 8) == 7) ? '\n' : ' ';
  }
  buf += '\n';
  out << buf;
}

struct Stat {
  double minMs = 0;
  double medianMs = 0;
  double maxMs = 0;
  std::uint64_t checksum = 0;
  bool ok = false;
};

Stat timeParse(const std::string &path, unsigned threads, int repeats) {
  Stat s;
  // Warm the page cache and confirm the file parses before timing.
  if (!Parser::ParseFile(path, threads, false))
    return s;

  std::vector<double> samples;
  samples.reserve(static_cast<size_t>(repeats));
  std::uint64_t checksum = 0;
  for (int k = 0; k < repeats; ++k) {
    const auto t0 = std::chrono::steady_clock::now();
    auto img = Parser::ParseFile(path, threads, false);
    const auto t1 = std::chrono::steady_clock::now();
    if (!img)
      return s;
    // Consume the buffer so -O2 cannot elide the decode.
    for (const Pixel &p : img->pixelData)
      checksum += static_cast<std::uint64_t>(p.r) + p.g + p.b;
    samples.push_back(
        std::chrono::duration<double, std::milli>(t1 - t0).count());
  }
  std::sort(samples.begin(), samples.end());
  s.minMs = samples.front();
  s.medianMs = samples[samples.size() / 2];
  s.maxMs = samples.back();
  s.checksum = checksum;
  s.ok = true;
  return s;
}

void benchFile(const std::string &path) {
  auto probe = Parser::ParseFile(path, 1, false);
  const std::string fmt = probe ? probe->imageHeader.ppmType : "???";

  const unsigned threadCounts[] = {1, 2, 4, 8, 16};
  double baseline = -1.0;
  std::uint64_t checksum = 0;
  bool haveChecksum = false;
  bool checksumConsistent = true;

  for (unsigned t : threadCounts) {
    const Stat s = timeParse(path, t, kRepeats);
    if (!s.ok) {
      std::printf("%-24s %-4s %7u   PARSE FAILED\n", path.c_str(), fmt.c_str(),
                  t);
      continue;
    }
    if (baseline < 0)
      baseline = s.minMs;
    if (!haveChecksum) {
      checksum = s.checksum;
      haveChecksum = true;
    } else if (s.checksum != checksum) {
      checksumConsistent = false;
    }
    std::printf("%-24s %-4s %7u  %9.2f  %10.2f  %7.2fx\n", path.c_str(),
                fmt.c_str(), t, s.minMs, s.medianMs, baseline / s.minMs);
  }
  if (haveChecksum)
    std::printf("    checksum=0x%llx%s\n",
                static_cast<unsigned long long>(checksum),
                checksumConsistent ? "" : "   [MISMATCH ACROSS THREADS!]");
  std::printf("\n");
}

} // namespace

int main(int argc, char **argv) {
  std::string p6 = (argc > 1) ? argv[1] : "test.ppm";
  std::string p3 = (argc > 2) ? argv[2] : "testascii.ppm";

  if (!fileExists(p6)) {
    p6 = "/tmp/bench_synth_p6.ppm";
    writeSynthP6(p6);
    std::printf("(synthesized %s: %dx%d P6)\n", p6.c_str(), kSynthW, kSynthH);
  }
  if (!fileExists(p3)) {
    p3 = "/tmp/bench_synth_p3.ppm";
    writeSynthP3(p3);
    std::printf("(synthesized %s: %dx%d P3)\n", p3.c_str(), kSynthW, kSynthH);
  }

  std::printf(
      "PPM Parser Benchmark  (steady_clock, warm cache, K=%d, min of K, -O2)\n",
      kRepeats);
  std::printf("hardware_concurrency = %u    | dev tool, not a CI gate\n\n",
              std::thread::hardware_concurrency());
  std::printf("%-24s %-4s %7s  %9s  %10s  %8s\n", "file", "fmt", "threads",
              "min(ms)", "median(ms)", "speedup");
  std::printf("------------------------ ---- -------  ---------  ----------  "
              "--------\n");

  benchFile(p6);
  benchFile(p3);
  return 0;
}
