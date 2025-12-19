# Quick Start Guide

This guide walks you through integrating SafeTimer into your embedded project in 3 simple steps.

---

## Step 1: Installation

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

---

## Step 2: Implement BSP (3 Functions)

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

**BSP Function Requirements:**
- `bsp_get_ticks()`: Return milliseconds since boot (32-bit tick counter)
- `bsp_enter_critical()`: Disable interrupts (atomic operations)
- `bsp_exit_critical()`: Enable interrupts

See [`examples/`](../examples/) for complete BSP implementations.

---

## Step 3: Use Timers

### Basic Usage (Core API)

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

---

### Simpler Alternative (Helper API, v1.1+)

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

See [`examples/helpers_demo/`](../examples/helpers_demo/) for detailed comparison.

---

## Advanced: Coroutines (v1.3.0+)

SafeTimer supports **stackless coroutines** (Protothread-style) for linear async programming. Perfect for UART timeouts, sensor polling, and state machines.

### Quick Example

```c
#include "safetimer.h"
#include "safetimer_coro.h"

typedef struct {
    SAFETIMER_CORO_CONTEXT;  /* Must be first member */
    int counter;
} my_coro_ctx_t;

void led_blink_coro(void *user_data) {
    my_coro_ctx_t *ctx = (my_coro_ctx_t *)user_data;

    SAFETIMER_CORO_BEGIN(ctx);

    while (1) {
        led_on();
        SAFETIMER_CORO_SLEEP(100);   /* LED on for 100ms */

        led_off();
        SAFETIMER_CORO_SLEEP(900);   /* LED off for 900ms */

        ctx->counter++;
    }

    SAFETIMER_CORO_END();
}

int main(void) {
    static my_coro_ctx_t ctx = {0};

    init_timer0();

    /* Create coroutine timer (MUST use TIMER_MODE_REPEAT) */
    safetimer_handle_t h = safetimer_create(
        10, TIMER_MODE_REPEAT, led_blink_coro, &ctx
    );
    ctx._coro_handle = h;  /* Store handle for SLEEP/WAIT_UNTIL */
    safetimer_start(h);

    while (1) {
        safetimer_process();
    }
}
```

**Coroutine Macros:**
- `SAFETIMER_CORO_SLEEP(ms)` - Sleep for specified milliseconds
- `SAFETIMER_CORO_WAIT_UNTIL(cond, poll_ms)` - Wait until condition is true
- `SAFETIMER_CORO_YIELD()` - Explicit yield
- `SAFETIMER_CORO_RESET()` - Restart coroutine from beginning
- `SAFETIMER_CORO_EXIT()` - Exit coroutine permanently

**When to Use Coroutines:**
- ✅ UART timeouts with state machines
- ✅ Sensor polling with multi-step sequences
- ✅ Protocol implementations with delays
- ❌ Simple periodic tasks (use callbacks instead)

See [Coroutines Tutorial](coroutines.md) for complete guide with semaphores.

---

## Next Steps

- **Configuration:** [Configuration & Tuning](configuration-and-tuning.md)
- **Real-world Examples:** [Use Cases & Best Practices](use-cases.md)
- **Hardware Porting:** [BSP Porting Guide](bsp-porting.md)
- **API Reference:** [../docs/api_reference.md](../docs/api_reference.md)

---

**Questions?** See [../docs/user_guide.md](../docs/user_guide.md) for comprehensive documentation.
