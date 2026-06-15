// Entry point for the unit-test runner. Every test case registers itself via
// the TEST() macro (see TestFramework.h), so all we do here is run them and
// forward the aggregate pass/fail result as the process exit code.
#include "TestFramework.h"

int main() { return ::testing::runAll(); }
