# Use Cases & Best Practices

This guide covers common SafeTimer usage patterns and anti-patterns.

---

## ✅ What SafeTimer is Good For

SafeTimer excels at **asynchronous timeout management** and **periodic task scheduling**.

### 1. Periodic Tasks

LED blinking, heartbeat packets, watchdog feeding:

```c
/* LED blinking every 500ms */
safetimer_handle_t h_led = safetimer_create(
    500, TIMER_MODE_REPEAT, led_blink, NULL
);

/* Heartbeat packet every 1 second */
safetimer_handle_t h_heartbeat = safetimer_create(
    1000, TIMER_MODE_REPEAT, send_heartbeat, NULL
);

safetimer_start(h_led);
safetimer_start(h_heartbeat);
```

---

### 2. Communication Timeout

Send data and wait for ACK with timeout:

```c
static safetimer_handle_t h_timeout;

void send_packet(void) {
    uart_send(data);

    /* Create 3-second timeout */
    h_timeout = safetimer_create(
        3000, TIMER_MODE_ONE_SHOT, timeout_cb, NULL
    );
    safetimer_start(h_timeout);
}

void on_ack_received(void) {
    /* Cancel timeout */
    safetimer_delete(h_timeout);
    h_timeout = SAFETIMER_INVALID_HANDLE;
}

void timeout_cb(void *data) {
    /* Handle timeout */
    retry_send();
}
```

---

### 3. Multi-Stage State Machines

Power-on sequence with delays:

```c
void power_on(void) {
    /* Stage 1: Wait 2 seconds before init */
    h1 = safetimer_create(
        2000, TIMER_MODE_ONE_SHOT, init_sensor_cb, NULL
    );
    safetimer_start(h1);
}

void init_sensor_cb(void *data) {
    init_sensor();

    /* Stage 2: Wait 5 seconds before comm */
    h2 = safetimer_create(
        5000, TIMER_MODE_ONE_SHOT, start_comm_cb, NULL
    );
    safetimer_start(h2);
}

void start_comm_cb(void *data) {
    start_communication();
}
```

---

### 4. Delayed Actions

Turn off LED after 10 seconds:

```c
void led_auto_off(void) {
    led_on();

    h_off = safetimer_create(
        10000, TIMER_MODE_ONE_SHOT, led_off_cb, NULL
    );
    safetimer_start(h_off);
}

void led_off_cb(void *data) {
    led_off();
    safetimer_delete(h_off);
}
```

---

## ❌ What SafeTimer is NOT For

### 1. Button Debouncing

**❌ Wasteful Approach:**
```c
/* 14 bytes RAM + 1 timer slot for simple task */
void button_isr(void) {
    h = safetimer_create(20, TIMER_MODE_ONE_SHOT, debounce_cb, NULL);
    safetimer_start(h);
}
```

**✅ Efficient Approach:**
```c
/* 6 bytes RAM, no timer slot needed */
static uint8_t g_last_state = 1;
static uint32_t g_last_change_time = 0;

void key_scan(void) {
    uint8_t current = BUTTON_PIN;
    uint32_t now = bsp_get_ticks();

    if (current != g_last_state) {
        if ((now - g_last_change_time) >= 20) {  /* 20ms debounce */
            g_last_state = current;
            g_last_change_time = now;

            if (current == 0) {
                /* Button pressed */
                handle_button_press();
            }
        }
    }
}
```

**Why?** Button debouncing requires frequent state checks (every main loop iteration). Creating/deleting timers for each press wastes RAM and CPU.

---

### 2. High-Frequency Polling

**❌ Don't use timers:**
```c
/* Polling every 1ms → CPU overhead */
h = safetimer_create(1, TIMER_MODE_REPEAT, poll_cb, NULL);
```

**✅ Use direct checking:**
```c
while (1) {
    if (data_ready()) {
        process_data();
    }
    /* Other tasks */
}
```

---

### 3. Microsecond Precision

SafeTimer uses **1ms tick resolution** → not suitable for µs-level timing.

**❌ Don't use SafeTimer:**
```c
/* Needs 100µs delays */
h = safetimer_create(0, TIMER_MODE_ONE_SHOT, ...);  /* Won't work */
```

**✅ Use hardware timers directly:**
```c
/* Configure hardware timer for µs precision */
TIM_SetPeriod(TIM2, 100);  /* 100µs */
```

---

### 4. Hard Real-Time Requirements

SafeTimer callbacks depend on `safetimer_process()` call frequency → timing is "soft real-time".

**❌ Critical timing:**
```c
/* Motor control requires exact 50µs pulses */
h = safetimer_create(0, ...);  /* Not reliable */
```

**✅ Use hardware interrupts:**
```c
void TIM_IRQHandler(void) {
    /* Exact timing guaranteed */
    motor_pulse();
}
```

---

## 📐 Design Guidelines

### Timer Slot Allocation Strategy

**Example: MAX_TIMERS = 4**

```c
/* Static timers (70-80%): Created once, never deleted */
Slot 0: LED blink (500ms REPEAT)        // Always running
Slot 1: Heartbeat (1000ms REPEAT)       // Always running

/* Dynamic timers (20-30%): Created/deleted as needed */
Slot 2: Communication timeout (temp)    // Created per transaction
Slot 3: Delayed action (temp)           // Created on-demand
```

**Benefits:**
- Minimizes pool fragmentation
- Predictable RAM usage
- Reduces create/delete overhead

---

### When to use `safetimer_delete()`

**✅ Use delete for:**

1. **Timeout cancellation:**
   ```c
   void on_ack_received(void) {
       safetimer_delete(h_timeout);  /* ACK arrived, cancel timeout */
   }
   ```

2. **Temporary delayed actions:**
   ```c
   void cleanup_temp_timer(void) {
       safetimer_delete(h_temp);  /* One-time action done */
   }
   ```

3. **Test teardown:**
   ```c
   void tearDown(void) {
       safetimer_delete(h_test_timer);  /* Clean up test state */
   }
   ```

**❌ DON'T use delete for:**

1. **Static periodic timers:**
   ```c
   /* ❌ Bad: Deleting LED blink timer */
   safetimer_delete(h_led);  /* Creates fragmentation */

   /* ✅ Good: Keep it running forever */
   safetimer_start(h_led);  /* Create once, run forever */
   ```

2. **Frequent create/delete cycles:**
   ```c
   /* ❌ Bad: Creating/deleting every button press */
   void button_cb(void) {
       h = safetimer_create(...);  /* Slot thrashing */
       safetimer_delete(h);
   }

   /* ✅ Good: Create once, reuse */
   h = safetimer_create(...);  /* Created at init */
   safetimer_start(h);         /* Reused on each press */
   ```

---

## 🎯 Common Patterns

### Pattern 1: Watchdog Kicking

```c
void watchdog_cb(void *data) {
    kick_watchdog();
}

h_wdt = safetimer_create(500, TIMER_MODE_REPEAT, watchdog_cb, NULL);
safetimer_start(h_wdt);
```

---

### Pattern 2: Retry with Exponential Backoff

```c
typedef struct {
    int retry_count;
    uint32_t delay_ms;
} retry_ctx_t;

void retry_cb(void *data) {
    retry_ctx_t *ctx = (retry_ctx_t *)data;

    if (send_data()) {
        safetimer_delete(h_retry);  /* Success */
    } else if (ctx->retry_count++ < 5) {
        /* Exponential backoff */
        ctx->delay_ms *= 2;
        safetimer_set_period(h_retry, ctx->delay_ms);
        safetimer_start(h_retry);
    } else {
        /* Max retries exceeded */
        handle_failure();
    }
}
```

---

### Pattern 3: Cascaded Timers

```c
void timer1_cb(void *data) {
    stage1_complete();

    /* Start next stage */
    h2 = safetimer_create(1000, TIMER_MODE_ONE_SHOT, timer2_cb, NULL);
    safetimer_start(h2);
}

void timer2_cb(void *data) {
    stage2_complete();

    /* Start final stage */
    h3 = safetimer_create(500, TIMER_MODE_ONE_SHOT, timer3_cb, NULL);
    safetimer_start(h3);
}
```

---

### Pattern 4: Conditional Timer Start

```c
h = safetimer_create(1000, TIMER_MODE_ONE_SHOT, callback, NULL);

/* Don't start immediately - wait for condition */
if (system_ready()) {
    safetimer_start(h);
}
```

---

## 🛡️ Safety Guidelines

1. **Always check return values:**
   ```c
   safetimer_handle_t h = safetimer_create(...);
   if (h == SAFETIMER_INVALID_HANDLE) {
       /* Handle error */
   }
   ```

2. **Store handles safely:**
   ```c
   /* ✅ Good: Static handle */
   static safetimer_handle_t h_led;

   /* ❌ Bad: Local handle (lost after return) */
   void init(void) {
       safetimer_handle_t h = safetimer_create(...);
       /* h lost after function returns! */
   }
   ```

3. **Don't delete inside callback:**
   ```c
   void callback(void *data) {
       safetimer_delete(h_self);  /* ⚠️ Dangerous */
   }
   ```

4. **Use `ENABLE_PARAM_CHECK=1` in development:**
   ```c
   #define ENABLE_PARAM_CHECK 1  /* Catch bugs early */
   ```

---

## 📖 Next Steps

- **Configuration:** [Configuration & Tuning](configuration-and-tuning.md)
- **Advanced Patterns:** [Coroutines Tutorial](coroutines.md)
- **Implementation:** [Design Guidelines](design-guidelines.md)

---

**Remember:** SafeTimer is for **soft real-time** async task management, not hard real-time control loops.
