CXX ?= c++
CXXFLAGS = -I include -Wall -std=c++17 -g -pthread
LDFLAGS = -lSDL2 -pthread

SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:.cpp=.o)
TARGET = view

# Header dependency tracking: -MMD writes a src/<name>.d makefile fragment next
# to each object listing the headers it included, and -MP adds phony targets for
# those headers so deleting one does not break the build. Including the
# fragments below makes editing a .h rebuild exactly the objects that use it
# (previously stale objects survived and failed at link time).
DEPFLAGS = -MMD -MP
DEP = $(OBJ:.o=.d)
HEADERS = $(wildcard include/*.h)

# --- Test build ------------------------------------------------------------
# The unit-test runner links the algorithmic core (parser + scaling math)
# directly, with no SDL and no app entry point, so it stays headless and fast.
TEST_DIR    = tests
TEST_SRC    = $(wildcard $(TEST_DIR)/*.cpp)
TESTED_SRC  = src/Parser.cpp src/BilinearLerper.cpp src/InverseMap.cpp \
              src/ViewController.cpp src/ThreadPool.cpp
TEST_TARGET = run_tests

all: $(TARGET)
$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGET) $(LDFLAGS)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

-include $(DEP)

# Build and run the deterministic unit tests. Returns non-zero if any fail,
# so it doubles as a regression gate after touching parsing/interpolation.
test: $(TEST_TARGET)
	@./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_SRC) $(TESTED_SRC) $(HEADERS) $(wildcard $(TEST_DIR)/*.h)
	$(CXX) $(CXXFLAGS) -I $(TEST_DIR) $(TEST_SRC) $(TESTED_SRC) -o $(TEST_TARGET)

# Headless end-to-end smoke check: run the real executable with SDL's dummy
# video driver (no display needed), where the software renderer draws the
# frames. A healthy viewer stays in its event loop until timeout stops it (124),
# so only 0 and 124 pass: error exits and signals (>=128, including 137 when a
# hung process has to be killed) fail. The first pass also greps the verbose
# output to confirm header parsing; the next ones exercise several -t values
# and zoom levels; test.ppm runs if present.
smoke: $(TARGET)
	@echo "Running headless CLI smoke test (SDL dummy driver)..."
	@fail=0; \
	run() { \
	  out=$$(SDL_VIDEODRIVER=dummy timeout -k 3 2 ./$(TARGET) "$$@" </dev/null 2>&1); \
	  code=$$?; \
	  if [ $$code -eq 0 ] || [ $$code -eq 124 ]; then return 0; fi; \
	  echo "  [FAIL] $$* (exit $$code)"; echo "$$out" | tail -n 3; fail=1; return 1; \
	}; \
	for f in $(TEST_DIR)/samples/4x4.ppm $(TEST_DIR)/samples/4x4_p6.ppm; do \
	  run $$f -v || continue; \
	  if echo "$$out" | grep -q "Image size: 4, 4"; then \
	    echo "  [OK]   $$f (parsed 4x4)"; \
	  else \
	    echo "  [FAIL] $$f did not parse as expected"; fail=1; \
	  fi; \
	done; \
	for f in $(TEST_DIR)/samples/4x4_p6.ppm $(TEST_DIR)/samples/32x32_p6.ppm $(TEST_DIR)/samples/32x32.ppm; do \
	  for t in 1 4 16; do \
	    run $$f -t $$t && echo "  [OK]   $$f -t $$t"; \
	  done; \
	done; \
	for f in $(TEST_DIR)/samples/4x4_p6.ppm $(TEST_DIR)/samples/32x32_p6.ppm; do \
	  for z in 2.0 50; do \
	    run $$f -z $$z && echo "  [OK]   $$f -z $$z"; \
	  done; \
	done; \
	if [ -f test.ppm ]; then \
	  run test.ppm -t 16 && echo "  [OK]   test.ppm -t 16"; \
	fi; \
	if [ $$fail -eq 0 ]; then echo "Smoke test passed."; \
	else echo "Smoke test failed."; exit 1; fi

# Parser load-time microbenchmark. Lives in bench/ (not tests/) so the
# run_tests wildcard never links its main(). Built at -O2 (not the project's
# default -g) so timings reflect release performance; links only src/Parser.cpp
# (no SDL, like run_tests). Dev tool, not a CI gate.
BENCH_SRC    = bench/bench_parser.cpp
BENCH_TARGET = bench_parser

bench: $(BENCH_SRC) src/Parser.cpp src/ThreadPool.cpp $(HEADERS)
	$(CXX) -I include -std=c++17 -O2 -pthread $(BENCH_SRC) src/Parser.cpp src/ThreadPool.cpp -o $(BENCH_TARGET)
	@./$(BENCH_TARGET)

clean:
	rm -f src/*.o src/*.d $(TARGET) $(TEST_TARGET) $(BENCH_TARGET)

.PHONY: all test smoke bench clean
