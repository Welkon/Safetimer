# Architecture Notes

This document explains SafeTimer's key architectural decisions and implementation details.

---

## 🏗️ Safe 32-bit Overflow Handling (ADR-005)

### The Problem

Most timer libraries fail when tick counters wrap around at 2³²-1 (~49.7 days):

```c
/* ❌ Naive approach - breaks at overflow */
if (current_tick >= expire_time) {
    /* Timer expired */
}

/* When current_tick wraps: 0xFFFFFFFF → 0x00000000
   expire_time = 0xFFFFFFFE (expected to expire in 2ms)
   current_tick = 0x00000001 (after wraparound)
   0x00000001 >= 0xFFFFFFFE? NO! ← Timer never fires */
```

---

### SafeTimer's Solution: Signed Difference Algorithm

SafeTimer uses **signed difference comparison**:

```c
/* ✅ Overflow-safe approach */
if ((long)(current_tick - expire_time) >= 0) {
    /* Timer expired */
}
```

**How it works:**

```
Case 1: Normal (no overflow)
  current_tick = 1000
  expire_time = 500
  diff = (long)(1000 - 500) = 500 ≥ 0 ✓ (expired)

Case 2: Near overflow
  current_tick = 0x00000002  (after wraparound)
  expire_time = 0xFFFFFFFE   (before wraparound)
  diff = (long)(0x00000002 - 0xFFFFFFFE)
       = (long)(0x00000004)  (unsigned subtraction wraps)
       = 4 ≥ 0 ✓ (expired)

Case 3: Future expiration
  current_tick = 100
  expire_time = 200
  diff = (long)(100 - 200) = -100 < 0 ✗ (not expired)
```

**Mathematical proof:**
- Unsigned subtraction always wraps correctly
- Casting to `long` interprets result as signed
- Works for any tick counter value
- No special overflow handling needed

---

### Limitations

**Single timer period ≤ 2³¹-1 ms (~24.8 days)**

```c
/* ✅ Valid */
safetimer_create(86400000, ...);  /* 24 hours */

/* ❌ Invalid (>2^31-1) */
safetimer_create(2200000000, ...);  /* 25.5 days - fails */
```

**Why?** Signed comparison requires period < 2³¹ to distinguish "expired" from "future".

---

### Infinite Runtime Capability

**Result:** No 49-day crash limit. SafeTimer can run indefinitely:

```
Day 1: tick = 86,400,000
Day 30: tick = 2,592,000,000
Day 49: tick = 4,233,600,000 (wraps to 0)
Day 50: tick = 86,400,000 (continues normally)
Year 1: tick = wraps ~7 times, no errors ✓
```

---

## 🔧 Minimal BSP Interface

SafeTimer abstracts hardware with only **3 functions**:

### bsp_get_ticks()

Return milliseconds since boot:

```c
bsp_tick_t bsp_get_ticks(void) {
    return s_ticks;  /* 32-bit tick counter */
}
```

**Requirements:**
- **Monotonically increasing** (never decrements)
- **1ms resolution** (each tick = 1ms)
- **Overflow-safe** (wraps at 2³²-1)

---

### bsp_enter_critical() / bsp_exit_critical()

Protect timer state from concurrent access:

```c
void bsp_enter_critical(void) {
    EA = 0;  /* Disable interrupts (8051) */
}

void bsp_exit_critical(void) {
    EA = 1;  /* Enable interrupts */
}
```

**Requirements:**
- **Nestable** (support multiple enter/exit pairs)
- **Fast** (< 10 CPU cycles)
- **Safe** (no deadlocks)

---

### Why Only 3 Functions?

**Benefits:**
- ✅ **Portability:** Works on any MCU with interrupt control
- ✅ **No HAL dependency:** No vendor-specific libraries required
- ✅ **Simplicity:** Easy to implement and verify
- ✅ **Zero overhead:** Direct hardware access

**Comparison with other libraries:**

| Library | BSP Functions | Dependencies |
|---------|---------------|--------------|
| SafeTimer | 3 | None |
| FreeRTOS | 15+ | RTOS kernel |
| Arduino Timer | 10+ | Arduino core |

---

## 🎯 Zero Dynamic Memory Design

### Static Memory Allocation

```c
/* All memory allocated at compile time */
typedef struct {
    safetimer_t slots[MAX_TIMERS];  /* Fixed-size array */
    uint16_t next_handle_id;        /* 2 bytes state */
} safetimer_pool_t;

static safetimer_pool_t g_timer_pool;  /* Static global */
```

**Benefits:**
- ✅ **Predictable RAM usage:** `RAM = MAX_TIMERS × 14 + 2`
- ✅ **No malloc/free failures:** Deterministic behavior
- ✅ **Fast allocation:** O(1) slot lookup
- ✅ **No fragmentation:** Fixed-size blocks

---

### Handle-Based Architecture (v1.3.2+)

**Handles** use generation counter to prevent ABA reuse:

```c
typedef int safetimer_handle_t;

/* Handle encoding: [generation:3bit][index:5bit] */
#define ENCODE_HANDLE(gen, idx) (((gen) << 5) | (idx))
#define DECODE_INDEX(handle)    ((handle) & 0x1F)
#define DECODE_GEN(handle)      (((handle) >> 5) & 0x07)

/* Example: generation=3, index=5 → handle=101 (0x65) */
```

**Benefits:**
- ✅ **ABA problem prevention:** Generation counter (1~7) prevents reuse bugs
- ✅ **Invalidation safety:** Deleted handles become invalid after 8 cycles
- ✅ **Compact:** 1 byte handle (supports up to 32 timers, 7 generations)
- ✅ **Zero runtime cost:** Encoding/decoding are simple bit operations

**RAM Cost:** +1 byte per timer + 1 byte global = MAX_TIMERS + 1 bytes

---

## 🚀 Phase-Locked Timing (v1.2.4+)

### Zero Cumulative Drift

Traditional approach (drifts over time):
```c
/* ❌ Old: Resets from current tick */
expire_time = current_tick + period;  /* Accumulates error */
```

SafeTimer approach (zero drift):
```c
/* ✅ New: Advances from previous expiration */
expire_time += period;  /* Phase-locked */
```

**Impact:**

| Duration | Traditional Drift | SafeTimer Drift |
|----------|-------------------|-----------------|
| 1 hour | +0.36s | 0s |
| 1 day | +8.64s | 0s |
| 1 month | +259s (4.3 min) | 0s |
| 1 year | +3154s (52.6 min) | 0s |

See [CHANGELOG.md](../CHANGELOG.md#v124) for detailed analysis.

---

## 🔄 Coroutine Implementation (v1.3.0+)

### Duff's Device for Stackless Coroutines

SafeTimer uses **Protothread-style** coroutines:

```c
#define SAFETIMER_CORO_BEGIN(ctx) \
    switch ((ctx)->_coro_lc) { case 0:

#define SAFETIMER_CORO_WAIT(ms) do { \
    safetimer_advance_period((ctx)->_coro_handle, (ms)); \
    (ctx)->_coro_lc = __LINE__; return; \
    case __LINE__:; \
} while(0)

#define SAFETIMER_CORO_END() }
```

**How it works:**

```c
/* User code */
SAFETIMER_CORO_BEGIN(ctx);
    led_on();
    SAFETIMER_CORO_WAIT(100);
    led_off();
SAFETIMER_CORO_END();

/* Expands to */
switch (ctx->_coro_lc) {
    case 0:
        led_on();
        safetimer_advance_period(ctx->_coro_handle, 100);
        ctx->_coro_lc = __LINE__; return;
    case __LINE__:
        led_off();
}
```

**RAM cost:** +4 bytes per coroutine (line counter + handle)

---

## 📚 Architectural Decision Records (ADRs)

Key architectural decisions (ADRs):

- **ADR-001:** Fixed-size timer pool with handle-based access
- **ADR-002:** Core/Helper API separation
- **ADR-003:** BSP abstraction with 3-function interface
- **ADR-004:** Static memory allocation (no malloc/free)
- **ADR-005:** Signed difference overflow handling
- **ADR-006:** Phase-locked timing for zero drift
- **ADR-007:** Stackless coroutines via Duff's Device

---

## 🔍 Implementation Details

### Timer Slot Structure

```c
typedef struct {
    bsp_tick_t expire_time;    /* 4 bytes: When timer fires */
    uint32_t period;           /* 4 bytes: Timer period */
    safetimer_callback_t cb;   /* 2-4 bytes: Callback pointer */
    void *user_data;           /* 2-4 bytes: User context */
    uint8_t active;            /* 1 byte: Running state */
    uint8_t mode;              /* 1 byte: ONE_SHOT/REPEAT */
} safetimer_t;  /* Total: 14-18 bytes (8-bit: 14, 32-bit: 18) */
```

---

### Processing Algorithm

```c
void safetimer_process(void) {
    bsp_tick_t now = bsp_get_ticks();

    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!slots[i].active) continue;

        /* Signed difference comparison (ADR-005) */
        if ((long)(now - slots[i].expire_time) >= 0) {
            /* Fire callback */
            if (slots[i].cb) slots[i].cb(slots[i].user_data);

            /* Reload for REPEAT mode */
            if (slots[i].mode == TIMER_MODE_REPEAT) {
                slots[i].expire_time += slots[i].period;  /* Phase-locked */
            } else {
                slots[i].active = 0;  /* ONE_SHOT: stop */
            }
        }
    }
}
```

**Complexity:** O(MAX_TIMERS) per call

---

## ⚠️ Low-Power Mode Considerations

### Sleep Time Compensation

When MCU enters low-power mode (Sleep/Stop), hardware timers may stop counting. SafeTimer relies on `bsp_get_ticks()` monotonicity, so BSP layer must compensate for sleep time.

**BSP Implementation Example:**

```c
static volatile uint32_t s_ticks = 0;
static uint32_t s_sleep_compensation = 0;

bsp_tick_t bsp_get_ticks(void) {
    return s_ticks + s_sleep_compensation;  /* Include compensation */
}

void mcu_enter_sleep(uint32_t duration_ms) {
    /* Enter low-power mode */
    rtc_sleep(duration_ms);

    /* Wakeup: compensate for sleep time */
    s_sleep_compensation += duration_ms;
}

/* Alternative: Use RTC to measure actual sleep duration */
void mcu_wakeup_handler(void) {
    uint32_t actual_sleep = rtc_get_elapsed_ms();
    s_sleep_compensation += actual_sleep;
}
```

**Key Points:**
- ✅ SafeTimer is **not** responsible for sleep compensation
- ✅ BSP layer **must** maintain tick monotonicity
- ✅ Use RTC or low-power timer to measure sleep duration
- ⚠️ Failure to compensate causes timer lag after wakeup

---

## 📖 Further Reading

- **BSP Implementation:** [BSP Porting Guide](bsp-porting.md)
- **API Reference:** See `include/safetimer.h` for complete API documentation
- **Source Code:** See `src/safetimer.c` for implementation details

---

**Design Philosophy:** Simplicity, safety, and zero surprises.
