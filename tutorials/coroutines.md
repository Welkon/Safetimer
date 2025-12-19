# Coroutines Tutorial (v1.3.0+)

**Prerequisites:** Read [Quick Start Guide](quick-start.md) first for basic coroutine usage.

This tutorial covers **advanced coroutine features**: semaphores, StateSmith integration, and complex patterns.

---

## 📚 Advanced Example: Multi-Task Coordination

```c
#include "safetimer.h"
#include "safetimer_coro.h"

typedef struct {
    SAFETIMER_CORO_CONTEXT;  /* Must be first member */
    int counter;
    uint8_t sensor_data;
} my_coro_ctx_t;

void sensor_task_coro(void *user_data) {
    my_coro_ctx_t *ctx = (my_coro_ctx_t *)user_data;

    SAFETIMER_CORO_BEGIN(ctx);

    while (1) {
        /* Step 1: Initialize sensor */
        sensor_init();
        SAFETIMER_CORO_SLEEP(100);   /* Wait 100ms for sensor warmup */

        /* Step 2: Read sensor data */
        ctx->sensor_data = sensor_read();
        SAFETIMER_CORO_SLEEP(50);    /* Wait 50ms between reads */

        /* Step 3: Process and transmit */
        uart_send(ctx->sensor_data);

        /* Step 4: Wait for next cycle */
        SAFETIMER_CORO_SLEEP(1000);  /* Poll every 1 second */

        ctx->counter++;
    }

    SAFETIMER_CORO_END();
}

int main(void) {
    static my_coro_ctx_t ctx = {0};

    init_timer0();

    /* Create coroutine timer (MUST use TIMER_MODE_REPEAT) */
    safetimer_handle_t h = safetimer_create(
        10, TIMER_MODE_REPEAT, sensor_task_coro, &ctx
    );
    ctx._coro_handle = h;  /* Store handle for SLEEP/WAIT_UNTIL */
    safetimer_start(h);

    while (1) {
        safetimer_process();
    }
}
```

**Note:** Basic coroutine macros (`SLEEP`, `YIELD`, `RESET`, `EXIT`) are covered in [Quick Start](quick-start.md#model-b-coroutines-v130).

---

## 🛠️ Advanced Macro: SAFETIMER_CORO_YIELD()

Use `YIELD()` for cooperative multitasking within long operations:

```c
SAFETIMER_CORO_BEGIN(ctx);
    for (int i = 0; i < 100; i++) {
        process_chunk(i);
        SAFETIMER_CORO_YIELD();  /* Yield after each chunk, allows other tasks */
    }
SAFETIMER_CORO_END();
```

**Use case:** Processing large data buffers without blocking other timers.

```c
SAFETIMER_CORO_BEGIN(ctx);
    if (task_complete()) {
        SAFETIMER_CORO_EXIT();  /* Stop coroutine */
    }
    /* Continue running */
SAFETIMER_CORO_END();
```

---

## 🔔 Semaphore Support

SafeTimer provides counting semaphores for inter-task synchronization.

### Basic Semaphore Example

```c
#include "safetimer_sem.h"

static volatile safetimer_sem_t data_ready_sem = 0;

/* Producer (interrupt) */
void uart_rx_isr(void) {
    SAFETIMER_SEM_SIGNAL(data_ready_sem);
}

/* Consumer (coroutine) */
void uart_task_coro(void *user_data) {
    my_coro_ctx_t *ctx = (my_coro_ctx_t *)user_data;

    SAFETIMER_CORO_BEGIN(ctx);

    while (1) {
        /* Wait for data with 3000ms timeout */
        SAFETIMER_CORO_WAIT_SEM(data_ready_sem, 10, 300);

        if (data_ready_sem == SAFETIMER_SEM_TIMEOUT) {
            handle_timeout();
        } else {
            process_data();
        }
    }

    SAFETIMER_CORO_END();
}
```

### Semaphore Macros

**SAFETIMER_SEM_SIGNAL(sem)** - Signal semaphore (interrupt-safe)
```c
void data_ready_isr(void) {
    SAFETIMER_SEM_SIGNAL(my_sem);
}
```

**SAFETIMER_CORO_WAIT_SEM(sem, poll_ms, max_polls)** - Wait for semaphore with timeout
```c
SAFETIMER_CORO_WAIT_SEM(data_ready_sem, 10, 100);  /* Max 1000ms */
if (data_ready_sem == SAFETIMER_SEM_TIMEOUT) {
    /* Timeout handling */
}
```

---

## 🎨 When to Use Coroutines

### ✅ Good Use Cases

**1. UART Communication with Timeouts**
```c
SAFETIMER_CORO_BEGIN(ctx);
    uart_send(command);
    SAFETIMER_CORO_WAIT_UNTIL(uart_rx_ready(), 10);
    response = uart_read();
SAFETIMER_CORO_END();
```

**2. Sensor Polling Sequences**
```c
SAFETIMER_CORO_BEGIN(ctx);
    sensor_start();
    SAFETIMER_CORO_SLEEP(50);      /* Warmup delay */
    data = sensor_read();
    SAFETIMER_CORO_SLEEP(1000);    /* Next poll */
SAFETIMER_CORO_END();
```

**3. Multi-Step Initialization**
```c
SAFETIMER_CORO_BEGIN(ctx);
    power_on();
    SAFETIMER_CORO_SLEEP(500);
    module_init();
    SAFETIMER_CORO_SLEEP(1000);
    start_operation();
SAFETIMER_CORO_END();
```

### ❌ Avoid Coroutines For

**1. Simple Periodic Tasks** - Use callbacks instead
```c
/* ✅ Better with callback */
void led_toggle(void *data) {
    toggle_led();
}
safetimer_handle_t h = safetimer_create(500, TIMER_MODE_REPEAT, led_toggle, NULL);
safetimer_start(h);
```

**2. One-Shot Delays** - Use TIMER_MODE_ONE_SHOT
```c
/* ✅ Better with one-shot timer */
safetimer_handle_t h = safetimer_create(5000, TIMER_MODE_ONE_SHOT, shutdown_cb, NULL);
safetimer_start(h);
```

---

## 🔧 Mixed-Mode Architecture

You can combine callbacks, StateSmith FSMs, and coroutines in one application:

```c
/* Callback: Simple periodic task */
void heartbeat_cb(void *data) {
    send_heartbeat();
}

/* Coroutine: Complex sensor polling */
void sensor_coro(void *data) {
    my_coro_ctx_t *ctx = (my_coro_ctx_t *)data;
    SAFETIMER_CORO_BEGIN(ctx);
    while (1) {
        sensor_poll_sequence();
        SAFETIMER_CORO_SLEEP(1000);
    }
    SAFETIMER_CORO_END();
}

/* StateSmith FSM: Complex event-driven logic */
void protocol_fsm_cb(void *data) {
    protocol_fsm_dispatch(EVENT_TICK);
}

int main(void) {
    /* Create all timers */
    safetimer_handle_t h1 = safetimer_create(1000, TIMER_MODE_REPEAT, heartbeat_cb, NULL);
    safetimer_handle_t h2 = safetimer_create(10, TIMER_MODE_REPEAT, sensor_coro, &sensor_ctx);
    safetimer_handle_t h3 = safetimer_create(100, TIMER_MODE_REPEAT, protocol_fsm_cb, &fsm_ctx);

    safetimer_start(h1);
    safetimer_start(h2);
    safetimer_start(h3);

    /* ... */
}
```

See [`examples/coroutine_demo/`](../examples/coroutine_demo/) for complete mixed-mode examples.

---

## 🎨 StateSmith Integration

**Prerequisites:** Read [Quick Start - StateSmith FSM](quick-start.md#model-c-statesmith-fsm-recommended-for-most-apps) for basic integration.

This section covers **advanced patterns**: FSM + Coroutine combinations for complex protocols.

### Advanced Pattern: FSM with Coroutine Actions

```c
#include "safetimer.h"
#include "protocol_fsm.h"  /* StateSmith-generated */

/* StateSmith context (generated) */
static protocol_fsm_t g_fsm;

/* Timer callback: dispatch tick event to FSM */
void fsm_tick_callback(void *user_data) {
    protocol_fsm_t *fsm = (protocol_fsm_t *)user_data;

    /* StateSmith-generated dispatch function */
    protocol_fsm_dispatch(fsm, PROTOCOL_FSM_EVENT_TICK);
}

int main(void) {
    /* Initialize StateSmith FSM */
    protocol_fsm_init(&g_fsm);

    /* Create SafeTimer for FSM clock (100ms tick) */
    safetimer_handle_t h_fsm = safetimer_create(
        100,                    /* 100ms tick period */
        TIMER_MODE_REPEAT,      /* Repeating timer */
        fsm_tick_callback,      /* Dispatch tick event */
        &g_fsm                  /* FSM context */
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

---

### Complete Example: UART Protocol FSM

**State Machine Definition:**

```c
/* States */
typedef enum {
    UART_IDLE,
    UART_WAIT_ACK,
    UART_CONNECTED,
    UART_ERROR
} uart_state_t;

/* Events */
typedef enum {
    UART_EVENT_TICK,        /* From SafeTimer */
    UART_EVENT_SEND,        /* User action */
    UART_EVENT_ACK,         /* UART interrupt */
    UART_EVENT_TIMEOUT      /* Generated by tick counter */
} uart_event_t;

/* FSM Context (StateSmith-generated structure) */
typedef struct {
    uart_state_t state;
    uint32_t timeout_counter;
    uint8_t retry_count;
} uart_fsm_t;
```

**StateSmith-Generated Dispatch Function:**

```c
void uart_fsm_dispatch(uart_fsm_t *fsm, uart_event_t event) {
    switch (fsm->state) {
        case UART_IDLE:
            if (event == UART_EVENT_SEND) {
                uart_send_packet();
                fsm->timeout_counter = 0;
                fsm->state = UART_WAIT_ACK;
            }
            break;

        case UART_WAIT_ACK:
            if (event == UART_EVENT_TICK) {
                fsm->timeout_counter++;
                if (fsm->timeout_counter >= 30) {  /* 3000ms / 100ms */
                    fsm->state = UART_ERROR;
                }
            } else if (event == UART_EVENT_ACK) {
                fsm->state = UART_CONNECTED;
            }
            break;

        case UART_CONNECTED:
            /* Handle connected state */
            break;

        case UART_ERROR:
            /* Handle error state */
            break;
    }
}
```

**SafeTimer Integration:**

```c
static uart_fsm_t g_uart_fsm = {0};

void uart_fsm_tick(void *user_data) {
    uart_fsm_t *fsm = (uart_fsm_t *)user_data;
    uart_fsm_dispatch(fsm, UART_EVENT_TICK);
}

void setup_uart_fsm(void) {
    /* Initialize FSM */
    g_uart_fsm.state = UART_IDLE;

    /* Create 100ms tick timer for FSM */
    safetimer_handle_t h = safetimer_create(
        100, TIMER_MODE_REPEAT, uart_fsm_tick, &g_uart_fsm
    );
    safetimer_start(h);
}

/* External event injection (from UART ISR) */
void uart_rx_isr(void) {
    if (is_ack_packet()) {
        uart_fsm_dispatch(&g_uart_fsm, UART_EVENT_ACK);
    }
}
```

---

### When to Use Each Pattern

| Pattern | Best For | Example |
|---------|----------|---------|
| **Callbacks** | Simple periodic tasks | LED blink, heartbeat |
| **Coroutines** | Linear sequences with delays | Sensor init → read → transmit |
| **StateSmith FSM** | Complex event-driven logic | Protocol state machines, UI flows |

**Decision Tree:**

```
Is the logic event-driven with multiple states?
├─ YES → Use StateSmith FSM
└─ NO → Is it a linear sequence with delays?
    ├─ YES → Use Coroutines
    └─ NO → Use Callbacks
```

---

### Advantages of StateSmith + SafeTimer

**✅ Benefits:**
1. **Visual Design:** UML diagrams are easier to understand than code
2. **Automatic Code Generation:** Reduces manual errors
3. **Precise Timing:** SafeTimer provides reliable tick source
4. **Zero Overhead:** StateSmith generates efficient C code
5. **Testability:** FSM logic can be unit tested independently

**⚠️ Trade-offs:**
- Requires StateSmith tool installation
- Adds build step (diagram → C code generation)
- Learning curve for UML state diagrams

---

### Real-World Use Case: IoT Device

```c
/* Three patterns working together */

/* 1. Callback: Heartbeat transmission (simple) */
void heartbeat_cb(void *data) {
    send_heartbeat();
}

/* 2. Coroutine: Sensor reading (linear sequence) */
void sensor_coro(void *data) {
    sensor_ctx_t *ctx = (sensor_ctx_t *)data;
    SAFETIMER_CORO_BEGIN(ctx);
    while (1) {
        sensor_power_on();
        SAFETIMER_CORO_SLEEP(100);      /* Warmup */
        ctx->data = sensor_read();
        SAFETIMER_CORO_SLEEP(5000);     /* Next reading */
    }
    SAFETIMER_CORO_END();
}

/* 3. StateSmith FSM: Network protocol (event-driven) */
void network_fsm_tick(void *data) {
    network_fsm_dispatch((network_fsm_t *)data, EVENT_TICK);
}

int main(void) {
    /* Create all timers */
    safetimer_create_started(1000, TIMER_MODE_REPEAT, heartbeat_cb, NULL);

    sensor_ctx_t sensor_ctx = {0};
    safetimer_handle_t h_sensor = safetimer_create(
        10, TIMER_MODE_REPEAT, sensor_coro, &sensor_ctx
    );
    sensor_ctx._coro_handle = h_sensor;
    safetimer_start(h_sensor);

    network_fsm_t net_fsm = {0};
    safetimer_create_started(100, TIMER_MODE_REPEAT, network_fsm_tick, &net_fsm);

    while (1) {
        safetimer_process();
    }
}
```

---

## ⚠️ Important Constraints

1. **MUST use TIMER_MODE_REPEAT** - Coroutines require repeating timers
2. **Store handle in context** - Required for SLEEP/WAIT_UNTIL macros
3. **Context must be static** - Coroutine state persists across invocations
4. **No blocking calls** - Coroutine body must return quickly
5. **SAFETIMER_CORO_CONTEXT first** - Must be first member of context struct

---

## 📖 Next Steps

- **Best Practices:** [Use Cases & Best Practices](use-cases.md)
- **Performance Tuning:** [Configuration & Tuning](configuration-and-tuning.md)
- **API Reference:** See `include/safetimer_coro.h` for complete coroutine API

---

**Made with ❤️ for embedded developers seeking async clarity**
