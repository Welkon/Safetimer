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

**File Summary:**
- ✅ **Required (4 files):** safetimer.h, safetimer_config.h, bsp.h, safetimer.c
- ✅ **Optional (1 file):** safetimer_helpers.h (convenience API, v1.1+)

---

## Step 2: Implement BSP (3 Functions)

Create `safetimer_bsp.c` with these 3 functions:

> **💡 Naming Tip:** We recommend `safetimer_bsp.c` to avoid conflicts with other libraries. Alternatively, use `myapp_bsp.c` or place in a subdirectory like `bsp/safetimer.c`.

```c
#include "bsp.h"

static volatile bsp_tick_t s_ticks = 0;
static volatile uint8_t s_critical_nesting = 0;
static volatile uint8_t s_saved_interrupt_state = 0;

/* Called by hardware timer interrupt every 1ms */
void timer_isr(void) {
    s_ticks++;
}

bsp_tick_t bsp_get_ticks(void) {
    bsp_tick_t ticks;
    uint8_t saved_state = EA;  /* Save current interrupt state */
    EA = 0;                    /* Disable for atomic read */
    ticks = s_ticks;
    EA = saved_state;          /* Restore original state */
    return ticks;
}

void bsp_enter_critical(void) {
    uint8_t ea_state = EA;
    EA = 0;  /* Disable interrupts */

    if (s_critical_nesting == 0) {
        s_saved_interrupt_state = ea_state;  /* Save state on first entry */
    }
    s_critical_nesting++;
}

void bsp_exit_critical(void) {
    if (s_critical_nesting > 0) {
        EA = 0;  /* Ensure atomic access */
        s_critical_nesting--;

        if (s_critical_nesting == 0) {
            EA = s_saved_interrupt_state;  /* Restore original state */
        }
    }
}
```

**BSP Function Requirements:**
- `bsp_get_ticks()`: Atomic read of tick counter (preserves interrupt state)
- `bsp_enter_critical()`: Disable interrupts with nesting support
- `bsp_exit_critical()`: Restore interrupts (only on outermost exit)

**Key Implementation Details:**
- **Nesting support:** `s_critical_nesting` tracks depth, allows nested calls
- **State preservation:** `s_saved_interrupt_state` restores original state (not always "enable")
- **Atomic reads:** `bsp_get_ticks()` protects 32-bit reads on 8-bit MCUs

**Why nesting matters:** Real-world scenarios where nesting occurs:
1. **User calls SafeTimer API from callback:**
   ```c
   void my_callback(void *data) {
       bsp_enter_critical();  // User's critical section
       safetimer_delete(h);   // SafeTimer enters critical again
       bsp_exit_critical();
   }
   ```
2. **User calls SafeTimer API from ISR:**
   ```c
   void uart_isr(void) {
       bsp_enter_critical();
       safetimer_start(timeout_handle);  // Nested critical section
       bsp_exit_critical();
   }
   ```

Without nesting support, the inner `bsp_exit_critical()` would incorrectly re-enable interrupts while still in the outer critical section.

See [`examples/sc8f072/bsp_sc8f072.c`](../examples/sc8f072/bsp_sc8f072.c) for production-ready implementation with detailed comments.

---

## Step 3: Understanding Programming Models

SafeTimer supports **three complementary programming models**. They work together - most applications use all three for different tasks:

- **Callbacks** for stateless periodic tasks
- **Coroutines** for linear sequences
- **StateSmith FSM** for state-driven logic

**Key Point:** These are **tools in your toolbox**, not mutually exclusive choices. Pick the right tool for each task.

### 🎯 Decision Flowchart

```mermaid
flowchart TD
    Start([What are you building?])

    Start --> Simple[Pure Periodic Task<br/>LED blink, Heartbeat<br/>No state tracking needed]
    Start --> Sequential[Linear Sequence<br/>Sensor: init → warmup → read<br/>Multiple steps, no branching]
    Start --> StateMachine[Has States & Events<br/>Button press, Protocol handler<br/>UI flow, Device modes]
    Start --> Mixed[Multiple Task Types<br/>IoT device, Industrial controller]

    Simple --> ModelA[✅ Use Callback Timers]
    Sequential --> ModelB[✅ Use Coroutines]
    StateMachine --> ModelC[✅ Use StateSmith FSM]
    Mixed --> ModelMix[✅ Mix Models]

    ModelA --> ImplA[See Model A below]
    ModelB --> ImplB[See Model B below]
    ModelC --> ImplC[See Model C below]
    ModelMix --> ImplMix[See Mixed-Mode Example]

    style ModelA fill:#90EE90,stroke:#333,stroke-width:2px
    style ModelB fill:#87CEEB,stroke:#333,stroke-width:2px
    style ModelC fill:#FFB6C1,stroke:#333,stroke-width:2px
    style ModelMix fill:#DDA0DD,stroke:#333,stroke-width:2px
    style Start fill:#FFE4B5,stroke:#333,stroke-width:2px
```

### 📊 Tool Selection Guide

| Tool | Solves | Example | Typical Usage |
|------|--------|---------|---------------|
| **Callback Timers** | Stateless periodic actions | LED blink, heartbeat | ~20% of tasks |
| **Coroutines** | Linear multi-step sequences | Sensor: init → warmup → read | ~30% of tasks |
| **StateSmith FSM** | State & event handling | Button press, protocol, UI | ~50% of tasks |

**💡 Reality Check:** Most real applications use **all three together**. A typical IoT device might have:
- 2-3 callback timers (heartbeat, status LED)
- 1-2 coroutines (sensor polling, data transmission)
- 2-5 state machines (button handling, protocol, power management)

---

### 🎨 Combining Models (The Normal Way)

**Mixing models is not "advanced" - it's the standard approach.** Each timer is independent, so you naturally use the right tool for each task.

#### Mixed-Mode Example

```c
int main(void) {
    init_timer0();

    /* Model A: Callback for simple periodic task */
    safetimer_create_started(1000, TIMER_MODE_REPEAT, heartbeat_send, NULL);

    /* Model B: Coroutine for sensor polling sequence */
    static sensor_ctx_t sensor_ctx = {0};
    safetimer_handle_t h_sensor = safetimer_create(
        10, TIMER_MODE_REPEAT, sensor_coro, &sensor_ctx
    );
    sensor_ctx._coro_handle = h_sensor;
    safetimer_start(h_sensor);

    /* Model C: StateSmith FSM for protocol handling */
    static protocol_fsm_t protocol_fsm = {0};
    protocol_fsm_init(&protocol_fsm);
    safetimer_create_started(100, TIMER_MODE_REPEAT, protocol_fsm_tick, &protocol_fsm);

    while (1) {
        safetimer_process();  /* Processes all timers */
    }
}
```

#### Real-World Examples

| Application Type | Natural Tool Mix | Why Each Tool? |
|------------------|------------------|----------------|
| **IoT Device** | All three | Heartbeat (callback), sensor polling (coroutine), network protocol (FSM) |
| **Industrial Controller** | Callbacks + FSM | Status LED (callback), control logic & safety interlocks (FSM) |
| **Smart Sensor** | Coroutines + FSM | Sensor sequence (coroutine), power modes & button (FSM) |
| **Simple Appliance** | Callbacks + FSM | Timer display (callback), user interface (FSM) |

**💡 Design Approach:**
1. Start by identifying your tasks
2. Pick the natural tool for each task
3. Don't force everything into one model

See [Coroutines Tutorial](coroutines.md#mixed-mode-architecture) for detailed examples.

---

## Step 4: Implementation Examples

Below are examples for each model. Remember: you'll typically use multiple models in one application.

### Model A: Callback Timers

**Use when:** Stateless periodic actions - no state tracking needed.

#### Basic Usage (Core API)

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

    if (led_timer == SAFETIMER_INVALID_HANDLE) {
        /* Handle error: timer pool full */
        error_handler();
    }

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

### Model B: Coroutines (v1.3.0+)

**Use when:** Linear multi-step sequences with delays between steps.

SafeTimer supports **stackless coroutines** (Protothread-style) for linear async programming. Perfect for UART timeouts, sensor polling, and multi-step initialization.

**Setup:** No configuration needed - just include the header:
```c
#include "safetimer_coro.h"  // Enables coroutine macros
```

#### Quick Example: UART Protocol with Timeout

```c
#include "safetimer.h"
#include "safetimer_coro.h"

typedef struct {
    SAFETIMER_CORO_CONTEXT;  /* Must be first member */
    uint8_t response[32];
    uint8_t response_len;
    uint8_t retry_count;
    uint16_t timeout_counter;
} uart_ctx_t;

void uart_protocol_coro(void *user_data) {
    uart_ctx_t *ctx = (uart_ctx_t *)user_data;

    SAFETIMER_CORO_BEGIN(ctx);

    while (1) {
        /* Send command */
        uart_send_command(0xA5);
        ctx->timeout_counter = 0;

        /* Wait for response with 300ms timeout (poll every 10ms) */
        while (ctx->timeout_counter < 30) {  /* 30 * 10ms = 300ms */
            if (uart_rx_ready()) {
                /* Response received */
                ctx->response_len = uart_read(ctx->response, sizeof(ctx->response));
                process_response(ctx->response, ctx->response_len);
                break;
            }
            ctx->timeout_counter++;
            SAFETIMER_CORO_SLEEP(10);  /* Poll every 10ms, returns here! */
        }

        /* Check if timeout occurred */
        if (ctx->timeout_counter >= 30) {
            /* Timeout - retry */
            ctx->retry_count++;
            if (ctx->retry_count >= 3) {
                handle_timeout_error();
                ctx->retry_count = 0;
            }
        } else {
            ctx->retry_count = 0;  /* Success, reset retry counter */
        }

        SAFETIMER_CORO_SLEEP(1000);  /* Next command in 1s */
    }

    SAFETIMER_CORO_END();
}

int main(void) {
    static uart_ctx_t ctx = {0};

    init_timer0();
    init_uart();

    /* Create coroutine timer (MUST use TIMER_MODE_REPEAT) */
    safetimer_handle_t h = safetimer_create(
        10, TIMER_MODE_REPEAT, uart_protocol_coro, &ctx
    );
    ctx._coro_handle = h;  /* Store handle for SLEEP */
    safetimer_start(h);

    while (1) {
        safetimer_process();
    }
}
```

**What This Example Shows:**
- **Protocol flow:** Send → Wait (with timeout) → Process → Retry → Repeat
- **Timeout handling:** Inner `while` loop polls for 300ms (30 × 10ms)
- **Non-blocking:** Each `SLEEP(10)` exits function, allowing other tasks to run
- **Retry logic:** Automatic retry on timeout (max 3 attempts)

**💡 Key Insight: The Inner `while` Loop is NOT Blocking!**

```c
while (ctx->timeout_counter < 30) {  // Looks like blocking loop
    if (uart_rx_ready()) break;
    ctx->timeout_counter++;
    SAFETIMER_CORO_SLEEP(10);  // ← Returns from function!
}
```

**What actually happens:**
- **Call 1:** Send command, counter=0, check UART, `SLEEP(10)` → **return**
- **Call 2:** Resume, counter=1, check UART, `SLEEP(10)` → **return**
- **Call 3-30:** Same pattern, each call increments counter
- **Call 31:** counter=30, exit loop, handle timeout

**Total:** 30 function calls over 300ms (completely non-blocking)

**Coroutine Macros:**
- `SAFETIMER_CORO_SLEEP(ms)` - Sleep for specified milliseconds (exits function)
- `SAFETIMER_CORO_YIELD()` - Explicit yield (returns immediately)
- `SAFETIMER_CORO_RESET()` - Restart coroutine from beginning
- `SAFETIMER_CORO_EXIT()` - Exit coroutine permanently

**✅ Good Use Cases:**
- **Protocol handlers:** UART, I2C, SPI with timeouts and retries
- **Sensor sequences:** Power on → warmup → read → process
- **Multi-step flows:** Initialization, calibration, data transmission

**❌ Avoid For:**
- Simple periodic tasks (use callbacks: LED blink, heartbeat)
- Complex branching logic (use StateSmith FSM: button states, UI flows)

See [Coroutines Tutorial](coroutines.md) for complete guide with semaphores.

---

### Model C: StateSmith FSM (Recommended for Most Apps)

**Use when:** Any logic with states and events - from simple button debounce to complex protocols.

**Why FSM?** State machines make code **self-documenting** and **bug-resistant**. Even 2-3 state logic is clearer as FSM than scattered if-else.

[StateSmith](https://github.com/StateSmith/StateSmith) generates efficient C code from UML state diagrams. SafeTimer provides the **clock source** for FSM tick events.

#### Integration Pattern

**Step 1: Define State Machine**

StateSmith uses UML state diagrams to generate C code. See the [official documentation](https://github.com/StateSmith/StateSmith/wiki/Behaviors) for syntax and examples.

**Example state machine definition:**
- Define states (Idle, Connecting, Connected)
- Define transitions with events and guards
- Add entry/exit actions and internal behaviors

**Step 2: Generate C Code**

Use StateSmith CLI to generate C code from your state machine definition. See [CLI Usage](https://github.com/StateSmith/StateSmith/wiki/CLI:-Usage) for details.

```bash
statesmith run --file protocol.csx
# Generates: protocol_fsm.h, protocol_fsm.c
```

**Step 3: Integrate with SafeTimer**

```c
#include "safetimer.h"
#include "protocol_fsm.h"  /* StateSmith-generated */

static protocol_fsm_t g_fsm;

/* Timer callback: dispatch tick event to FSM */
void fsm_tick_callback(void *user_data) {
    protocol_fsm_t *fsm = (protocol_fsm_t *)user_data;
    protocol_fsm_dispatch(fsm, PROTOCOL_FSM_EVENT_TICK);
}

int main(void) {
    init_timer0();
    protocol_fsm_init(&g_fsm);

    /* Create 100ms tick timer for FSM */
    safetimer_handle_t h_fsm = safetimer_create(
        100, TIMER_MODE_REPEAT, fsm_tick_callback, &g_fsm
    );
    safetimer_start(h_fsm);

    while (1) {
        safetimer_process();

        /* External events can be dispatched directly */
        if (button_pressed()) {
            protocol_fsm_dispatch(&g_fsm, PROTOCOL_FSM_EVENT_CONNECT);
        }
    }
}
```

**✅ Good Use Cases (Most Applications!):**
- **Button handling:** Debounce, single/double/long press detection (even simple debounce is clearer as FSM)
- **Protocol handlers:** UART, I2C, SPI communication with timeouts
- **UI flows:** Menu navigation, screen transitions, user interactions
- **Device control:** Power management, mode switching, error recovery
- **Simple logic:** Even 2-3 state logic (idle/active/error) benefits from FSM clarity

**Example: Button Debounce FSM (3 states)**
```
States: IDLE → PRESSED → DEBOUNCING
Events: button_down, button_up, timeout
Result: Clear, testable, no hidden bugs
```

**❌ Avoid For:**
- Pure periodic tasks with no state (use callbacks: LED blink, heartbeat)
- Strictly linear sequences (use coroutines: sensor init → read → transmit)

See [Coroutines Tutorial - StateSmith Integration](coroutines.md#statesmith-integration) for complete examples.

---

## Next Steps

- **Configuration:** [Configuration & Tuning](configuration-and-tuning.md)
- **Real-world Examples:** [Use Cases & Best Practices](use-cases.md)
- **Hardware Porting:** [BSP Porting Guide](bsp-porting.md)
- **API Reference:** See `include/safetimer.h` for complete API documentation
