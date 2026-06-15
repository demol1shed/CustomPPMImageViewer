#pragma once
// ---------------------------------------------------------------------------
// Minimal, zero-dependency unit-test framework.
//
// The project rule (CLAUDE.md §6.5) forbids external libraries, so instead of
// pulling in GoogleTest/Catch2 we provide a tiny header-only harness:
//   * TEST(suite, name) { ... }      -- defines & auto-registers a test case
//   * CHECK_TRUE / CHECK_EQ / ...    -- non-fatal assertions (a failed check
//                                       marks the test failed but keeps going)
//   * testing::runAll()              -- runs every registered case, prints a
//                                       summary, returns 0 on success else 1
//
// A test "fails" if any CHECK_* inside it reports a failure. The process exit
// code mirrors the result, so `make test` is a reliable autonomous gate.
// ---------------------------------------------------------------------------
#include <cstdio>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

namespace testing {

struct TestCase {
  std::string name;
  std::function<void()> fn;
};

// Single global registry, populated at static-init time by the Registrar below.
inline std::vector<TestCase> &registry() {
  static std::vector<TestCase> tests;
  return tests;
}

// Running tally of failed checks. Compared before/after each test to decide
// whether that individual case passed.
inline int &failureCount() {
  static int failures = 0;
  return failures;
}

// Registering a TestCase at construction time lets the TEST() macro add cases
// without an explicit registration list.
struct Registrar {
  Registrar(const std::string &name, std::function<void()> fn) {
    registry().push_back({name, std::move(fn)});
  }
};

inline void reportFailure(const char *file, int line, const std::string &msg) {
  failureCount()++;
  std::fprintf(stderr, "    [CHECK FAILED] %s:%d: %s\n", file, line,
               msg.c_str());
}

// Stringify integral values as decimal (so uint8_t prints as a number, not a
// raw character); everything else degrades to a placeholder.
template <typename T> std::string toStr(const T &value) {
  if constexpr (std::is_integral_v<T>) {
    return std::to_string(static_cast<long long>(value));
  } else if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
    return value;
  } else {
    return std::string("<value>");
  }
}

// Compare two values; for integral pairs cast to a wide signed type so that
// mixed signed/unsigned comparisons never trip -Wsign-compare.
template <typename A, typename B> bool valuesEqual(const A &a, const B &b) {
  if constexpr (std::is_integral_v<A> && std::is_integral_v<B>) {
    return static_cast<long long>(a) == static_cast<long long>(b);
  } else {
    return a == b;
  }
}

// Pixel-like comparison kept as a template so this header stays decoupled from
// Pixel.h (any struct exposing .r/.g/.b works).
template <typename P>
void checkPixel(const char *file, int line, const P &p, int er, int eg,
                int eb) {
  if (static_cast<int>(p.r) != er || static_cast<int>(p.g) != eg ||
      static_cast<int>(p.b) != eb) {
    reportFailure(file, line,
                  "pixel expected=(" + std::to_string(er) + "," +
                      std::to_string(eg) + "," + std::to_string(eb) +
                      ") actual=(" + std::to_string(static_cast<int>(p.r)) +
                      "," + std::to_string(static_cast<int>(p.g)) + "," +
                      std::to_string(static_cast<int>(p.b)) + ")");
  }
}

inline int runAll() {
  int passed = 0;
  int failed = 0;
  for (auto &tc : registry()) {
    const int before = failureCount();
    std::printf("[ RUN  ] %s\n", tc.name.c_str());
    tc.fn();
    if (failureCount() == before) {
      std::printf("[  OK  ] %s\n", tc.name.c_str());
      passed++;
    } else {
      std::printf("[ FAIL ] %s\n", tc.name.c_str());
      failed++;
    }
  }
  std::printf("\n==================================================\n");
  std::printf(" %d passed, %d failed (%zu total)\n", passed, failed,
              registry().size());
  std::printf("==================================================\n");
  return failed == 0 ? 0 : 1;
}

} // namespace testing

// --- Public macros ---------------------------------------------------------

// Defines a test body and registers it. Usage: TEST(Parser, ReadsHeader) { ... }
#define TEST(suite, name)                                                      \
  static void suite##_##name##_body();                                         \
  static ::testing::Registrar suite##_##name##_registrar(                      \
      #suite "." #name, suite##_##name##_body);                               \
  static void suite##_##name##_body()

#define CHECK_TRUE(cond)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                            \
      ::testing::reportFailure(__FILE__, __LINE__,                            \
                               "CHECK_TRUE failed: " #cond);                  \
    }                                                                          \
  } while (0)

#define CHECK_FALSE(cond)                                                      \
  do {                                                                         \
    if ((cond)) {                                                             \
      ::testing::reportFailure(__FILE__, __LINE__,                            \
                               "CHECK_FALSE failed: " #cond);                 \
    }                                                                          \
  } while (0)

#define CHECK_EQ(expected, actual)                                            \
  do {                                                                         \
    auto _check_e = (expected);                                               \
    auto _check_a = (actual);                                                 \
    if (!::testing::valuesEqual(_check_e, _check_a)) {                        \
      ::testing::reportFailure(                                               \
          __FILE__, __LINE__,                                                 \
          std::string("CHECK_EQ(" #expected ", " #actual ") expected=") +    \
              ::testing::toStr(_check_e) + " actual=" +                       \
              ::testing::toStr(_check_a));                                    \
    }                                                                          \
  } while (0)

// Compares a Pixel-like value against expected r/g/b channels.
#define CHECK_PIXEL(px, r, g, b)                                               \
  ::testing::checkPixel(__FILE__, __LINE__, (px), (r), (g), (b))
