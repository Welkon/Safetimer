# Configuration & Tuning Guide

This guide covers SafeTimer's compile-time configuration options and resource optimization strategies.

---

## 📊 Resource Usage

### Default Configuration (4 timers)

- **RAM:** 58 bytes (4 timers) to 114 bytes (8 timers)
- **Flash:** ~0.8 KB (minimal) to ~1.2 KB (full featured)
- **Processing:** ~5-10µs per `safetimer_process()` call on typical 8-bit MCUs

### Flash Configurations

**Minimal (~0.8KB):** Production-optimized
```c
#define ENABLE_QUERY_API 0
#define ENABLE_PARAM_CHECK 0
```

**Full (~1.0KB):** Development/debugging
```c
#define ENABLE_QUERY_API 1
#define ENABLE_PARAM_CHECK 1
```

### RAM Scalability

Configurable via `MAX_TIMERS` and optimization flags.

**Formula per timer:**
- **Standard (32-bit ticks):** 13 bytes
- **Low RAM (16-bit ticks):** 9 bytes
- **No User Data:** Subtract 2 bytes (on 16-bit arch) or 4 bytes (on 32-bit arch)

**Overhead:** 2 bytes fixed (bitmap + processing flag) for <= 8 timers.

| Configuration | 16-bit Tick | 32-bit Tick |
|---------------|-------------|-------------|
| 4 timers (default) | 40 bytes | 56 bytes |
| 8 timers | 74 bytes | 108 bytes |
| 4 timers (Ultra-Opt*) | **30 bytes** | - |

*\*Ultra-Opt: 16-bit ticks + No User Data*

---

## ⚙️ Configuration Options

### ENABLE_QUERY_API (default: 0)

Control availability of query/diagnostic APIs.

```c
#define ENABLE_QUERY_API 1  /* Enable query APIs */
```

**Disabled (0):**
- **Saves:** ~200 bytes Flash (20% of library size)
- **Removes:** `safetimer_stop()`, `safetimer_get_status()`, `safetimer_get_remaining()`, `safetimer_get_pool_usage()`
- **Best for:** Production builds on resource-constrained MCUs

**Enabled (1):**
- **Full API available** for debugging/diagnostics
- **Best for:** Development, testing, dynamic timer management

---

### ENABLE_HELPER_API (default: 1)

Control availability of convenience helper functions.

```c
#define ENABLE_HELPER_API 0  /* Disable convenience functions */
```

**Enabled (1, default):**
- **Provides:** `safetimer_create_started()`, `safetimer_create_started_batch()`, `SAFETIMER_CREATE_STARTED_OR()`
- **Cost:** 0 bytes (inline functions, only compiled if actually used)
- **Best for:** Most applications benefit from simpler create+start API

**Disabled (0):**
- **Removes:** All convenience wrappers
- **Only core APIs available:** `create()`, `start()`, `delete()`, `process()`
- **Best for:** Strict minimal API surface, explicit control preferred

---

### ENABLE_PARAM_CHECK (default: 1)

Control parameter validation in API calls.

```c
#define ENABLE_PARAM_CHECK 0  /* Disable for production */
```

**Enabled (1):**
- **Safer:** Validates all API parameters
- **Cost:** ~150 bytes Flash
- **Best for:** Development, debugging, untrusted input

**Disabled (0):**
- **Faster:** No validation overhead
- **Minimal footprint**
- **Best for:** Production builds with validated code

---

### MAX_TIMERS (default: 4)

Control maximum concurrent timers.

```c
#define MAX_TIMERS 8  /* Increase timer pool */
```

**Guidelines:**
- **4 timers:** Typical embedded applications (LED, heartbeat, timeout, sensor)
- **8 timers:** Medium complexity (multiple sensors, protocols)
- **16+ timers:** Complex systems (many concurrent tasks)

**RAM Impact:** Each timer costs 14 bytes

---

### BSP_TICK_TYPE_16BIT (default: 0)

Control tick counter size for ultra-low RAM mode.

```c
#define BSP_TICK_TYPE_16BIT 1  /* Ultra-low RAM mode */
```

| Value | Tick Type | Max Period | RAM Savings |
|-------|-----------|------------|-------------|
| **0** (default) | uint32_t | 49.7 days | 0 bytes |
| **1** | uint16_t | 65.5 seconds | ~20 bytes |

**Memory Impact:** Saves 4 bytes per timer + 4 bytes global.

⚠️ **Warning:** Periods > 65535ms will be truncated in 16-bit mode!

---

### SAFETIMER_ENABLE_USER_DATA (default: 1)
**New in v1.3.1**

Removes the `void *user_data` context from timer callbacks and creation API.

```c
#define SAFETIMER_ENABLE_USER_DATA 0
```

- **Enabled (1):** Standard callbacks `void cb(void *user_data)`.
- **Disabled (0):** Simplified callbacks `void cb(void)`.
- **Savings:** Saves pointer storage (2-4 bytes) per timer.

### SAFETIMER_REPEAT_ONLY (default: 0)
**New in v1.3.1**

Restricts library to support ONLY `TIMER_MODE_REPEAT`.

```c
#define SAFETIMER_REPEAT_ONLY 1
```

- **Enabled (1):** Removes all logic for `TIMER_MODE_ONE_SHOT`.
- **Savings:** ~50 bytes Flash code size.

### SAFETIMER_ENABLE_CORO (default: 1)
**New in v1.3.1**

Controls compilation of coroutine support helpers.

```c
#define SAFETIMER_ENABLE_CORO 0
```

- **Enabled (1):** Supports `safetimer_coro.h`.
- **Disabled (0):** Removes coroutine binding logic.
- **Savings:** ~200 bytes Flash code size.

---

### USE_STDINT_H (default: 0)

Control standard integer type definitions.

```c
#define USE_STDINT_H 1  /* Use stdint.h */
```

**Disabled (0):** Use custom typedefs (better for ancient compilers)
**Enabled (1):** Use `stdint.h` (recommended for C99+ compilers)

---

## 🚀 Optimization Strategies

### Strategy 1: Production Minimal Build

**Target:** Smallest Flash/RAM footprint
```c
#define MAX_TIMERS 4
#define ENABLE_QUERY_API 0
#define ENABLE_PARAM_CHECK 0
#define BSP_TICK_TYPE_16BIT 0
```

**Result:** ~0.8KB Flash, 58 bytes RAM

---

### Strategy 2: Ultra-Low RAM Build

**Target:** MCUs with <160 bytes RAM
```c
#define MAX_TIMERS 2
#define ENABLE_QUERY_API 0
#define ENABLE_PARAM_CHECK 0
#define BSP_TICK_TYPE_16BIT 1
#define SAFETIMER_ENABLE_USER_DATA 0 /* Save pointer overhead */
```

**Result:** ~0.8KB Flash, **~16 bytes RAM** (for 2 timers)
**Limitation:** Max period 65.5s, no user context in callbacks.

---

### Strategy 3: Development/Debug Build

**Target:** Maximum diagnostics
```c
#define MAX_TIMERS 8
#define ENABLE_QUERY_API 1
#define ENABLE_PARAM_CHECK 1
#define BSP_TICK_TYPE_16BIT 0
```

**Result:** ~1.2KB Flash, 114 bytes RAM

---

### Strategy 4: Balanced Build

**Target:** Production with query APIs
```c
#define MAX_TIMERS 4
#define ENABLE_QUERY_API 1
#define ENABLE_PARAM_CHECK 0
#define BSP_TICK_TYPE_16BIT 0
```

**Result:** ~1.0KB Flash, 58 bytes RAM

---

## 🔧 Compiler Flags

Override config file settings via compiler flags:

```bash
# GCC/Clang
gcc -DMAX_TIMERS=16 -DENABLE_PARAM_CHECK=0 -DBSP_TICK_TYPE_16BIT=1 ...

# SDCC
sdcc -DMAX_TIMERS=8 -DENABLE_QUERY_API=1 ...

# Keil C51
C51 DEFINE(MAX_TIMERS=4,ENABLE_PARAM_CHECK=0) ...
```

---

## 📐 Resource Estimation Tool

Calculate RAM usage for your configuration:

```
RAM = MAX_TIMERS × SlotSize + 2
Flash = Base + (QUERY_API × 200) + (PARAM_CHECK × 150)
```
**SlotSize:**
- 32-bit + UserData: 13 bytes
- 16-bit + UserData: 9 bytes
- 16-bit No UserData: 7 bytes

**Examples:**
- `MAX_TIMERS=4, QUERY=0, PARAM=0`: 60 bytes RAM, ~750 bytes Flash
- `MAX_TIMERS=8, QUERY=1, PARAM=1`: 114 bytes RAM, ~1200 bytes Flash

---

## ⚡ Performance Tuning

### safetimer_process() Call Frequency

**Rule of thumb:** Call `safetimer_process()` at least **2× the shortest timer period**

**Examples:**
- Shortest timer: 10ms → Call process() every ≤5ms
- Shortest timer: 100ms → Call process() every ≤50ms

**Overhead:**
- Per process() call: ~5-10µs @ 8MHz (8 timers)
- Scales linearly with active timers

### Timer Pool Fragmentation

Use [Best Practices](use-cases.md) to minimize fragmentation:
- **70-80% static timers** (created once, never deleted)
- **20-30% dynamic timers** (created/deleted as needed)

---

## 🎯 Configuration Examples by Platform

### SC8F072 (128B RAM, 2KB Flash)
```c
#define MAX_TIMERS 4
#define ENABLE_QUERY_API 0
#define ENABLE_PARAM_CHECK 1  /* Debug builds only */
#define BSP_TICK_TYPE_16BIT 1
```

### ATtiny85 (512B RAM, 8KB Flash)
```c
#define MAX_TIMERS 8
#define ENABLE_QUERY_API 1
#define ENABLE_PARAM_CHECK 0
#define BSP_TICK_TYPE_16BIT 0
```

### STM32F0 (8KB RAM, 64KB Flash)
```c
#define MAX_TIMERS 16
#define ENABLE_QUERY_API 1
#define ENABLE_PARAM_CHECK 1
#define BSP_TICK_TYPE_16BIT 0
```

---

## 📖 Next Steps

- **API Details:** See `include/safetimer_config.h` for all configuration options
- **Hardware Porting:** [BSP Porting Guide](bsp-porting.md)
- **Design Patterns:** [Use Cases & Best Practices](use-cases.md)

---

See [safetimer_config.h](../include/safetimer_config.h) for complete configuration reference.
