# SafeTimer Tutorials

This directory contains comprehensive tutorials and guides for SafeTimer.

## 📚 Available Tutorials

| Tutorial | Description |
|----------|-------------|
| [Quick Start](quick-start.md) | Installation, BSP implementation, and first timer |
| [Coroutines Tutorial](coroutines.md) | Stackless coroutines and semaphore patterns (v1.3.0+) |
| [Configuration & Tuning](configuration-and-tuning.md) | Resource optimization and compile-time flags |
| [Testing Guide](testing.md) | Unit tests, coverage reporting, and Unity framework |
| [Use Cases & Best Practices](use-cases.md) | Common patterns, anti-patterns, and design guidelines |
| [Architecture Notes](architecture-notes.md) | Overflow handling and BSP abstraction |
| [BSP Porting Guide](bsp-porting.md) | Hardware abstraction layer implementation |

## 🧩 Standalone Components

SafeTimer includes zero-dependency modules that can be used independently:

| Component | Header | Example | Description |
|-----------|--------|---------|-------------|
| **Coroutines** | `include/coro_base.h` | `examples/coro_standalone/` | Pure stackless coroutines (C89, no scheduler dependency) |

## 🚀 Getting Started

New to SafeTimer? Start with the [Quick Start](quick-start.md) tutorial.

## 📖 Learning Path

**Beginner:**
1. [Quick Start](quick-start.md) - Installation, BSP implementation, and first timer
2. [BSP Porting Guide](bsp-porting.md) - Hardware abstraction layer implementation

**Advanced:**
3. [Coroutines Tutorial](coroutines.md) - Stackless coroutines and semaphore patterns
4. [Use Cases & Best Practices](use-cases.md) - Common patterns, anti-patterns, and design guidelines

**Reference:**
- [Architecture Notes](architecture-notes.md) - Overflow handling and design decisions
- [API Reference](../docs/api_reference.md) - Complete API documentation
- [Architecture Document](../docs/architecture.md) - Design decisions and ADRs

---

**Quick Links:**
- [Use Cases & Best Practices](use-cases.md)
- [Configuration & Tuning](configuration-and-tuning.md)
- [Testing Guide](testing.md)
