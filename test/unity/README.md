# Unity Test Framework Integration

## Overview

SafeTimer uses Unity 2.5.x for unit testing. Unity is a lightweight C testing framework ideal for embedded systems.

## Installation

### Method 1: Manual Download (Recommended)

1. Download Unity 2.5.2 from GitHub:
   ```bash
   wget https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.5.2.tar.gz
   ```

2. Extract to `test/unity/`:
   ```bash
   tar -xzf v2.5.2.tar.gz
   mv Unity-2.5.2/src/* test/unity/
   ```

3. Required files (copy to `test/unity/`):
   - `unity.c`
   - `unity.h`
   - `unity_internals.h`

### Method 2: Git Submodule

```bash
cd SafeTimer
git submodule add https://github.com/ThrowTheSwitch/Unity.git test/unity
git submodule update --init --recursive
```

## Required Files

After installation, `test/unity/` should contain:

```
test/unity/
├── unity.c              # Core Unity implementation
├── unity.h              # Public API
└── unity_internals.h    # Internal definitions
```

## Unity API Quick Reference

### Basic Assertions

```c
TEST_ASSERT_EQUAL(expected, actual)
TEST_ASSERT_NOT_EQUAL(expected, actual)
TEST_ASSERT_TRUE(condition)
TEST_ASSERT_FALSE(condition)
TEST_ASSERT_NULL(pointer)
TEST_ASSERT_NOT_NULL(pointer)
```

### Integer Assertions

```c
TEST_ASSERT_EQUAL_INT(expected, actual)
TEST_ASSERT_EQUAL_INT8(expected, actual)
TEST_ASSERT_EQUAL_INT16(expected, actual)
TEST_ASSERT_EQUAL_INT32(expected, actual)
TEST_ASSERT_EQUAL_UINT32(expected, actual)
```

### Test Structure

```c
#include "unity.h"

void setUp(void)
{
    /* Called before each test */
}

void tearDown(void)
{
    /* Called after each test */
}

void test_example(void)
{
    TEST_ASSERT_EQUAL(1, 1);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_example);
    return UNITY_END();
}
```

## Compilation

Unity requires C99 or later. Compile with:

```bash
gcc -std=c99 -Itest/unity test/unity/unity.c test_*.c -o test_runner
```

## Documentation

Full Unity documentation: https://github.com/ThrowTheSwitch/Unity

## License

Unity is MIT licensed, compatible with SafeTimer's MIT license.
