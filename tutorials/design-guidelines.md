# Design Guidelines

This guide covers timer pool management and lifecycle best practices.

---

## 🎯 Timer Pool Management

### Pool Allocation Strategy

**Recommended split:**
- **70-80% static timers** (created once, never deleted)
- **20-30% dynamic timers** (created/deleted as needed)

**Example with MAX_TIMERS = 4:**

```c
/* Static timers: Created at initialization */
static safetimer_handle_t h_led;       // Slot 0
static safetimer_handle_t h_heartbeat; // Slot 1

void init(void) {
    h_led = safetimer_create(500, TIMER_MODE_REPEAT, led_blink, NULL);
    h_heartbeat = safetimer_create(1000, TIMER_MODE_REPEAT, send_heartbeat, NULL);

    safetimer_start(h_led);
    safetimer_start(h_heartbeat);
}

/* Dynamic timers: Created/deleted on demand */
void send_packet(void) {
    safetimer_handle_t h_timeout = safetimer_create(
        3000, TIMER_MODE_ONE_SHOT, timeout_cb, NULL
    );  // Uses Slot 2 or 3
    safetimer_start(h_timeout);
}
```

---

## 🔄 Timer Lifecycle Patterns

### Pattern 1: Static Timers (Create Once, Run Forever)

**Best for:** LED blink, heartbeat, watchdog, periodic sensor polling

```c
/* ✅ Good: Create at init, never delete */
static safetimer_handle_t h_periodic;

void app_init(void) {
    h_periodic = safetimer_create(
        1000, TIMER_MODE_REPEAT, periodic_task, NULL
    );
    safetimer_start(h_periodic);
}

/* Timer runs forever - no delete needed */
```

**Benefits:**
- Zero pool fragmentation
- Predictable RAM usage
- Minimal CPU overhead

---

### Pattern 2: Dynamic Timers (Create/Delete as Needed)

**Best for:** Timeouts, one-shot delays, temporary sequences

```c
/* ✅ Good: Create when needed, delete when done */
void send_with_timeout(void) {
    safetimer_handle_t h = safetimer_create(
        3000, TIMER_MODE_ONE_SHOT, timeout_cb, NULL
    );
    safetimer_start(h);

    uart_send(data);

    /* Timeout callback will delete timer */
}

void timeout_cb(void *data) {
    handle_timeout();
    safetimer_delete(h);  /* Free slot */
}
```

**Benefits:**
- Flexible resource usage
- Suitable for unpredictable workloads

**Cost:**
- Potential pool fragmentation
- CPU overhead for create/delete

---

### Pattern 3: Pre-allocated Pools (Hybrid)

**Best for:** Frequent but bounded dynamic allocations

```c
/* ✅ Good: Pre-allocate, reuse handles */
#define TIMEOUT_POOL_SIZE 2
static safetimer_handle_t timeout_pool[TIMEOUT_POOL_SIZE];

void init_timeout_pool(void) {
    for (int i = 0; i < TIMEOUT_POOL_SIZE; i++) {
        timeout_pool[i] = safetimer_create(
            3000, TIMER_MODE_ONE_SHOT, timeout_cb, NULL
        );
        /* Don't start yet */
    }
}

void send_packet(int id) {
    safetimer_start(timeout_pool[id % TIMEOUT_POOL_SIZE]);
    uart_send(data);
}
```

**Benefits:**
- No runtime allocation overhead
- Deterministic behavior
- No fragmentation

---

## 🚫 Anti-Patterns to Avoid

### Anti-Pattern 1: Deleting Static Timers

```c
/* ❌ Bad: Deleting LED blink timer */
void disable_led(void) {
    safetimer_delete(h_led);  /* Creates pool fragmentation */
}

/* ✅ Good: Stop instead */
void disable_led(void) {
    safetimer_stop(h_led);  /* Can restart later */
}
```

---

### Anti-Pattern 2: Frequent Create/Delete Cycles

```c
/* ❌ Bad: Creating/deleting every loop */
while (1) {
    safetimer_handle_t h = safetimer_create(...);
    safetimer_start(h);
    delay_ms(100);
    safetimer_delete(h);  /* Slot thrashing */
}

/* ✅ Good: Create once, reuse */
safetimer_handle_t h = safetimer_create(...);
while (1) {
    safetimer_start(h);
    delay_ms(100);
    safetimer_stop(h);  /* Reuse slot */
}
```

---

### Anti-Pattern 3: Ignoring Pool Exhaustion

```c
/* ❌ Bad: No error handling */
safetimer_handle_t h = safetimer_create(...);
safetimer_start(h);  /* Assumes success */

/* ✅ Good: Check return value */
safetimer_handle_t h = safetimer_create(...);
if (h == SAFETIMER_INVALID_HANDLE) {
    /* Handle pool exhaustion */
    cleanup_old_timers();
    h = safetimer_create(...);  /* Retry */
}
```

---

### Anti-Pattern 4: Deleting Inside Callback

```c
/* ❌ Dangerous: Deleting self in callback */
static safetimer_handle_t h_self;

void my_callback(void *data) {
    do_work();
    safetimer_delete(h_self);  /* ⚠️ Undefined behavior */
}

/* ✅ Safe: Use ONE_SHOT mode */
h_self = safetimer_create(
    1000, TIMER_MODE_ONE_SHOT, my_callback, NULL
);
/* ONE_SHOT timers auto-stop after firing */
```

---

## 📏 Pool Sizing Guidelines

### Step 1: Count Static Timers

```
Static Timers:
- LED blink: 1
- Heartbeat: 1
- Watchdog: 1
- Sensor poll: 1
Total: 4 static
```

### Step 2: Estimate Dynamic Timers

```
Dynamic Timers (peak):
- UART timeouts: 2 concurrent
- Temp delays: 1
Total: 3 dynamic (peak)
```

### Step 3: Calculate MAX_TIMERS

```
MAX_TIMERS = Static + Dynamic + Safety Margin
           = 4 + 3 + 1
           = 8 timers
```

**Safety margin:** 10-20% extra capacity for unexpected loads

---

## 🔍 Monitoring Pool Usage

### Check Pool Utilization

```c
#if ENABLE_QUERY_API
void check_pool_health(void) {
    int used, total;
    safetimer_get_pool_usage(&used, &total);

    if (used > (total * 0.8)) {
        /* Pool >80% full - warning */
        log_warning("Timer pool near capacity");
    }
}
#endif
```

### Debug Pool Fragmentation

```c
void debug_timers(void) {
    for (int i = 0; i < MAX_TIMERS; i++) {
        int is_running;
        if (safetimer_get_status(i, &is_running) == TIMER_OK) {
            printf("Slot %d: %s\n", i, is_running ? "active" : "inactive");
        }
    }
}
```

---

## ⚡ Performance Optimization

### Minimize Create/Delete Operations

**Slow:**
```c
/* Creating/deleting every time */
void send_packet(void) {
    h = safetimer_create(...);  /* ~50 CPU cycles */
    safetimer_start(h);
    /* ... */
    safetimer_delete(h);        /* ~30 CPU cycles */
}
```

**Fast:**
```c
/* Pre-allocated, reuse */
static safetimer_handle_t h_timeout;

void init(void) {
    h_timeout = safetimer_create(...);  /* Once at init */
}

void send_packet(void) {
    safetimer_start(h_timeout);  /* ~10 CPU cycles */
    /* ... */
}
```

---

### Batch Timer Operations

```c
/* ✅ Good: Start multiple timers together */
void init_all_timers(void) {
    safetimer_start(h1);
    safetimer_start(h2);
    safetimer_start(h3);
}

/* ❌ Bad: Starting timers scattered across code */
```

---

## 🎯 Configuration Recommendations

### Small Applications (≤4 concurrent tasks)
```c
#define MAX_TIMERS 4
#define ENABLE_QUERY_API 0  /* Save Flash */
```

### Medium Applications (5-10 concurrent tasks)
```c
#define MAX_TIMERS 8
#define ENABLE_QUERY_API 1  /* Enable diagnostics */
```

### Complex Applications (>10 concurrent tasks)
```c
#define MAX_TIMERS 16
#define ENABLE_QUERY_API 1
#define ENABLE_PARAM_CHECK 1  /* Extra safety */
```

---

## 📖 Next Steps

- **Use Cases:** [Use Cases & Best Practices](use-cases.md)
- **Configuration:** [Configuration & Tuning](configuration-and-tuning.md)
- **API Reference:** [../docs/api_reference.md](../docs/api_reference.md)

---

**Golden Rule:** Prefer static timers (70-80%) over dynamic timers (20-30%) for stable, predictable systems.
