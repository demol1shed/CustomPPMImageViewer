CXX ?= c++
CXXFLAGS = -I include -Wall -std=c++17 -g
LDFLAGS = -lSDL2

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

# Headless end-to-end smoke check: run the real executable on each sample with
# SDL's dummy video driver (no display needed) and -v, under a timeout safety
# cap. We assert two things: the process did not crash (a signal exit, >=128,
# e.g. a segfault) and its verbose output reports the expected parsed
# dimensions. This works whether the dummy driver exits cleanly (code 0) or a
# real display makes the app block until the timeout (code 124).
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
	if [ $$fail -eq 0 ]; then echo "Smoke test passed."; \
	else echo "Smoke test failed."; exit 1; fi

clean:
	rm -f src/*.o $(TARGET) $(TEST_TARGET)

.PHONY: all test smoke clean
