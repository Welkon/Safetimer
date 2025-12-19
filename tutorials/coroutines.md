# Coroutines Tutorial (v1.3.0+)

SafeTimer v1.3.0 introduces **stackless coroutines** (Protothread-style) for linear async programming, perfect for UART timeouts, sensor polling, and state machines.

---

## 🎯 Why Coroutines?

Traditional callback-based timers require splitting logic across multiple functions:

```c
/* ❌ Callback style: logic split across functions */
void step1_callback(void *data) {
    sensor_init();
    safetimer_create(2000, TIMER_MODE_ONE_SHOT, step2_callback, NULL);
}

void step2_callback(void *data) {
    sensor_read();
    safetimer_create(1000, TIMER_MODE_ONE_SHOT, step3_callback, NULL);
}
```

**Coroutines enable linear code flow:**

```c
/* ✅ Coroutine style: sequential logic */
SAFETIMER_CORO_BEGIN(ctx);
    sensor_init();
    SAFETIMER_CORO_SLEEP(2000);

    sensor_read();
    SAFETIMER_CORO_SLEEP(1000);

    process_data();
SAFETIMER_CORO_END();
```

---

## 📚 Complete Example

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

---

## 🛠️ Coroutine Macros

### SAFETIMER_CORO_SLEEP(ms)

Sleep for specified milliseconds (zero cumulative drift, v1.3.1+).

```c
SAFETIMER_CORO_BEGIN(ctx);
    led_on();
    SAFETIMER_CORO_SLEEP(100);   /* Sleep 100ms */
    led_off();
    SAFETIMER_CORO_SLEEP(900);   /* Sleep 900ms */
SAFETIMER_CORO_END();
```

### SAFETIMER_CORO_WAIT_UNTIL(condition, poll_ms)

Wait until condition becomes true, polling every `poll_ms` milliseconds.

```c
SAFETIMER_CORO_BEGIN(ctx);
    uart_send(cmd);
    SAFETIMER_CORO_WAIT_UNTIL(uart_rx_ready(), 10);  /* Poll every 10ms */
    data = uart_read();
SAFETIMER_CORO_END();
```

### SAFETIMER_CORO_YIELD()

Explicit yield control (returns immediately, resumes on next timer callback).

```c
SAFETIMER_CORO_BEGIN(ctx);
    for (int i = 0; i < 100; i++) {
        process_chunk(i);
        SAFETIMER_CORO_YIELD();  /* Yield after each chunk */
    }
SAFETIMER_CORO_END();
```

### SAFETIMER_CORO_RESET()

Restart coroutine from beginning.

```c
SAFETIMER_CORO_BEGIN(ctx);
    if (error_occurred()) {
        SAFETIMER_CORO_RESET();  /* Restart from top */
    }
    /* Normal flow */
SAFETIMER_CORO_END();
```

### SAFETIMER_CORO_EXIT()

Exit coroutine permanently (stops timer).

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
h = safetimer_create(500, TIMER_MODE_REPEAT, led_toggle, NULL);
```

**2. One-Shot Delays** - Use TIMER_MODE_ONE_SHOT
```c
/* ✅ Better with one-shot timer */
h = safetimer_create(5000, TIMER_MODE_ONE_SHOT, shutdown_cb, NULL);
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
    h1 = safetimer_create(1000, TIMER_MODE_REPEAT, heartbeat_cb, NULL);
    h2 = safetimer_create(10, TIMER_MODE_REPEAT, sensor_coro, &sensor_ctx);
    h3 = safetimer_create(100, TIMER_MODE_REPEAT, protocol_fsm_cb, &fsm_ctx);

    /* ... */
}
```

See [`examples/coroutine_demo/`](../examples/coroutine_demo/) for complete mixed-mode examples.

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
