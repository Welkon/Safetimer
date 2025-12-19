# BSP Porting Guide

This guide walks you through implementing SafeTimer's BSP (Board Support Package) for your target MCU.

---

## 🎯 Overview

SafeTimer requires only **3 BSP functions**:

1. `bsp_get_ticks()` - Return milliseconds since boot
2. `bsp_enter_critical()` - Disable interrupts
3. `bsp_exit_critical()` - Enable interrupts

---

## 📝 BSP Template

Create `safetimer_bsp.c`:

```c
#include "bsp.h"

/* Global tick counter (incremented by timer ISR) */
static volatile bsp_tick_t s_ticks = 0;

/* Hardware timer interrupt (called every 1ms) */
void timer_isr(void) {
    s_ticks++;
}

/* BSP Function 1: Get current tick count */
bsp_tick_t bsp_get_ticks(void) {
    return s_ticks;
}

/* BSP Function 2: Enter critical section */
void bsp_enter_critical(void) {
    /* TODO: Disable interrupts for your MCU */
    EA = 0;  /* Example: 8051 */
}

/* BSP Function 3: Exit critical section */
void bsp_exit_critical(void) {
    /* TODO: Enable interrupts for your MCU */
    EA = 1;  /* Example: 8051 */
}
```

---

## 🔧 Step-by-Step Implementation

### Step 1: Configure Hardware Timer

**Goal:** Generate 1ms periodic interrupt

**Example (SC8F072):**

```c
void init_timer0(void) {
    /* Configure Timer0 for 1ms @ 8MHz */
    TMOD = 0x01;          /* Timer0: 16-bit mode */
    TH0 = 0xFC;           /* Reload value */
    TL0 = 0x18;           /* 8MHz / 12 / 1000Hz */
    ET0 = 1;              /* Enable Timer0 interrupt */
    TR0 = 1;              /* Start Timer0 */
    EA = 1;               /* Enable global interrupts */
}

void timer0_isr(void) __interrupt(1) {
    TH0 = 0xFC;           /* Reload timer */
    TL0 = 0x18;
    s_ticks++;            /* Increment tick counter */
}
```

---

### Step 2: Implement bsp_get_ticks()

**Requirements:**
- Return `bsp_tick_t` (uint32_t)
- Monotonically increasing
- Overflow-safe (wraps at 2³²-1)
- Atomic read (if tick counter is multi-byte)

**Simple implementation:**

```c
bsp_tick_t bsp_get_ticks(void) {
    return s_ticks;  /* Single read - atomic on 32-bit MCUs */
}
```

**Atomic implementation (8-bit/16-bit MCUs):**

```c
bsp_tick_t bsp_get_ticks(void) {
    bsp_tick_t ticks;

    bsp_enter_critical();
    ticks = s_ticks;  /* Multi-byte read - needs protection */
    bsp_exit_critical();

    return ticks;
}
```

---

### Step 3: Implement Critical Section Functions

**Requirements:**
- Nestable (support multiple enter/exit pairs)
- Fast (< 10 CPU cycles)
- Balance guaranteed (every enter must match exit)

**Simple implementation (non-nestable):**

```c
void bsp_enter_critical(void) {
    __disable_irq();  /* Compiler intrinsic */
}

void bsp_exit_critical(void) {
    __enable_irq();
}
```

**Nestable implementation:**

```c
static uint8_t g_critical_nesting = 0;

void bsp_enter_critical(void) {
    __disable_irq();
    g_critical_nesting++;
}

void bsp_exit_critical(void) {
    if (--g_critical_nesting == 0) {
        __enable_irq();
    }
}
```

---

## 🖥️ Platform-Specific Examples

### 8051 Family (8-bit)

```c
void bsp_enter_critical(void) {
    EA = 0;  /* Disable interrupts */
}

void bsp_exit_critical(void) {
    EA = 1;  /* Enable interrupts */
}
```

---

### AVR (8-bit)

```c
#include <avr/interrupt.h>

void bsp_enter_critical(void) {
    cli();  /* Clear global interrupt flag */
}

void bsp_exit_critical(void) {
    sei();  /* Set global interrupt flag */
}
```

---

### ARM Cortex-M (32-bit)

```c
#include "cmsis.h"

static uint32_t g_primask = 0;

void bsp_enter_critical(void) {
    g_primask = __get_PRIMASK();
    __disable_irq();
}

void bsp_exit_critical(void) {
    __set_PRIMASK(g_primask);
}
```

---

### STM32 HAL

```c
void bsp_enter_critical(void) {
    HAL_NVIC_DisableIRQ(TIM2_IRQn);  /* Or use __disable_irq() */
}

void bsp_exit_critical(void) {
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
}
```

---

### ESP32 (FreeRTOS)

```c
static portMUX_TYPE g_spinlock = portMUX_INITIALIZER_UNLOCKED;

void bsp_enter_critical(void) {
    portENTER_CRITICAL(&g_spinlock);
}

void bsp_exit_critical(void) {
    portEXIT_CRITICAL(&g_spinlock);
}
```

---

## ⚠️ Common Pitfalls

### Pitfall 1: Incorrect Timer Frequency

**❌ Problem:**
```c
/* Timer configured for 10ms, not 1ms */
void timer_isr(void) {
    s_ticks += 10;  /* Wrong! */
}
```

**✅ Solution:**
```c
/* Timer interrupt every 1ms */
void timer_isr(void) {
    s_ticks++;  /* Correct */
}
```

---

### Pitfall 2: Non-Atomic Tick Read

**❌ Problem (8-bit MCU):**
```c
/* s_ticks is 32-bit, read is not atomic */
bsp_tick_t bsp_get_ticks(void) {
    return s_ticks;  /* Race condition! */
}
```

**✅ Solution:**
```c
bsp_tick_t bsp_get_ticks(void) {
    bsp_tick_t ticks;
    EA = 0;            /* Disable interrupts */
    ticks = s_ticks;   /* Atomic read */
    EA = 1;            /* Enable interrupts */
    return ticks;
}
```

---

### Pitfall 3: Unbalanced Critical Sections

**❌ Problem:**
```c
void buggy_function(void) {
    bsp_enter_critical();
    if (error) return;  /* Exit without bsp_exit_critical()! */
    bsp_exit_critical();
}
```

**✅ Solution:**
```c
void correct_function(void) {
    bsp_enter_critical();
    if (error) {
        bsp_exit_critical();  /* Always balance */
        return;
    }
    bsp_exit_critical();
}
```

---

### Pitfall 4: Tick Counter Overflow

**❌ Problem:**
```c
static volatile uint16_t s_ticks = 0;  /* Overflows at 65.5 seconds! */
```

**✅ Solution:**
```c
static volatile uint32_t s_ticks = 0;  /* Overflows at 49.7 days */
```

---

## 🧪 Testing Your BSP

### Test 1: Tick Increment

```c
void test_bsp_ticks(void) {
    init_timer();

    bsp_tick_t start = bsp_get_ticks();
    delay_ms(100);  /* Wait 100ms */
    bsp_tick_t end = bsp_get_ticks();

    /* Should be ~100 ticks (±5% tolerance) */
    assert(end - start >= 95 && end - start <= 105);
}
```

---

### Test 2: Critical Section Balance

```c
void test_critical_section(void) {
    bsp_enter_critical();
    /* Interrupts disabled */

    bsp_enter_critical();  /* Nested */
    bsp_exit_critical();
    /* Interrupts still disabled */

    bsp_exit_critical();
    /* Interrupts re-enabled */
}
```

---

### Test 3: Tick Overflow

```c
void test_tick_overflow(void) {
    s_ticks = 0xFFFFFFFE;  /* Near overflow */

    delay_ms(5);  /* Wait for overflow */

    bsp_tick_t now = bsp_get_ticks();
    /* Should wrap correctly: 0xFFFFFFFE → 0xFFFFFFFF → 0x00000000 → ... */
    assert(now < 10);  /* Wrapped successfully */
}
```

---

## 📊 Performance Benchmarks

Measure BSP overhead:

```c
void benchmark_bsp(void) {
    /* Benchmark bsp_get_ticks() */
    bsp_tick_t start = read_cycle_counter();
    bsp_get_ticks();
    bsp_tick_t end = read_cycle_counter();
    printf("bsp_get_ticks: %lu cycles\n", end - start);

    /* Benchmark critical section */
    start = read_cycle_counter();
    bsp_enter_critical();
    bsp_exit_critical();
    end = read_cycle_counter();
    printf("critical section: %lu cycles\n", end - start);
}
```

**Target:**
- `bsp_get_ticks()`: < 20 cycles
- `bsp_enter/exit_critical()`: < 10 cycles each

---

## 📖 Complete Examples

See [`examples/`](../examples/) for complete BSP implementations:

- **SC8F072:** 8-bit 8051-compatible MCU
- **Coroutine Demo:** Mock BSP for testing

For detailed porting guide, see [../docs/porting_guide.md](../docs/porting_guide.md).

---

## 🚀 Next Steps

- **Test Your Port:** [Testing Guide](testing.md)
- **Optimize Configuration:** [Configuration & Tuning](configuration-and-tuning.md)
- **Build Application:** [Quick Start](quick-start.md)

---

**Need help?** Open an issue on [GitHub](https://github.com/Welkon/SafeTimer/issues).
