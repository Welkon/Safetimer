# SafeTimer ⏱️

**Lightweight Embedded Timer Library for Resource-Constrained 8-bit MCUs**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.3.1-green.svg)]()
[![C99](https://img.shields.io/badge/C-C99-brightgreen.svg)]()
[![Test Coverage](https://img.shields.io/badge/coverage-96.30%25-brightgreen.svg)]()
[![Tests](https://img.shields.io/badge/tests-63%20passing-success.svg)]()

English | [简体中文](README_zh-CN.md)

---

## 🎯 Features

- **Minimal RAM Footprint:** Only 60 bytes for 4 concurrent timers (v1.3.0)
- **Small Code Size:** ~0.8KB Flash (query APIs disabled) | ~1.0KB Flash (full featured)
- **Zero Dynamic Allocation:** No malloc/free, fully static memory
- **Overflow-Safe:** Handles 32-bit time wraparound automatically (ADR-005)
- **Portable:** 3-function BSP interface, works on any MCU
- **Flexible API:** Core API for explicit control + optional helpers for convenience (v1.1+)
- **Coroutine Support (v1.3.0):** Stackless coroutines (Protothread-style) + semaphores for async programming
- **Well-Tested:** 63 unit tests, 96.30% coverage
- **Production-Ready:** MISRA-C compliant, static analysis clean

---

## 📦 Quick Start

### 3-Step Integration

**1. Copy Files (4 required)**
```bash
cp SafeTimer/include/{safetimer.h,safetimer_config.h,bsp.h} your_project/
cp SafeTimer/src/safetimer.c your_project/
```

**2. Implement BSP (3 functions)**
```c
bsp_tick_t bsp_get_ticks(void);      // Return milliseconds since boot
void bsp_enter_critical(void);       // Disable interrupts
void bsp_exit_critical(void);        // Enable interrupts
```

**3. Use Timers**
```c
#include "safetimer.h"

safetimer_handle_t h = safetimer_create(1000, TIMER_MODE_REPEAT, callback, NULL);
safetimer_start(h);

while (1) {
    safetimer_process();  // Call in main loop
}
```

**📖 Complete Tutorial:** See [tutorials/quick-start.md](tutorials/quick-start.md)

---

## ⚙️ System Requirements

**Hardware:**
- **RAM:** 58 bytes (4 timers) | 114 bytes (8 timers)
- **Flash:** ~0.8-1.2 KB
- **Timer:** 1ms periodic interrupt

**Software:**
- **Compiler:** C99 or C89 with `stdint.h`
- **No Dependencies:** No RTOS, HAL, or dynamic memory required

**Compatibility:** 8-bit MCUs (8051, AVR, PIC) | 16-bit | 32-bit | Any architecture with interrupt support

---

## 🎓 API Overview

### Core API

```c
/* Create timer */
safetimer_handle_t h = safetimer_create(period_ms, mode, callback, user_data);

/* Lifecycle */
safetimer_start(h);
safetimer_stop(h);   // Optional (requires ENABLE_QUERY_API=1)
safetimer_delete(h);

/* Processing (call in main loop) */
safetimer_process();

/* Query (optional, requires ENABLE_QUERY_API=1) */
safetimer_get_status(h, &is_running);
safetimer_get_remaining(h, &remaining_ms);
safetimer_get_pool_usage(&used, &total);
```

**📖 Complete API Reference:** See `include/safetimer.h` for full API documentation

---

## 📚 Documentation

### Tutorials

Start here to learn SafeTimer: **[Tutorial Index](tutorials/README.md)**

| Tutorial | Description |
|----------|-------------|
| [Quick Start](tutorials/quick-start.md) | Installation, BSP, first timer |
| [Coroutines Tutorial](tutorials/coroutines.md) | Async programming with coroutines (v1.3.0+) |
| [Configuration & Tuning](tutorials/configuration-and-tuning.md) | Resource optimization, compile-time flags |
| [Use Cases & Best Practices](tutorials/use-cases.md) | Common patterns, anti-patterns, design guidelines |
| [Testing Guide](tutorials/testing.md) | Unit tests, coverage, CI/CD |
| [BSP Porting Guide](tutorials/bsp-porting.md) | Hardware abstraction implementation |
| [Architecture Notes](tutorials/architecture-notes.md) | Overflow handling, design decisions |

### Technical Reference

- **[API Reference](docs/api_reference.md)** - Complete API documentation
- **[Architecture Document](docs/architecture.md)** - Design decisions and ADRs

### Header Files

Browse API directly in `include/` directory:
- `safetimer.h` - Core API + convenience helpers
- `safetimer_config.h` - Configuration options
- `safetimer_coro.h` - Coroutine macros (v1.3.0+)
- `bsp.h` - BSP interface specification

---

## 🛠️ Supported Platforms

SafeTimer is highly portable and works on any MCU with:
- C99-compatible compiler (or C89 with `stdint.h`)
- Interrupt support (enable/disable)
- Hardware timer capable of 1ms periodic interrupt

**Compatible Architectures:** 8-bit (8051, AVR, PIC) | 16-bit | 32-bit | Any with above requirements

**Examples:** See [`examples/`](examples/) for reference BSP implementations (SC8F072, coroutine demos).

---

## 🚀 Roadmap

### v1.3.x (Current)
- [x] Core implementation with overflow handling
- [x] Coroutine support (v1.3.0)
- [x] Zero cumulative drift (v1.3.1)
- [x] 63 unit tests, 96.30% coverage
- [x] GitHub Actions CI/CD

### v1.4 (Future)
- [ ] Additional BSP examples
- [ ] Timer groups
- [ ] Timer priority
- [ ] Performance benchmarks

See [CHANGELOG.md](CHANGELOG.md) for complete version history.

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

## 📝 Changelog

See [CHANGELOG.md](CHANGELOG.md) for complete version history.

**Latest:** v1.3.1 (2025-12-19) - Fixed coroutine cumulative timing error with `safetimer_advance_period()` API

---

**Made with ❤️ for embedded developers fighting resource constraints**

---

**Current Version:** 1.3.1 (2025-12-19)
