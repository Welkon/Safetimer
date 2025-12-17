# SafeTimer ⏱️

**Lightweight Embedded Timer Library for Resource-Constrained 8-bit MCUs**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.2.3-green.svg)]()
[![C99](https://img.shields.io/badge/C-C99-brightgreen.svg)]()
[![Test Coverage](https://img.shields.io/badge/coverage-96.30%25-brightgreen.svg)]()
[![Tests](https://img.shields.io/badge/tests-55%20passing-success.svg)]()

English | [简体中文](README_zh-CN.md)

---

## 🎯 Features

- **Minimal RAM Footprint:** Only 58 bytes for 4 concurrent timers (default)
- **Small Code Size:** ~0.8KB Flash (query APIs disabled) | ~1.0KB Flash (full featured)
- **Zero Dynamic Allocation:** No malloc/free, fully static memory
- **Overflow-Safe:** Handles 32-bit time wraparound automatically ([ADR-005](docs/architecture.md))
- **Portable:** 3-function BSP interface, works on any MCU
- **Flexible API:** Core API for explicit control + optional helpers for convenience (v1.1+)
- **Well-Tested:** 55 unit tests, 96.30% coverage
- **Production-Ready:** MISRA-C compliant, static analysis clean

---

## 📦 Quick Start

### 1. Installation

**🎯 Choose Your Integration Method:**

---

#### 📋 Method 1A: Single-File Version (Easiest) ⭐

**Best for:** Quick prototyping, simple projects, learning

```bash
# Only 2 files needed!
cp SafeTimer/single-file/safetimer_single.h your_project/
cp SafeTimer/single-file/safetimer_single.c your_project/
```

✅ Minimal setup, full core functionality
✅ All configuration in one header file
✅ [See single-file README](single-file/README.md) for details

---

#### 📋 Method 1B: Standard Version (Recommended)

Copy these files to your project directory:

```bash
# Step 1: Copy required files (4 files)
cp SafeTimer/include/safetimer.h your_project/
cp SafeTimer/include/safetimer_config.h your_project/
cp SafeTimer/include/bsp.h your_project/
cp SafeTimer/src/safetimer.c your_project/

# Step 2 (Optional): Copy helper API if needed
cp SafeTimer/include/safetimer_helpers.h your_project/
```

⚠️ **Do NOT copy** `src/safetimer_internal.h` (internal use only)

**File Summary:**
- ✅ **Required (4 files):** safetimer.h, safetimer_config.h, bsp.h, safetimer.c
- ✅ **Optional (1 file):** safetimer_helpers.h (convenience API, v1.1+)
- ❌ **Never copy:** safetimer_internal.h (internal implementation)

### 2. Implement BSP (3 Functions)

Create `safetimer_bsp.c` with these 3 functions:

> **💡 Naming Tip:** We recommend `safetimer_bsp.c` to avoid conflicts with other libraries. Alternatively, use `myapp_bsp.c` or place in a subdirectory like `bsp/safetimer.c`.

```c
#include "bsp.h"

static volatile bsp_tick_t s_ticks = 0;

/* Called by hardware timer interrupt every 1ms */
void timer_isr(void) {
    s_ticks++;
}

bsp_tick_t bsp_get_ticks(void) {
    return s_ticks;
}

void bsp_enter_critical(void) {
    EA = 0;  /* Disable interrupts */
}

void bsp_exit_critical(void) {
    EA = 1;  /* Enable interrupts */
}
```

See [`examples/`](examples/) for complete BSP implementations.

### 3. Use Timers

```c
#include "safetimer.h"

void led_callback(void *user_data) {
    toggle_led();  /* User code */
}

int main(void) {
    /* Initialize hardware timer (1ms tick) */
    init_timer0();

    /* Create a 1000ms repeating timer */
    safetimer_handle_t led_timer = safetimer_create(
        1000,                    /* period_ms */
        TIMER_MODE_REPEAT,       /* mode */
        led_callback,            /* callback */
        NULL                     /* user_data */
    );

    safetimer_start(led_timer);

    /* Main loop */
    while (1) {
        safetimer_process();  /* Process timers */
    }
}
```

**⚡ Simpler Alternative (v1.1+):**

For common immediate-start scenarios, use the optional helper API:

```c
#include "safetimer_helpers.h"  /* Optional convenience layer */

int main(void) {
    init_timer0();

    /* Create and start in one line (zero overhead) */
    safetimer_handle_t led_timer = safetimer_create_started(
        1000, TIMER_MODE_REPEAT, led_callback, NULL
    );

    if (led_timer == SAFETIMER_INVALID_HANDLE) {
        /* Handle error */
    }

    while (1) {
        safetimer_process();
    }
}
```

**When to Use Which API:**
- 📦 **Core API** (`safetimer.h`): Cascaded timers, conditional start
- ⚡ **Helper API** (`safetimer_helpers.h`): Immediate start (90% of use cases)

See [`examples/helpers_demo/`](examples/helpers_demo/) for detailed comparison.

---

## ⚙️ System Requirements

SafeTimer has minimal requirements and runs on almost any 8-bit MCU:

### Hardware Requirements
- **RAM:** Minimum 58 bytes (for 4 timers + internal state)
  - Each timer: 14 bytes
  - Overhead: 2 bytes
  - Additional: User stack space (~20-50 bytes recommended)
- **Flash/ROM:** Minimum 1.0 KB (with parameter checking enabled)
  - Optimized build: ~0.8 KB
- **Clock Source:** 1ms tick timer (hardware timer interrupt)
  - Any precision: ±1% typical, ±5% maximum acceptable

### Software Requirements
- **Compiler:** C99-compatible or C89 with `stdint.h`
  - Tested: SDCC 4.x, GCC 9+, Keil C51 9.x
- **Standard Library:** None required (optional `stddef.h` for helpers)
- **Interrupt Support:** Must support enable/disable interrupts

### MCU Requirements
- **Architecture:** Any (8-bit, 16-bit, 32-bit)
- **Endianness:** Any (little-endian or big-endian)
- **Timer:** Any hardware timer capable of 1ms periodic interrupt

### NOT Required
- ❌ RTOS
- ❌ Dynamic memory allocation (malloc/free)
- ❌ Complex HAL libraries
- ❌ Specific MCU vendor

---

## 📊 Resource Usage

**Default Configuration (4 timers):**
- **RAM:** 58 bytes (4 timers) to 114 bytes (8 timers)
- **Flash:** ~0.8 KB (minimal) to ~1.2 KB (full featured)
- **Processing:** ~5-10µs per `safetimer_process()` call on typical 8-bit MCUs

**Flash Configurations:**
- **Minimal (~0.8KB):** `ENABLE_QUERY_API=0` + `ENABLE_PARAM_CHECK=0` (production-optimized)
- **Full (~1.0KB):** `ENABLE_QUERY_API=1` + `ENABLE_PARAM_CHECK=1` (development/debugging)

**Scalability (configurable via MAX_TIMERS):**
- 4 timers (default) = 58 bytes RAM
- 8 timers = 114 bytes RAM
- 16 timers = 226 bytes RAM
- 32 timers = 450 bytes RAM

### Configuration Options

SafeTimer provides compile-time configuration for resource optimization:

**ENABLE_QUERY_API** (default: 0 - disabled)
```c
#define ENABLE_QUERY_API 1  /* Enable query/diagnostic APIs */
```
- **Disabled (0):** Saves ~200 bytes Flash (20% of library size)
  - Removes: `safetimer_stop()`, `safetimer_get_status()`, `safetimer_get_remaining()`, `safetimer_get_pool_usage()`
  - Best for: Production builds on resource-constrained MCUs with limited Flash
- **Enabled (1):** Full API available for debugging/diagnostics
  - Best for: Development, testing, dynamic timer management

**ENABLE_PARAM_CHECK** (default: 1 - enabled)
```c
#define ENABLE_PARAM_CHECK 0  /* Disable for production */
```
- **Enabled (1):** Safer, validates all API parameters (~150 bytes Flash)
- **Disabled (0):** Faster, minimal footprint (recommended for production)

**MAX_TIMERS** (default: 4)
```c
#define MAX_TIMERS 8  /* Increase timer pool */
```
- Controls RAM usage: `RAM = MAX_TIMERS × 14 + 2 bytes`

**BSP_TICK_TYPE_16BIT** (default: 0 - 32-bit)
```c
#define BSP_TICK_TYPE_16BIT 1  /* Ultra-low RAM mode */
```
- **32-bit (0):** Max period 24.8 days (standard)
- **16-bit (1):** Max period 65.5 seconds (saves ~20 bytes RAM)

See [safetimer_config.h](include/safetimer_config.h) for complete configuration guide.

---

## 🏗️ Architecture Highlights

### Safe 32-bit Overflow Handling (ADR-005)

SafeTimer uses a **signed difference comparison algorithm** to handle time wraparound automatically:

```c
/* Works correctly even when tick counter wraps from 2^32-1 to 0 */
if ((long)(current_tick - expire_time) >= 0) {
    /* Timer expired */
}
```

**Result:** No 49-day crash limit, infinite runtime.

**Limitation:** Single timer period ≤ 2³¹-1 ms (~24.8 days).

### Minimal BSP Interface

Only 3 functions needed:
- `bsp_get_ticks()` - Return milliseconds since boot
- `bsp_enter_critical()` - Disable interrupts
- `bsp_exit_critical()` - Enable interrupts

No complex HAL or RTOS dependencies!

---

## 📚 Documentation

| Document | Description |
|----------|-------------|
| [User Guide](docs/user_guide.md) | Quick start and common patterns |
| [API Reference](docs/api_reference.md) | Complete API documentation |
| [Porting Guide](docs/porting_guide.md) | BSP implementation guide |
| [Architecture](docs/architecture.md) | Design decisions (ADRs) |
| [Project Status](docs/project_status.md) | Current implementation status |
| [Epics & Stories](docs/epics_and_stories.md) | Development roadmap |

---

## 🔬 Testing

SafeTimer includes comprehensive unit tests using Unity framework:

```bash
cd test

# Install Unity (one-time)
cd unity && wget https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.5.2.tar.gz
tar -xzf v2.5.2.tar.gz && cp Unity-2.5.2/src/unity.* .

# Run tests
cd .. && make test

# Generate coverage report
make coverage
```

**Test Coverage:** ~80% (target: ≥95%)

---

## 🛠️ Supported Platforms

SafeTimer is designed to be highly portable and works on any MCU with:
- C99-compatible compiler (or C89 with `stdint.h`)
- Interrupt support (enable/disable)
- Hardware timer capable of 1ms periodic interrupt

**Compatible Architectures:**
- 8-bit MCUs (8051, AVR, PIC, etc.)
- 16-bit MCUs
- 32-bit MCUs
- Any architecture with the above requirements

See [`examples/`](examples/) directory for reference BSP implementations.

---

## 🎓 API Overview

### Timer Lifecycle

```c
/* Create timer (does not start it) */
safetimer_handle_t h = safetimer_create(period_ms, mode, callback, user_data);

/* Start/stop timer */
safetimer_start(h);
safetimer_stop(h);

/* Delete timer (release slot) */
safetimer_delete(h);
```

### Timer Processing

```c
void main_loop(void) {
    while (1) {
        safetimer_process();  /* Call regularly (≥2x shortest period) */
    }
}
```

### Query Functions

```c
int is_running;
safetimer_get_status(h, &is_running);

uint32_t remaining_ms;
safetimer_get_remaining(h, &remaining_ms);

int used, total;
safetimer_get_pool_usage(&used, &total);
```

---

## ⚙️ Configuration

Edit `include/safetimer_config.h` or use compiler flags:

```c
#define MAX_TIMERS 8              /* Maximum concurrent timers (1-32) */
#define ENABLE_PARAM_CHECK 1      /* Enable parameter validation (0/1) */
#define BSP_TICK_TYPE_16BIT 0     /* 0=32-bit (default), 1=16-bit (saves RAM) */
#define USE_STDINT_H 0            /* Use stdint.h or custom typedefs */
```

**Compiler Flags:**
```bash
gcc -DMAX_TIMERS=16 -DENABLE_PARAM_CHECK=0 -DBSP_TICK_TYPE_16BIT=1 ...
```

**BSP_TICK_TYPE_16BIT Configuration:**

| Value | Tick Type | Max Period | RAM Savings | Use Case |
|-------|-----------|------------|-------------|----------|
| **0** (default) | uint32_t | 49.7 days | 0 bytes | Standard MCUs (>256B RAM) |
| **1** | uint16_t | 65.5 seconds | ~20 bytes | Ultra-low RAM (<160B) |

**Memory Impact (16-bit vs 32-bit ticks):**
- Per timer slot: 4 bytes saved (period + expire_time)
- BSP tick counter: 4 bytes saved
- Total for MAX_TIMERS=4: ~20 bytes saved

⚠️ **Warning**: With 16-bit ticks, timer periods > 65535ms will be truncated!
Enable `ENABLE_PARAM_CHECK=1` to catch this at runtime.

---

## 🚀 Roadmap

### v1.0 (Current)
- [x] Core implementation
- [x] Unit tests + Mock BSP
- [ ] BSP examples for various MCUs
- [ ] Complete documentation
- [ ] Test coverage ≥95%

### v1.1 (Future)
- [ ] GitHub Actions CI
- [ ] Additional BSP examples
- [ ] Timer groups
- [ ] Timer priority
- [ ] Performance benchmarks

See [Epics & Stories](docs/epics_and_stories.md) for detailed roadmap.

---

## 🤝 Contributing

Contributions welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

**High-impact areas:**
- BSP implementations for additional MCUs
- Documentation improvements
- Bug reports from real-world usage
- Test case additions

---

## 📄 License

MIT License - see [LICENSE](LICENSE) file for details.

Free for commercial and non-commercial use.

---

## 🙏 Acknowledgments

- **Unity Test Framework** by ThrowTheSwitch (MIT)
- Inspired by FreeRTOS Software Timers
- Overflow algorithm from embedded systems best practices

---

## 📞 Support

- **Issues:** [GitHub Issues](https://github.com/your-repo/SafeTimer/issues) (TBD)
- **Discussions:** [GitHub Discussions](https://github.com/your-repo/SafeTimer/discussions) (TBD)
- **Documentation:** `docs/` directory

---

## ⭐ Why SafeTimer?

Compared to other timer libraries:

| Feature | SafeTimer | FreeRTOS Timers | Arduino Timer | Roll Your Own |
|---------|-----------|-----------------|---------------|---------------|
| RAM Usage | 114 bytes | 500+ bytes | 200+ bytes | Variable |
| Code Size | ~1KB | ~5KB | ~3KB | Variable |
| Dependencies | None | RTOS | Arduino | - |
| Overflow Safe | ✅ Yes | ✅ Yes | ❌ No | ⚠️ Maybe |
| 8-bit MCU Ready | ✅ Yes | ❌ No | ⚠️ Limited | ⚠️ Maybe |
| Test Coverage | ✅ 80%+ | ✅ Yes | ❌ No | ❌ No |

**SafeTimer = Minimal + Portable + Safe**

---

## 💡 Use Cases & Best Practices

### ✅ What SafeTimer is Good For

SafeTimer excels at **asynchronous timeout management** and **periodic task scheduling**:

**1. Periodic Tasks**
```c
/* LED blinking, heartbeat packets, watchdog feeding */
h_led = safetimer_create(500, TIMER_MODE_REPEAT, led_blink, NULL);
h_heartbeat = safetimer_create(1000, TIMER_MODE_REPEAT, send_heartbeat, NULL);
```

**2. Communication Timeout**
```c
/* Send data and wait for ACK with 3s timeout */
void send_packet(void) {
    uart_send(data);
    h_timeout = safetimer_create(3000, TIMER_MODE_ONE_SHOT, timeout_cb, NULL);
    safetimer_start(h_timeout);
}

void on_ack_received(void) {
    safetimer_delete(h_timeout);  /* Cancel timeout */
}
```

**3. Multi-Stage State Machines**
```c
/* Power-on sequence: 2s → init sensor → 5s → start comm */
void power_on(void) {
    h1 = safetimer_create(2000, TIMER_MODE_ONE_SHOT, init_sensor_cb, NULL);
    safetimer_start(h1);
}

void init_sensor_cb(void *data) {
    init_sensor();
    h2 = safetimer_create(5000, TIMER_MODE_ONE_SHOT, start_comm_cb, NULL);
    safetimer_start(h2);
}
```

**4. Delayed Actions**
```c
/* Turn off LED after 10 seconds */
h_off = safetimer_create(10000, TIMER_MODE_ONE_SHOT, led_off_cb, NULL);
safetimer_start(h_off);
```

---

### ❌ What SafeTimer is NOT For

**1. Button Debouncing** - Use timestamp-based approach instead:
```c
/* ✅ Efficient: 6 bytes RAM, no timer slot needed */
void key_scan(void) {
    uint8_t current = BUTTON_PIN;
    uint32_t now = bsp_get_ticks();

    if (current != g_last_state) {
        if ((now - g_last_change_time) >= 20) {  /* 20ms debounce */
            g_last_state = current;
            g_last_change_time = now;
            if (current == 0) g_key_event = 1;
        }
    }
}

/* ❌ Wasteful: 14 bytes RAM + 1 timer slot for simple task */
/* Don't create/delete timers for every button press! */
```

**2. High-Frequency Polling** - Use direct checking in main loop
**3. Microsecond Precision** - SafeTimer uses 1ms tick resolution
**4. Hard Real-Time** - Callback timing depends on `safetimer_process()` call frequency

---

### 📐 Design Guidelines

**Timer Slot Allocation Strategy:**
```c
/* Example: MAX_TIMERS = 4 */

/* Static timers (70-80%): Created once, never deleted */
Slot 0: LED blink (500ms REPEAT)
Slot 1: Heartbeat (1000ms REPEAT)

/* Dynamic timers (20-30%): Created/deleted as needed */
Slot 2: Communication timeout (temp)
Slot 3: Delayed action (temp)
```

**When to use `safetimer_delete()`:**
- ✅ Cancel timeout timers (e.g., ACK received before timeout)
- ✅ Clean up temporary delayed actions
- ✅ Test teardown functions
- ❌ NOT for static periodic timers (create once, run forever)

---

**Made with ❤️ for embedded developers fighting resource constraints**

---

## 📝 Changelog

### v1.2.5 (2025-12-17)
- 🐛 **Regression Fix**: Fixed catch-up burst introduced in v1.2.4
  - Added `SAFETIMER_ENABLE_CATCHUP` macro (default: 0 - skip missed intervals)
  - Default behavior: Skip missed intervals, no burst callbacks (deterministic CPU usage)
  - Optional behavior: Enable catch-up with `SAFETIMER_ENABLE_CATCHUP=1` (v1.2.4 behavior)
  - Restores pre-v1.2.4 expectations for LED blink, timeouts, heartbeat scenarios
  - Maintains zero cumulative drift from v1.2.4
  - See CHANGELOG.md for detailed analysis and design decisions

### v1.2.4 (2025-12-17)
- 🐛 **Bug Fix**: Eliminated cumulative drift in REPEAT timers
  - Changed from `expire_time = current_tick + period` to `expire_time += period`
  - REPEAT timers now phase-locked to original schedule, zero cumulative error
  - Long-term accuracy maintained even with `safetimer_process()` call delays
  - Compatible with ADR-005 overflow handling (signed difference algorithm)
  - No API changes, purely internal algorithm improvement

### v1.2.3 (2025-12-16)
- 📚 **Documentation Improvements**: Use cases and best practices
  - Added "Use Cases & Best Practices" section to README
  - Clarified when to use SafeTimer (async timeouts, periodic tasks)
  - Clarified when NOT to use SafeTimer (button debouncing, high-freq polling)
  - Added efficient button debouncing example using timestamp method
  - Design guidelines for timer slot allocation strategy
  - Guidance on when to use `safetimer_delete()`

### v1.2.2 (2025-12-16)
- 🎯 Resource optimization: Optional query APIs
- ⚙️ New configuration: `ENABLE_QUERY_API` (default: disabled)
- 💾 Flash savings: ~200 bytes when query APIs disabled
- 🔧 4 APIs now optional: stop, get_status, get_remaining, get_pool_usage
- ✅ 100% backward compatible via compile-time flag

### v1.2.1 (2025-12-16)
- 🔧 16-bit tick type support (BSP_TICK_TYPE_16BIT)
- 🐛 Critical fix: bsp_exit_critical() unbalanced call protection
- 📚 Enhanced documentation for tick overflow handling
- ⚡ safetimer_tick_diff() helper for wraparound-safe subtraction

### v1.2.0 (2025-12-14)
- 🔧 Optimized default: MAX_TIMERS 8 → 4 (SC8F072-optimized)
- 💾 RAM usage: 114B → 58B (49% reduction)
- ⚡ Processing: ~10µs → ~5µs (50% faster)
- 📊 Increased user RAM: 18B → 74B (+311%)

### v1.1.0 (2025-12-14)
- ⚡ Optional helper API for immediate-start timers
- 📦 New header: safetimer_helpers.h
- 🚀 Zero-overhead inline convenience functions
- 📝 Batch operations and error-handling macros

### v1.0.0 (2025-12-13)
- 🎉 Initial release
- ✅ Core timer implementation with overflow handling
- ✅ Unit tests with 96.30% coverage
- ✅ Multi-file and single-file versions

---

**Current Version:** 1.2.5 (2025-12-17)
