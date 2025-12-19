# Testing Guide

This guide covers SafeTimer's testing infrastructure using the Unity framework.

---

## 🔬 Test Suite Overview

SafeTimer includes comprehensive unit tests:

- **63 unit tests** (55 core + 8 advance_period API)
- **96.30% code coverage**
- **Unity test framework**
- **Mock BSP** for hardware abstraction

---

## 📦 Quick Start

### Install Unity (One-Time Setup)

```bash
cd test

# Download Unity v2.5.2
cd unity
wget https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.5.2.tar.gz
tar -xzf v2.5.2.tar.gz
cp Unity-2.5.2/src/unity.* .

cd ..
```

### Run Tests

```bash
# Build and run all tests
make test

# Run tests with verbose output
make test-verbose

# Run specific test
make test-single TEST=test_create_valid_timer
```

### Generate Coverage Report

```bash
make coverage
```

---

## 📊 Test Categories

### Basic Tests (14 tests)
- Timer lifecycle (create, start, stop, delete)
- State management
- Expiration timing
- Handle validation

### Callback Tests (7 tests)
- Execution correctness
- User data passing
- Critical section safety
- Multiple callbacks

### Edge Case Tests (16 tests)
- Boundary conditions
- Invalid parameters
- Rapid create/delete operations
- Timer pool exhaustion

### Stress Tests (8 tests)
- 1000+ create/delete cycles without leaks
- All timers active simultaneously
- 1000 rapid `safetimer_process()` calls
- 10-day long-running simulation (864,000 callbacks)
- Pool fragmentation resistance
- Memory leak detection (500 iterations)
- 32-bit wraparound boundary testing
- Multi-timer accuracy over 60 seconds

### Advance Period API Tests (8 tests, v1.3.1+)
- Phase-locked period advancement
- Zero cumulative error validation
- Catch-up logic for delayed execution
- 32-bit overflow handling
- Regression testing

---

## 🛠️ Writing Custom Tests

### Test File Structure

```c
#include "unity.h"
#include "safetimer.h"
#include "mock_bsp.h"

/* Test implementation */
void test_my_feature(void) {
    /* Arrange */
    mock_bsp_set_ticks(0);
    safetimer_handle_t h = safetimer_create(100, TIMER_MODE_ONE_SHOT, NULL, NULL);

    /* Act */
    safetimer_start(h);
    mock_bsp_set_ticks(100);
    safetimer_process();

    /* Assert */
    TEST_ASSERT_EQUAL(expected, actual);

    /* Cleanup */
    safetimer_delete(h);
}
```

### Adding Tests to Build System

1. Create test file: `test/test_my_feature.c`
2. Add to `test/Makefile`:
   ```makefile
   TEST_IMPL_SRCS = test_safetimer_basic.c \
                    ...
                    test_my_feature.c
   ```
3. Add to `test/test_runner_all.c`:
   ```c
   extern void test_my_feature(void);
   ...
   RUN_TEST(test_my_feature);
   ```

---

## 🧪 Mock BSP API

### mock_bsp_set_ticks(tick)

Set current tick value (simulates time passage):

```c
mock_bsp_set_ticks(0);      /* Start at 0ms */
mock_bsp_set_ticks(1000);   /* Jump to 1000ms */
```

### mock_bsp_reset()

Reset BSP state (call in `setUp()`):

```c
void setUp(void) {
    mock_bsp_reset();
    safetimer_test_reset_pool();
}
```

### Critical Section Tracking

Mock BSP automatically tracks critical section balance:

```c
/* This will fail if critical sections are unbalanced */
TEST_ASSERT_EQUAL(0, mock_bsp_get_critical_nesting());
```

---

## 📈 Coverage Analysis

### View Coverage Report

```bash
make coverage
less coverage/safetimer.c.gcov
```

### Coverage Interpretation

```
   100:  123:    if (handle >= MAX_TIMERS) {
    ##:  124:        return TIMER_ERR_INVALID;  /* ← Not covered */
```

- `100`: Line executed 100 times
- `##`: Line never executed (uncovered)
- `-`: Line not executable (comments, declarations)

### Target Coverage

- **Target:** ≥95%
- **Current:** 96.30%
- **Critical paths:** 100% branch coverage

---

## 🚀 Continuous Integration

SafeTimer uses GitHub Actions for automated testing:

```yaml
# .github/workflows/test.yml
- name: Run Tests
  run: |
    cd test
    make test
    make coverage
```

**CI Checks:**
- ✅ All tests pass
- ✅ Coverage ≥95%
- ✅ Static analysis clean (cppcheck)
- ✅ Build succeeds on multiple platforms

---

## 🔍 Static Analysis

### Run cppcheck

```bash
make static-analysis
```

**Checks:**
- Memory leaks
- Null pointer dereferences
- Buffer overflows
- Uninitialized variables
- MISRA-C compliance

---

## 📖 Next Steps

- **API Reference:** [../docs/api_reference.md](../docs/api_reference.md)
- **Contributing Tests:** [../CONTRIBUTING.md](../CONTRIBUTING.md)
- **CI Configuration:** [../.github/workflows/test.yml](../.github/workflows/test.yml)

---

**Test Coverage:** ~96.30% (target: ≥95%)
