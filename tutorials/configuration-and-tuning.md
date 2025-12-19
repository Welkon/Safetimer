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

Configurable via `MAX_TIMERS`:

| Configuration | RAM Usage | Use Case |
|---------------|-----------|----------|
| 4 timers (default) | 58 bytes | Typical applications |
| 8 timers | 114 bytes | Medium complexity |
| 16 timers | 226 bytes | High complexity |
| 32 timers | 450 bytes | Maximum capacity |

**Formula:** `RAM = MAX_TIMERS × 14 + 2 bytes`

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

| Value | Tick Type | Max Period | RAM Savings | Use Case |
|-------|-----------|------------|-------------|----------|
| **0** (default) | uint32_t | 49.7 days | 0 bytes | Standard MCUs (>256B RAM) |
| **1** | uint16_t | 65.5 seconds | ~20 bytes | Ultra-low RAM (<160B) |

**Memory Impact (16-bit vs 32-bit):**
- Per timer slot: 4 bytes saved (period + expire_time)
- BSP tick counter: 4 bytes saved
- Total for MAX_TIMERS=4: ~20 bytes saved

⚠️ **Warning:** With 16-bit ticks, timer periods > 65535ms will be truncated! Enable `ENABLE_PARAM_CHECK=1` to catch this at runtime.

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
#define BSP_TICK_TYPE_16BIT 1  /* 16-bit ticks */
```

**Result:** ~0.8KB Flash, ~32 bytes RAM
**Limitation:** Max timer period 65.5 seconds

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
RAM = MAX_TIMERS × 14 + 2
Flash = Base + (QUERY_API × 200) + (PARAM_CHECK × 150)
  where Base = 600 bytes
```

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

Use [Design Guidelines](design-guidelines.md) to minimize fragmentation:
- **70-80% static timers** (created once, never deleted)
- **20-30% dynamic timers** (created/deleted as needed)

---

## 🎯 Configuration Examples by Platform

### SC8F072 (128B RAM, 4KB Flash)
```c
#define MAX_TIMERS 4
#define ENABLE_QUERY_API 0
#define ENABLE_PARAM_CHECK 1  /* Debug builds only */
#define BSP_TICK_TYPE_16BIT 0
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
- **Design Patterns:** [Design Guidelines](design-guidelines.md)

---

See [safetimer_config.h](../include/safetimer_config.h) for complete configuration reference.
