# SafeTimer Testing Guide

## Overview

SafeTimer includes comprehensive unit tests using the Unity test framework and Mock BSP for deterministic testing on PC.

## Quick Start

### 1. Install Unity Framework

First, install Unity test framework (see `unity/README.md` for details):

```bash
cd test/unity
wget https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.5.2.tar.gz
tar -xzf v2.5.2.tar.gz
cp Unity-2.5.2/src/unity.c Unity-2.5.2/src/unity.h Unity-2.5.2/src/unity_internals.h .
```

### 2. Run Tests

```bash
cd test
make test
```

Expected output:
```
========== Running SafeTimer Tests ==========
test_safetimer_basic.c:29:test_create_valid_timer_one_shot:PASS
test_safetimer_basic.c:37:test_create_valid_timer_repeat:PASS
...
-----------------------
15 Tests 0 Failures 0 Ignored
OK
========== Tests Complete ==========
```

### 3. Generate Coverage Report

```bash
make coverage
```

View detailed coverage:
```bash
less coverage/safetimer.c.gcov
```

## Test Structure

```
test/
├── Makefile                    # Build system
├── test_safetimer_basic.c      # Basic functionality tests
├── mocks/
│   ├── mock_bsp.h              # Mock BSP interface
│   └── mock_bsp.c              # Mock BSP implementation
└── unity/
    ├── README.md               # Unity installation guide
    ├── unity.c                 # Unity framework
    ├── unity.h
    └── unity_internals.h
```

## Writing New Tests

### Test File Template

```c
#include "unity.h"
#include "safetimer.h"
#include "mock_bsp.h"

void setUp(void)
{
    mock_bsp_reset();  /* Reset before each test */
}

void tearDown(void)
{
    /* Verify critical section balance */
    TEST_ASSERT_EQUAL_INT(0, mock_bsp_get_critical_nesting());
}

void test_my_new_test(void)
{
    safetimer_handle_t handle;

    /* Arrange */
    handle = safetimer_create(1000, TIMER_MODE_ONE_SHOT, NULL, NULL);

    /* Act */
    safetimer_start(handle);
    mock_bsp_advance_time(500);
    safetimer_process();

    /* Assert */
    uint32_t remaining;
    safetimer_get_remaining(handle, &remaining);
    TEST_ASSERT_TRUE(remaining > 0);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_my_new_test);
    return UNITY_END();
}
```

### Add Test to Makefile

Edit `Makefile` and add your test file:

```makefile
TEST_SRCS = test_safetimer_basic.c test_my_new_test.c
```

## Mock BSP Features

### Time Control

```c
mock_bsp_set_ticks(1000);      /* Set absolute time */
mock_bsp_advance_time(500);    /* Advance by 500ms */
```

### Critical Section Validation

```c
mock_bsp_enable_validation(1); /* Detect unbalanced critical sections */
int nesting = mock_bsp_get_critical_nesting();
```

### Statistics

```c
mock_bsp_stats_t stats;
mock_bsp_get_stats(&stats);
printf("get_ticks called %lu times\n", stats.get_ticks_count);
```

## Testing Checklist

### Basic Functionality
- [x] Timer creation (valid/invalid parameters)
- [x] Timer start/stop
- [x] Timer deletion
- [x] Pool exhaustion
- [x] Slot reuse

### Timer Processing
- [x] Timer expiration (one-shot)
- [x] Timer expiration (repeat)
- [x] Multiple timers
- [ ] Callback execution (TODO: needs callback tests)

### Edge Cases
- [x] 32-bit wraparound (ADR-005)
- [ ] Maximum period (2^31-1 ms)
- [ ] Minimum period (1 ms)
- [ ] Callback reentrancy protection (TODO)

### Concurrency
- [ ] Critical section balance
- [ ] Interrupt safety (TODO: needs ISR tests)

## Coverage Goals

Target coverage metrics:
- **Line Coverage**: ≥ 95%
- **Branch Coverage**: ≥ 90%
- **Function Coverage**: 100%

Current coverage:
```bash
make coverage
grep "Lines executed" coverage/safetimer.c.gcov
```

## Static Analysis

Run cppcheck:
```bash
make static-analysis
```

Expected: 0 errors, 0 warnings for production code.

## Continuous Integration

### GitHub Actions (Planned)

```yaml
# .github/workflows/test.yml
name: SafeTimer Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Install Unity
        run: cd test && make check-unity
      - name: Run Tests
        run: cd test && make test
      - name: Coverage Report
        run: cd test && make coverage
```

## Troubleshooting

### "Unity framework not found"

Run: `make check-unity` to verify Unity installation.

### "undefined reference to unity_*"

Ensure Unity source files are in `test/unity/`:
- unity.c
- unity.h
- unity_internals.h

### Tests fail with "Critical section nesting error"

Check for unbalanced `bsp_enter_critical()`/`bsp_exit_critical()` calls.

## References

- Unity Documentation: https://github.com/ThrowTheSwitch/Unity
- SafeTimer Architecture: `docs/architecture.md`
- BSP Interface: `include/bsp.h`
