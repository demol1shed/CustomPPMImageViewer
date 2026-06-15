CXX ?= c++
CXXFLAGS = -I include -Wall -std=c++17 -g -pthread
LDFLAGS = -lSDL2 -pthread

SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:.cpp=.o)
TARGET = view

# --- Test build ------------------------------------------------------------
# The unit-test runner links the algorithmic core (parser + scaling math)
# directly, with no SDL and no app entry point, so it stays headless and fast.
TEST_DIR    = tests
TEST_SRC    = $(wildcard $(TEST_DIR)/*.cpp)
TESTED_SRC  = src/Parser.cpp src/BilinearLerper.cpp src/InverseMap.cpp
TEST_TARGET = run_tests

all: $(TARGET)
$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGET) $(LDFLAGS)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Build and run the deterministic unit tests. Returns non-zero if any fail,
# so it doubles as a regression gate after touching parsing/interpolation.
test: $(TEST_TARGET)
	@./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_SRC) $(TESTED_SRC)
	$(CXX) $(CXXFLAGS) -I $(TEST_DIR) $(TEST_SRC) $(TESTED_SRC) -o $(TEST_TARGET)

# Headless end-to-end smoke check: run the real executable with SDL's dummy
# video driver (no display needed), under a timeout safety cap. A signal exit
# (>=128, e.g. a segfault) is a failure; a clean exit (0) or a timeout (124,
# when a real display makes the app block) is fine. First pass also greps the
# verbose output to confirm header parsing; second pass exercises the threaded
# P6 path across several -t values; last pass runs the real test.ppm if present.
smoke: $(TARGET)
	@echo "Running headless CLI smoke test (SDL dummy driver)..."
	@fail=0; \
	for f in $(TEST_DIR)/samples/4x4.ppm $(TEST_DIR)/samples/4x4_p6.ppm; do \
	  out=$$(SDL_VIDEODRIVER=dummy timeout 5 ./$(TARGET) $$f -v 2>&1); \
	  code=$$?; \
	  if [ $$code -ge 128 ]; then \
	    echo "  [FAIL] $$f crashed with signal $$((code - 128))"; fail=1; \
	  elif echo "$$out" | grep -q "Image size: 4, 4"; then \
	    echo "  [OK]   $$f (no crash, parsed 4x4)"; \
	  else \
	    echo "  [FAIL] $$f did not parse as expected (exit $$code)"; fail=1; \
	  fi; \
	done; \
	for f in $(TEST_DIR)/samples/4x4_p6.ppm $(TEST_DIR)/samples/32x32_p6.ppm; do \
	  for t in 1 4 16; do \
	    SDL_VIDEODRIVER=dummy timeout 5 ./$(TARGET) $$f -t $$t >/dev/null 2>&1; \
	    code=$$?; \
	    if [ $$code -ge 128 ]; then \
	      echo "  [FAIL] $$f -t $$t crashed with signal $$((code - 128))"; fail=1; \
	    else \
	      echo "  [OK]   $$f -t $$t (no crash)"; \
	    fi; \
	  done; \
	done; \
	for f in $(TEST_DIR)/samples/4x4_p6.ppm $(TEST_DIR)/samples/32x32_p6.ppm; do \
	  for z in 2.0 50; do \
	    SDL_VIDEODRIVER=dummy timeout 5 ./$(TARGET) $$f -z $$z >/dev/null 2>&1; \
	    code=$$?; \
	    if [ $$code -ge 128 ]; then \
	      echo "  [FAIL] $$f -z $$z crashed with signal $$((code - 128))"; fail=1; \
	    else \
	      echo "  [OK]   $$f -z $$z (no crash)"; \
	    fi; \
	  done; \
	done; \
	if [ -f test.ppm ]; then \
	  SDL_VIDEODRIVER=dummy timeout 10 ./$(TARGET) test.ppm -t 16 >/dev/null 2>&1; \
	  code=$$?; \
	  if [ $$code -ge 128 ]; then \
	    echo "  [FAIL] test.ppm -t 16 crashed with signal $$((code - 128))"; fail=1; \
	  else \
	    echo "  [OK]   test.ppm -t 16 (no crash)"; \
	  fi; \
	fi; \
	if [ $$fail -eq 0 ]; then echo "Smoke test passed."; \
	else echo "Smoke test failed."; exit 1; fi

# Parser load-time microbenchmark. Lives in bench/ (not tests/) so the
# run_tests wildcard never links its main(). Built at -O2 (not the project's
# default -g) so timings reflect release performance; links only src/Parser.cpp
# (no SDL, like run_tests). Dev tool, not a CI gate.
BENCH_SRC    = bench/bench_parser.cpp
BENCH_TARGET = bench_parser

bench: $(BENCH_SRC) src/Parser.cpp
	$(CXX) -I include -std=c++17 -O2 -pthread $(BENCH_SRC) src/Parser.cpp -o $(BENCH_TARGET)
	@./$(BENCH_TARGET)

clean:
	rm -f src/*.o $(TARGET) $(TEST_TARGET) $(BENCH_TARGET)

.PHONY: all test smoke bench clean
