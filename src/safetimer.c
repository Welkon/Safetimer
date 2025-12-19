/**
 * @file    safetimer.c
 * @brief   SafeTimer Core Implementation
 * @version 1.2.6
 * @date    2025-12-16
 * @author  SafeTimer Project
 * @license MIT
 *
 * Implementation follows:
 * - DEC-001: BSP Interface (3 functions)
 * - DEC-002: Data Structure (114 bytes RAM)
 * - DEC-003: API Naming (safetimer_ prefix)
 * - ADR-005: Overflow Handling (signed difference algorithm)
 * - Step 5: Implementation Patterns (code style, MISRA-C, etc.)
 */

/* ========== Include Files ========== */
#include "safetimer.h"
#include <stddef.h> /* For NULL definition */


/* ========== Internal Data Structures ========== */

/**
 * @brief Timer slot structure (15 bytes per timer)
 *
 * Memory layout (8-bit MCU, 2-byte pointers):
 *   period:          4 bytes (uint32_t)
 *   expire_time:     4 bytes (uint32_t / bsp_tick_t)
 *   callback:        2 bytes (function pointer)
 *   user_data:       2 bytes (void pointer)
 *   mode:            1 byte  (uint8_t)
 *   active:          1 byte  (uint8_t)
 *   generation:      1 byte  (uint8_t) - ABA problem prevention
 *   TOTAL:          15 bytes/timer
 */
typedef struct {
  bsp_tick_t period;      /**< Timer period in milliseconds */
  bsp_tick_t expire_time; /**< Expiration timestamp (for overflow algorithm) */
  timer_callback_t callback; /**< User callback function (can be NULL) */
  void *user_data;           /**< User data passed to callback */
  uint8_t mode;              /**< TIMER_MODE_ONE_SHOT or TIMER_MODE_REPEAT */
  uint8_t active;            /**< 1=active, 0=inactive */
  uint8_t generation;        /**< Generation counter (1~7, prevents ABA reuse) */
} timer_slot_t;

/**
 * @brief Timer pool structure (global state)
 *
 * Memory layout:
 *   slots:           MAX_TIMERS * 15 bytes (timer_slot_t array)
 *   used_bitmap:     4 bytes  (uint32_t, supports up to 32 timers)
 *   next_generation: 1 byte   (uint8_t, global generation counter)
 *   TOTAL:           MAX_TIMERS * 15 + 5 bytes
 *
 * For MAX_TIMERS=4: 4*15 + 5 = 65 bytes
 * For MAX_TIMERS=8: 8*15 + 5 = 125 bytes
 */
typedef struct {
  timer_slot_t slots[MAX_TIMERS]; /**< Timer slot array */
  uint32_t used_bitmap; /**< Bitmap of used slots (bit 0 = slot 0, etc.) */
  uint8_t next_generation; /**< Next generation ID (1~7, wraps, 0 reserved) */
} safetimer_pool_t;

/* Compile-time validation: MAX_TIMERS must not exceed bitmap width */
#if MAX_TIMERS > 32
#error "SafeTimer bitmap only supports MAX_TIMERS <= 32"
#endif

/* ========== Global Variables ========== */

/**
 * @brief Global timer pool (65 bytes for MAX_TIMERS=4, 125 bytes for
 * MAX_TIMERS=8)
 *
 * Initialized to zero by C standard (all timers inactive at startup).
 * next_generation starts at 0 and will be incremented to 1 on first use.
 * Protected by BSP critical sections during modifications.
 */
static safetimer_pool_t g_timer_pool = {0};

/* ========== Handle Encoding/Decoding (ABA Prevention) ========== */

/**
 * @brief Handle encoding: [generation:3bit][index:5bit]
 *
 * Generation: 1~7 (0 reserved for INVALID_HANDLE = -1)
 * Index: 0~31 (supports MAX_TIMERS <= 32)
 *
 * Example: generation=3, index=5 → handle = (3 << 5) | 5 = 101 (0x65)
 */
#define HANDLE_INDEX_MASK  0x1F  /* Lower 5 bits: 0b00011111 */
#define HANDLE_GEN_MASK    0xE0  /* Upper 3 bits: 0b11100000 */
#define HANDLE_GEN_SHIFT   5

#define ENCODE_HANDLE(gen, idx) ((safetimer_handle_t)(((gen) << HANDLE_GEN_SHIFT) | (idx)))
#define DECODE_INDEX(handle)    ((int)((handle) & HANDLE_INDEX_MASK))
#define DECODE_GEN(handle)      ((uint8_t)(((handle) & HANDLE_GEN_MASK) >> HANDLE_GEN_SHIFT))

/* ========== Static Function Prototypes ========== */

STATIC int32_t safetimer_tick_diff(bsp_tick_t lhs, bsp_tick_t rhs);
STATIC int validate_handle(safetimer_handle_t handle);
STATIC int find_free_slot(void);
STATIC void update_expire_time(int slot_index, bsp_tick_t current_tick);
STATIC void trigger_timer(int slot_index, bsp_tick_t current_tick,
                          timer_callback_t *callback_out, void **user_data_out);

/* ========== Internal Helper Functions ========== */

/**
 * @brief Calculate signed difference between two tick values (handles
 * wraparound)
 *
 * This function correctly handles tick counter wraparound for both 16-bit and
 * 32-bit tick types. The subtraction MUST be performed in the native tick width
 * before sign extension to preserve wraparound semantics.
 *
 * Why this is critical:
 * - Wrong: (int32_t)(lhs - rhs)  // Subtraction in wrong width!
 * - Right: (int32_t)(signed short)(uint16_t)(lhs - rhs)  // Correct for 16-bit
 *
 * Example (16-bit wraparound):
 *   current_tick = 1, expire_time = 65535
 *   Unsigned subtraction: (1 - 65535) = 2 (in uint16_t)
 *   Sign extension: (signed short)2 = 2
 *   Result: Timer expired 2ms ago ✓
 *
 * @param lhs Left-hand side tick value
 * @param rhs Right-hand side tick value
 * @return Signed difference (lhs - rhs), correctly handling wraparound
 */
STATIC int32_t safetimer_tick_diff(bsp_tick_t lhs, bsp_tick_t rhs) {
#if BSP_TICK_TYPE_16BIT
  /* For 16-bit ticks: subtract in uint16_t domain, then sign-extend via signed
   * short */
  uint16_t diff_u16 = (uint16_t)(lhs - rhs);
  int16_t diff_s16 = (int16_t)diff_u16;
  return (int32_t)diff_s16;
#else
  /* For 32-bit ticks: direct signed cast handles wraparound correctly */
  return (int32_t)(lhs - rhs);
#endif
}

/* ========== Public API Implementation ========== */

/**
 * @brief Create a new timer
 *
 * Implementation details:
 * - Uses bitmap to find free slot (O(n) search)
 * - Critical section protects slot allocation
 * - Timer initialized but NOT started (user must call safetimer_start)
 * - Assigns generation counter to prevent ABA handle reuse (fixes Trap #1)
 */
safetimer_handle_t safetimer_create(uint32_t period_ms, timer_mode_t mode,
                                    timer_callback_t callback,
                                    void *user_data) {
  int slot_index;
  uint8_t generation;
  safetimer_handle_t handle;

#if ENABLE_PARAM_CHECK
  /* Validate parameters */
  if (period_ms == 0 || period_ms > 0x7FFFFFFFUL) {
    return SAFETIMER_INVALID_HANDLE; /* Period must be 1 ~ 2^31-1 */
  }

#if BSP_TICK_TYPE_16BIT
  /* 16-bit mode: max period is 65535ms (fixes Trap #13) */
  if (period_ms > 65535UL) {
    return SAFETIMER_INVALID_HANDLE; /* Period exceeds 16-bit limit */
  }
#endif

  if (mode != TIMER_MODE_ONE_SHOT && mode != TIMER_MODE_REPEAT) {
    return SAFETIMER_INVALID_HANDLE; /* Invalid mode */
  }
#endif

  /* Find free slot */
  bsp_enter_critical();
  slot_index = find_free_slot();

  if (slot_index < 0) {
    bsp_exit_critical();
    return SAFETIMER_INVALID_HANDLE; /* Pool full */
  }

  /* Allocate next generation ID (1~7, wraps, 0 reserved for INVALID_HANDLE) */
  g_timer_pool.next_generation++;
  if (g_timer_pool.next_generation == 0 || g_timer_pool.next_generation > 7) {
    g_timer_pool.next_generation = 1;
  }
  generation = g_timer_pool.next_generation;

  /* Initialize timer slot */
  g_timer_pool.slots[slot_index].period = period_ms;
  g_timer_pool.slots[slot_index].mode = (uint8_t)mode;
  g_timer_pool.slots[slot_index].callback = callback;
  g_timer_pool.slots[slot_index].user_data = user_data;
  g_timer_pool.slots[slot_index].active = 0; /* Not started yet */
  g_timer_pool.slots[slot_index].generation = generation;
  g_timer_pool.used_bitmap |= (1U << slot_index);

  /* Encode handle: [generation:3bit][index:5bit] */
  handle = ENCODE_HANDLE(generation, slot_index);

  bsp_exit_critical();

  return handle;
}

/**
 * @brief Start a timer
 *
 * Implementation details:
 * - Sets expire_time = current_tick + period
 * - Activates timer (sets active=1)
 * - Critical section protects state modification
 */
timer_error_t safetimer_start(safetimer_handle_t handle) {
  int slot_index;

#if ENABLE_PARAM_CHECK
  if (!validate_handle(handle)) {
    return TIMER_ERR_INVALID;
  }
#endif

  bsp_tick_t start_tick;

  /* Decode handle to get slot index */
  slot_index = DECODE_INDEX(handle);

  /* Read BSP tick before entering the SafeTimer critical section to avoid
   * nested interrupt masking inside bsp_get_ticks(). */
  start_tick = bsp_get_ticks();

  bsp_enter_critical();

  /* Update expiration time */
  update_expire_time(slot_index, start_tick);

  /* Mark as active */
  g_timer_pool.slots[slot_index].active = 1;

  bsp_exit_critical();

  return TIMER_OK;
}

/* ========== Optional Query/Diagnostic APIs ========== */
#if ENABLE_QUERY_API

/**
 * @brief Stop a timer
 *
 * Implementation details:
 * - Marks timer as inactive (active=0)
 * - Does NOT delete timer (slot remains allocated)
 * - Critical section protects state modification
 */
timer_error_t safetimer_stop(safetimer_handle_t handle) {
  int slot_index;

#if ENABLE_PARAM_CHECK
  if (!validate_handle(handle)) {
    return TIMER_ERR_INVALID;
  }
#endif

  slot_index = DECODE_INDEX(handle);

  bsp_enter_critical();
  g_timer_pool.slots[slot_index].active = 0;
  bsp_exit_critical();

  return TIMER_OK;
}

#endif /* ENABLE_QUERY_API */

/**
 * @brief Delete a timer
 *
 * Implementation details:
 * - Clears used_bitmap bit (releases slot)
 * - Stops timer if running
 * - Critical section protects bitmap modification
 * - Generation counter prevents deleted handle reuse (ABA protection)
 */
timer_error_t safetimer_delete(safetimer_handle_t handle) {
  int slot_index;

#if ENABLE_PARAM_CHECK
  if (!validate_handle(handle)) {
    return TIMER_ERR_INVALID;
  }
#endif

  slot_index = DECODE_INDEX(handle);

  bsp_enter_critical();

  /* Stop timer */
  g_timer_pool.slots[slot_index].active = 0;

  /* Release slot (generation remains, preventing handle reuse) */
  g_timer_pool.used_bitmap &= ~(1U << slot_index);

  bsp_exit_critical();

  return TIMER_OK;
}

/**
 * @brief Dynamically change timer period
 *
 * Implementation details:
 * - Validates period range (1 ~ 2^31-1 ms)
 * - Validates handle using validate_handle()
 * - Updates period field immediately
 * - If timer is running (active=1), restarts countdown from current tick
 * - If timer is stopped (active=0), new period takes effect on next start()
 * - Critical section protects period update and expire_time recalculation
 *
 * Design trade-off:
 * - Immediate restart breaks REPEAT mode phase-locking (intentional)
 * - Alternative "delayed effect" would require +4B RAM per timer
 * - Users can preserve phase by calling from timer callback
 *
 * @note This function is thread-safe and can be called from any context
 *       except timer callbacks (ADR-003 restriction applies)
 */
timer_error_t safetimer_set_period(safetimer_handle_t handle,
                                   uint32_t new_period_ms) {
  int slot_index;

#if ENABLE_PARAM_CHECK
  /* Validate period range */
  if (new_period_ms == 0 || new_period_ms > 0x7FFFFFFFUL) {
    return TIMER_ERR_INVALID; /* Period must be 1 ~ 2^31-1 */
  }

#if BSP_TICK_TYPE_16BIT
  /* 16-bit mode: max period is 65535ms (fixes Trap #13) */
  if (new_period_ms > 65535UL) {
    return TIMER_ERR_INVALID; /* Period exceeds 16-bit limit */
  }
#endif

  /* Validate handle and check if slot is allocated */
  if (!validate_handle(handle)) {
    return TIMER_ERR_INVALID; /* Invalid handle or timer deleted */
  }
#endif

  bsp_tick_t current_tick;

  slot_index = DECODE_INDEX(handle);

  /* Read BSP tick before entering the SafeTimer critical section to avoid
   * nested interrupt masking inside bsp_get_ticks(). */
  current_tick = bsp_get_ticks();

  bsp_enter_critical();

  /* Update period field */
  g_timer_pool.slots[slot_index].period = new_period_ms;

  /* If timer is currently running, restart countdown with new period.
   * This is equivalent to "delete + create + start" but preserves handle.
   * Breaks phase-locking intentionally - documented trade-off. */
  if (g_timer_pool.slots[slot_index].active) {
    g_timer_pool.slots[slot_index].expire_time = current_tick + new_period_ms;
  }
  /* If timer is stopped, new period takes effect on next safetimer_start() */

  bsp_exit_critical();

  return TIMER_OK;
}

/**
 * @brief Advance timer period (phase-locked, zero cumulative error)
 *
 * Unlike safetimer_set_period() which resets countdown from current tick,
 * this function advances the expire_time by the new period, maintaining
 * phase-locking and eliminating cumulative timing error in coroutines.
 *
 * Implementation details:
 * - Calculates last_expire = current_expire_time - old_period
 * - Sets new_expire_time = last_expire + new_period
 * - If new_expire_time is in the past, advances it to the future
 * - Uses ADR-005 overflow-safe comparison (safetimer_tick_diff)
 * - Thread-safe with BSP critical section protection
 *
 * Use cases:
 * - Coroutine SAFETIMER_CORO_SLEEP() for zero-drift periodic tasks
 * - Any scenario requiring strict phase-locking (LED blink, sensor polling)
 *
 * Difference from safetimer_set_period():
 * - set_period(): expire_time = current_tick + period (resets phase)
 * - advance_period(): expire_time += period (preserves phase)
 *
 * @param handle Valid timer handle
 * @param new_period_ms New period in milliseconds (1 ~ 2^31-1)
 *
 * @return TIMER_OK on success, error code otherwise
 * @retval TIMER_ERR_INVALID Invalid handle or period out of range
 * @retval TIMER_ERR_NOT_FOUND Timer not found or deleted
 *
 * @note If timer is inactive, behaves like set_period() (no previous phase)
 * @note Handles overflow/wraparound automatically (ADR-005)
 *
 * @par Example:
 * @code
 * // Coroutine internal use (via macro)
 * SAFETIMER_CORO_SLEEP(100);  // Uses advance_period() internally
 * @endcode
 */
timer_error_t safetimer_advance_period(safetimer_handle_t handle,
                                       uint32_t new_period_ms) {
  int slot_index;

#if ENABLE_PARAM_CHECK
  /* Validate period range */
  if (new_period_ms == 0 || new_period_ms > 0x7FFFFFFFUL) {
    return TIMER_ERR_INVALID; /* Period must be 1 ~ 2^31-1 */
  }

#if BSP_TICK_TYPE_16BIT
  /* 16-bit mode: max period is 65535ms (fixes Trap #13) */
  if (new_period_ms > 65535UL) {
    return TIMER_ERR_INVALID; /* Period exceeds 16-bit limit */
  }
#endif

  /* Validate handle and check if slot is allocated */
  if (!validate_handle(handle)) {
    return TIMER_ERR_INVALID; /* Invalid handle or timer deleted */
  }
#endif

  bsp_tick_t current_tick;

  slot_index = DECODE_INDEX(handle);

  /* Read BSP tick before entering the SafeTimer critical section */
  current_tick = bsp_get_ticks();

  bsp_enter_critical();

  /* Store old period before updating */
  uint32_t prev_period = g_timer_pool.slots[slot_index].period;

  /* Update period field */
  g_timer_pool.slots[slot_index].period = new_period_ms;

  /* Phase-locked advance: maintain timing relationship to previous expire_time
   */
  if (g_timer_pool.slots[slot_index].active) {
    /* Calculate when the timer SHOULD have expired based on old period.
     * This is overflow-safe because both values are bsp_tick_t (uint32_t). */
    bsp_tick_t last_expire =
        g_timer_pool.slots[slot_index].expire_time - prev_period;

    /* Advance from last scheduled expiration, not current time */
    g_timer_pool.slots[slot_index].expire_time = last_expire + new_period_ms;

    /* If coroutine execution delayed too long and new expire_time is in the
     * past, advance it to the future using REPEAT-style catch-up logic. This
     * prevents burst callbacks while maintaining zero cumulative error. */
    while (safetimer_tick_diff(current_tick,
                               g_timer_pool.slots[slot_index].expire_time) >= 0) {
      g_timer_pool.slots[slot_index].expire_time += new_period_ms;
    }
  } else {
    /* Timer not active: no previous phase to preserve, behave like set_period()
     */
    g_timer_pool.slots[slot_index].expire_time = current_tick + new_period_ms;
  }

  bsp_exit_critical();

  return TIMER_OK;
}

/**
 * @brief Process all active timers
 *
 * Implementation details (ADR-005 CRITICAL):
 * - Uses signed difference comparison: (long)(current - expire) >= 0
 * - Handles 32-bit wraparound automatically (two's complement)
 * - O(n) algorithm: iterates all MAX_TIMERS slots
 * - Triggers callback outside critical section for safety
 * - Repeating timers automatically reset expire_time
 */
void safetimer_process(void) {
  int i;
  bsp_tick_t current_tick;

  current_tick = bsp_get_ticks();

  for (i = 0; i < MAX_TIMERS; i++) {
    timer_callback_t callback = NULL;
    void *user_data = NULL;
    int should_invoke = 0;

    /*
     * Copy slot state under the BSP critical section (prevents races with
     * start/stop/delete) and only call user code after releasing the lock.
     */
    bsp_enter_critical();

    /* Skip inactive timers */
    if (!g_timer_pool.slots[i].active) {
      bsp_exit_critical();
      continue;
    }

    /*
     * ADR-005: Signed Difference Comparison Algorithm (updated for
     * 16-bit/32-bit)
     *
     * Check: safetimer_tick_diff(current_tick, expire_time) >= 0
     *
     * This handles wraparound correctly for both tick widths:
     * - 32-bit: current=1000, expire=500 → diff=500 ≥ 0 ✓
     * - 32-bit wraparound: current=95, expire=4294967295 → diff=96 ≥ 0 ✓
     * - 16-bit wraparound: current=1, expire=65535 → diff=2 ≥ 0 ✓
     *
     * Why safetimer_tick_diff() is required:
     * - For 32-bit ticks: (int32_t)(current - expire) works correctly
     * - For 16-bit ticks: Must subtract in uint16_t then sign-extend to int32_t
     *   Direct cast (int32_t)(current - expire) would be wrong!
     */
    if (safetimer_tick_diff(current_tick, g_timer_pool.slots[i].expire_time) >=
        0) {
      /* Timer expired - trigger it */
      trigger_timer(i, current_tick, &callback, &user_data);
      should_invoke = 1;
    }

    bsp_exit_critical();

    /* Execute callback OUTSIDE critical section */
    if (should_invoke && callback != NULL) {
      /* Double-check active state to prevent TOCTOU race (fixes Trap #6)
       * Scenario: After releasing critical section, an ISR might call stop().
       * Without this check, we'd execute callback on a stopped timer. */
      bsp_enter_critical();
      int still_active = g_timer_pool.slots[i].active;
      bsp_exit_critical();

      if (still_active) {
        callback(user_data);
      }
    }
  }
}

/* ========== Optional Query/Diagnostic APIs ========== */
#if ENABLE_QUERY_API

/**
 * @brief Get timer running status
 */
timer_error_t safetimer_get_status(safetimer_handle_t handle, int *is_running) {
  int slot_index;

#if ENABLE_PARAM_CHECK
  if (!validate_handle(handle)) {
    return TIMER_ERR_INVALID;
  }

  if (is_running == NULL) {
    return TIMER_ERR_INVALID;
  }
#endif

  slot_index = DECODE_INDEX(handle);

  bsp_enter_critical();
  *is_running = (int)g_timer_pool.slots[slot_index].active;
  bsp_exit_critical();

  return TIMER_OK;
}

/**
 * @brief Get remaining time until expiration
 */
timer_error_t safetimer_get_remaining(safetimer_handle_t handle,
                                      uint32_t *remaining_ms) {
  bsp_tick_t current_tick;
  int32_t diff; /* Use int32_t for correct wraparound handling (ADR-005) */
  int slot_index;

#if ENABLE_PARAM_CHECK
  if (!validate_handle(handle)) {
    return TIMER_ERR_INVALID;
  }

  if (remaining_ms == NULL) {
    return TIMER_ERR_INVALID;
  }
#endif

  slot_index = DECODE_INDEX(handle);

  bsp_enter_critical();

  if (!g_timer_pool.slots[slot_index].active) {
    /* Stopped timer */
    *remaining_ms = 0;
    bsp_exit_critical();
    return TIMER_OK;
  }

  current_tick = bsp_get_ticks();
  diff =
      safetimer_tick_diff(g_timer_pool.slots[slot_index].expire_time, current_tick);

  if (diff < 0) {
    /* Already expired but not yet processed */
    *remaining_ms = 0;
  } else {
    *remaining_ms = (uint32_t)diff;
  }

  bsp_exit_critical();

  return TIMER_OK;
}

/**
 * @brief Get timer pool usage statistics
 */
timer_error_t safetimer_get_pool_usage(int *used_count, int *total_count) {
  int i;
  int count;

  count = 0;

  bsp_enter_critical();

  /* Count set bits in bitmap */
  for (i = 0; i < MAX_TIMERS; i++) {
    if (g_timer_pool.used_bitmap & (1U << i)) {
      count++;
    }
  }

  bsp_exit_critical();

  if (used_count != NULL) {
    *used_count = count;
  }

  if (total_count != NULL) {
    *total_count = MAX_TIMERS;
  }

  return TIMER_OK;
}

#endif /* ENABLE_QUERY_API */

/* ========== Static Function Implementation ========== */

#ifdef UNIT_TEST
/**
 * @brief Reset timer pool (FOR TESTING ONLY)
 *
 * @warning This function is ONLY for unit testing
 * @warning DO NOT use in production code
 */
void safetimer_test_reset_pool(void) {
  int i;
  for (i = 0; i < MAX_TIMERS; i++) {
    g_timer_pool.slots[i].period = 0;
    g_timer_pool.slots[i].expire_time = 0;
    g_timer_pool.slots[i].callback = NULL;
    g_timer_pool.slots[i].user_data = NULL;
    g_timer_pool.slots[i].mode = 0;
    g_timer_pool.slots[i].active = 0;
  }
  g_timer_pool.used_bitmap = 0;
}
#endif

/**
 * @brief Validate timer handle (with generation check for ABA prevention)
 *
 * @param handle Timer handle to validate
 *
 * @return 1 if valid and allocated, 0 otherwise
 *
 * @note Internal helper - assumes ENABLE_PARAM_CHECK=1
 * @note Validates both slot index and generation counter (fixes Trap #1)
 */
STATIC int validate_handle(safetimer_handle_t handle) {
  int slot_index;
  uint8_t handle_gen;

  /* Decode handle */
  slot_index = DECODE_INDEX(handle);
  handle_gen = DECODE_GEN(handle);

  /* Range check */
  if (slot_index < 0 || slot_index >= MAX_TIMERS) {
    return 0;
  }

  /* Allocation check */
  if ((g_timer_pool.used_bitmap & (1U << slot_index)) == 0) {
    return 0;
  }

  /* Generation check (ABA prevention) */
  if (g_timer_pool.slots[slot_index].generation != handle_gen) {
    return 0; /* Handle is from a previous allocation cycle */
  }

  return 1;
}

/**
 * @brief Find first free slot in timer pool
 *
 * @return Slot index (0 ~ MAX_TIMERS-1) if found, -1 if pool full
 *
 * @note Uses bitmap for O(n) search
 * @note Called inside critical section - keep fast!
 */
STATIC int find_free_slot(void) {
  int i;

  for (i = 0; i < MAX_TIMERS; i++) {
    if ((g_timer_pool.used_bitmap & (1U << i)) == 0) {
      return i; /* Found free slot */
    }
  }

  return -1; /* Pool full */
}

/**
 * @brief Update expiration time for a timer slot
 *
 * @param slot_index Valid slot index (0 ~ MAX_TIMERS-1)
 *
 * @note Calculates: expire_time = current_tick + period
 * @note Handles 32-bit wraparound automatically
 * @note Called inside critical section
 */
STATIC void update_expire_time(int slot_index, bsp_tick_t current_tick) {
  /*
   * Use the tick captured outside the SafeTimer critical section so BSP
   * implementations remain free to mask interrupts when returning ticks.
   *
   * Set expiration time (32-bit wraparound handled by ADR-005 algorithm)
   * Example: current=4294967290, period=100
   *   → expire = 4294967390 (wraps to 94)
   *   → safetimer_process() will correctly detect expiration
   */
  g_timer_pool.slots[slot_index].expire_time =
      current_tick + g_timer_pool.slots[slot_index].period;
}

/**
 * @brief Trigger timer callback and handle mode
 *
 * @param slot_index Valid slot index (0 ~ MAX_TIMERS-1)
 *
 * @note For ONE_SHOT: stops timer after callback
 * @note For REPEAT: advances expire_time by period (phase-locked, eliminates
 * drift)
 * @note Calls user callback if not NULL
 * @note Called outside critical section to allow callback to run safely
 */
STATIC void trigger_timer(int slot_index, bsp_tick_t current_tick,
                          timer_callback_t *callback_out,
                          void **user_data_out) {
  /* Caller already holds the BSP critical section. */

  if (callback_out != NULL) {
    *callback_out = g_timer_pool.slots[slot_index].callback;
  }

  if (user_data_out != NULL) {
    *user_data_out = g_timer_pool.slots[slot_index].user_data;
  }

  if (g_timer_pool.slots[slot_index].mode == TIMER_MODE_ONE_SHOT) {
    /* ONE_SHOT: stop timer */
    g_timer_pool.slots[slot_index].active = 0;
  } else {
    /* REPEAT: advance until the next expiration is in the future */
#if SAFETIMER_ENABLE_CATCHUP
    /* Catch-up mode: fire callbacks for each missed interval */
    g_timer_pool.slots[slot_index].expire_time +=
        g_timer_pool.slots[slot_index].period;
#else
    /* Skip mode (default): coalesce missed intervals using math instead of loop
     * to prevent watchdog timeout in critical section (fixes Trap #2) */
    bsp_tick_t old_expire = g_timer_pool.slots[slot_index].expire_time;
    uint32_t period = g_timer_pool.slots[slot_index].period;

    /* Calculate how many periods we're behind */
    int32_t lag = safetimer_tick_diff(current_tick, old_expire);
    if (lag >= 0) {
      /* We're behind schedule - advance by N periods to catch up */
      uint32_t missed_periods = ((uint32_t)lag / period) + 1;
      g_timer_pool.slots[slot_index].expire_time = old_expire + (missed_periods * period);
    } else {
      /* Should not happen (timer not expired yet), but handle gracefully */
      g_timer_pool.slots[slot_index].expire_time += period;
    }
#endif
  }
}
