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

/* ========== Handle Encoding/Decoding (ABA Prevention) ========== */

/**
 * @brief Dynamic handle encoding based on MAX_TIMERS
 *
 * Optimizes bit allocation: uses minimum bits for index, maximizes generation
 * bits.
 *
 * Examples:
 * - MAX_TIMERS=8:  [gen:5bit (1~31)][idx:3bit (0~7)]  ← 4x better ABA
 * protection
 * - MAX_TIMERS=16: [gen:4bit (1~15)][idx:4bit (0~15)] ← 2x better ABA
 * protection
 * - MAX_TIMERS=32: [gen:3bit (1~7)][idx:5bit (0~31)]  ← Original behavior
 *
 * Generation: 1 ~ HANDLE_GEN_MAX (0 reserved for INVALID_HANDLE = -1)
 */

/* Compile-time calculation of required index bits */
#if MAX_TIMERS <= 2
#define HANDLE_INDEX_BITS 1
#elif MAX_TIMERS <= 4
#define HANDLE_INDEX_BITS 2
#elif MAX_TIMERS <= 8
#define HANDLE_INDEX_BITS 3
#elif MAX_TIMERS <= 16
#define HANDLE_INDEX_BITS 4
#else
#define HANDLE_INDEX_BITS 5
#endif

/* Derive generation bits and masks */
/* CRITICAL: Must cap GEN_BITS at 6 to fit in uint8_t meta with mode+active */
#define RAW_GEN_BITS (8 - HANDLE_INDEX_BITS)
#define HANDLE_GEN_BITS (RAW_GEN_BITS > 6 ? 6 : RAW_GEN_BITS)
#define HANDLE_GEN_MAX ((1 << HANDLE_GEN_BITS) - 1)

#define HANDLE_INDEX_MASK ((1 << HANDLE_INDEX_BITS) - 1)
#define HANDLE_GEN_MASK (((1 << HANDLE_GEN_BITS) - 1) << HANDLE_INDEX_BITS)
#define HANDLE_GEN_SHIFT HANDLE_INDEX_BITS

#define ENCODE_HANDLE(gen, idx)                                                \
  ((safetimer_handle_t)(((gen) << HANDLE_GEN_SHIFT) | (idx)))
#define DECODE_INDEX(handle) ((uint8_t)((handle) & HANDLE_INDEX_MASK))
#define DECODE_GEN(handle)                                                     \
  ((uint8_t)(((handle) & HANDLE_GEN_MASK) >> HANDLE_GEN_SHIFT))

/* Compression Macros */
#if USE_BITFIELD_META
/* C Bitfields: Cleaner syntax, but compiler-dependent order */
typedef struct {
  uint8_t active : 1;
  uint8_t mode : 1;
  uint8_t generation : 6;
} timer_meta_t;

#define META_INIT(act, mod, gen) {(act), (mod), (gen)}
#define SLOT_SET_ACTIVE(slot, val) (slot).meta.active = (val)
#define SLOT_GET_ACTIVE(slot) ((slot).meta.active)
#define SLOT_SET_MODE(slot, val) (slot).meta.mode = (val)
#define SLOT_GET_MODE(slot) ((slot).meta.mode)
#define SLOT_SET_GEN(slot, val) (slot).meta.generation = (val)
#define SLOT_GET_GEN(slot) ((slot).meta.generation)

#else
/* Manual Masking: Portable, explicit control */
typedef uint8_t timer_meta_t;

/* Layout: [gen:6][mode:1][active:1] */
#define META_MASK_ACTIVE 0x01U
#define META_MASK_MODE 0x02U
#define META_MASK_GEN 0xFCU
#define META_SHIFT_MODE 1
#define META_SHIFT_GEN 2

#define META_INIT(act, mod, gen)                                               \
  ((uint8_t)(((act) & 1) | (((mod) & 1) << META_SHIFT_MODE) |                  \
             (((gen) & 0x3F) << META_SHIFT_GEN)))

#define SLOT_SET_ACTIVE(slot, val)                                             \
  do {                                                                         \
    if (val)                                                                   \
      (slot).meta |= META_MASK_ACTIVE;                                         \
    else                                                                       \
      (slot).meta &= (uint8_t)~META_MASK_ACTIVE;                               \
  } while (0)
#define SLOT_GET_ACTIVE(slot) (((slot).meta & META_MASK_ACTIVE) != 0)

#define SLOT_SET_MODE(slot, val)                                               \
  do {                                                                         \
    if (val)                                                                   \
      (slot).meta |= META_MASK_MODE;                                           \
    else                                                                       \
      (slot).meta &= (uint8_t)~META_MASK_MODE;                                 \
  } while (0)
#define SLOT_GET_MODE(slot)                                                    \
  ((((slot).meta & META_MASK_MODE) >> META_SHIFT_MODE))

#define SLOT_SET_GEN(slot, val)                                                \
  do {                                                                         \
    (slot).meta =                                                              \
        ((slot).meta & ~META_MASK_GEN) | (((val) & 0x3F) << META_SHIFT_GEN);   \
  } while (0)
#define SLOT_GET_GEN(slot) (((slot).meta & META_MASK_GEN) >> META_SHIFT_GEN)

#endif

/**
 * @brief Timer slot structure (13 bytes per timer with meta compression)
 *
 * Memory layout (8-bit MCU, 2-byte pointers):
 *   period:          4 bytes (uint32_t)
 *   expire_time:     4 bytes (uint32_t / bsp_tick_t)
 *   callback:        2 bytes (function pointer)
 *   user_data:       2 bytes (void pointer)
 *   meta:            1 byte  (active:1 + mode:1 + generation:6)
 *   TOTAL:          13 bytes/timer (11 bytes w/ 16-bit ticks)
 */
typedef struct {
  bsp_tick_t period;      /**< Timer period in milliseconds */
  bsp_tick_t expire_time; /**< Expiration timestamp (for overflow algorithm) */
  timer_callback_t callback; /**< User callback function (can be NULL) */
#if SAFETIMER_ENABLE_USER_DATA
  void *user_data; /**< User data passed to callback */
#endif
  timer_meta_t meta; /**< Compressed state: active(1)+mode(1)+gen(6) */
} timer_slot_t;

/**
 * @brief Bitmap type selection based on MAX_TIMERS
 *
 * Automatically selects the smallest type that can hold MAX_TIMERS bits:
 * - MAX_TIMERS <= 8:  uint8_t  (saves 3 bytes RAM)
 * - MAX_TIMERS <= 32: uint32_t (supports more timers)
 */
#if MAX_TIMERS <= 8
typedef uint8_t safetimer_bitmap_t;
#define BITMAP_ONE 1U
#else
typedef uint32_t safetimer_bitmap_t;
#define BITMAP_ONE 1UL
#endif

/**
 * @brief Timer pool structure (global state)
 *
 * Memory layout:
 *   slots:           MAX_TIMERS * 13 bytes (timer_slot_t array)
 *   used_bitmap:     1 or 4 bytes (depends on MAX_TIMERS)
 *   next_generation: 1 byte (uint8_t, global generation counter)
 *
 * For MAX_TIMERS=4:  4*13 + 1 + 1 = 54 bytes (was 62)
 * For MAX_TIMERS=8:  8*13 + 1 + 1 = 106 bytes (was 122)
 * For MAX_TIMERS=16: 16*13 + 4 + 1 = 213 bytes (was 245)
 */
typedef struct {
  timer_slot_t slots[MAX_TIMERS]; /**< Timer slot array */
  safetimer_bitmap_t used_bitmap; /**< Bitmap of used slots */
  uint8_t next_generation; /**< Next generation ID (1~7, wraps, 0 reserved) */
} safetimer_pool_t;

/* Compile-time validation: MAX_TIMERS must not exceed bitmap width */
#if MAX_TIMERS > 32
#error "SafeTimer bitmap only supports MAX_TIMERS <= 32"
#endif

/* ========== Global Variables ========== */

/**
 * @brief Global timer pool
 *
 * Initialized to zero by C standard (all timers inactive at startup).
 * next_generation starts at 0 and will be incremented to 1 on first use.
 * Protected by BSP critical sections during modifications.
 */
static safetimer_pool_t g_timer_pool = {0};

/**
 * @brief Recursion guard flag (prevents Trap #19: Recursive Stack Overflow)
 *
 * Set to 1 when safetimer_process() is executing.
 * If callback erroneously calls safetimer_process() again, the guard
 * prevents recursion and returns immediately.
 *
 * RAM cost: +1 byte
 */
static volatile uint8_t s_processing = 0;

/**
 * @brief Currently executing timer handle (for auto-binding coroutines)
 *
 * Set to the active handle during callback execution in safetimer_process().
 * Allows coroutines to discover their own handle via
 * safetimer_get_current_handle().
 *
 * RAM cost: +1 byte
 */
static safetimer_handle_t g_executing_handle = SAFETIMER_INVALID_HANDLE;

/* ========== Handle Encoding/Decoding (ABA Prevention) ========== */
/* Moved to top of file to support struct definition */

/* ========== Static Function Prototypes ========== */

STATIC int32_t safetimer_tick_diff(bsp_tick_t lhs, bsp_tick_t rhs);
STATIC int validate_handle(safetimer_handle_t handle);
STATIC int find_free_slot(void);
STATIC void update_expire_time(uint8_t slot_index, bsp_tick_t current_tick);
STATIC void trigger_timer(uint8_t slot_index, bsp_tick_t current_tick,
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
#if SAFETIMER_ENABLE_USER_DATA
safetimer_handle_t safetimer_create(uint32_t period_ms, timer_mode_t mode,
                                    timer_callback_t callback,
                                    void *user_data) {
#else
safetimer_handle_t safetimer_create(uint32_t period_ms, timer_mode_t mode,
                                    timer_callback_t callback) {
#endif
  safetimer_handle_t handle;
  uint8_t slot_index;
  int free_slot;
  uint8_t generation;

#if SAFETIMER_REPEAT_ONLY
  /* Force mode to REPEAT if compiled in Repeat-Only mode */
  mode = TIMER_MODE_REPEAT;
#endif

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
  free_slot = find_free_slot();

  if (free_slot < 0) {
    bsp_exit_critical();
    return SAFETIMER_INVALID_HANDLE; /* Pool full */
  }

  slot_index = (uint8_t)free_slot;

  /* Allocate next generation ID (1~HANDLE_GEN_MAX, wraps, 0 reserved) */
  g_timer_pool.next_generation++;
  if (g_timer_pool.next_generation == 0 ||
      g_timer_pool.next_generation > HANDLE_GEN_MAX) {
    g_timer_pool.next_generation = 1;
  }
  generation = g_timer_pool.next_generation;

  /* Initialize timer slot (cast period_ms to bsp_tick_t for 16-bit mode) */
  g_timer_pool.slots[slot_index].period = (bsp_tick_t)period_ms;
  SLOT_SET_MODE(g_timer_pool.slots[slot_index], (uint8_t)mode);
  g_timer_pool.slots[slot_index].callback = callback;
#if SAFETIMER_ENABLE_USER_DATA
  g_timer_pool.slots[slot_index].user_data = user_data;
#endif
  SLOT_SET_ACTIVE(g_timer_pool.slots[slot_index], 0); /* Not started yet */
  SLOT_SET_GEN(g_timer_pool.slots[slot_index], generation);
  g_timer_pool.used_bitmap |= (BITMAP_ONE << slot_index);

  /* Encode handle: [generation:3bit][index:5bit] */
  handle = ENCODE_HANDLE(generation, slot_index);

  /* Prevent handle collision with SAFETIMER_INVALID_HANDLE (-1)
   * Edge case: When handle type is int8_t or when generation/index combination
   * produces -1, we must skip to next generation to ensure valid handle.
   * Loop guarantees we find a valid handle (max iterations = HANDLE_GEN_MAX).
   */
  while (handle == SAFETIMER_INVALID_HANDLE) {
    g_timer_pool.next_generation++;
    if (g_timer_pool.next_generation == 0 ||
        g_timer_pool.next_generation > HANDLE_GEN_MAX) {
      g_timer_pool.next_generation = 1;
    }
    generation = g_timer_pool.next_generation;
    SLOT_SET_GEN(g_timer_pool.slots[slot_index], generation);
    handle = ENCODE_HANDLE(generation, slot_index);
  }

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
  uint8_t slot_index;
  bsp_tick_t start_tick; /* C89: declare before statements */

#if ENABLE_PARAM_CHECK
  if (!validate_handle(handle)) {
    return TIMER_ERR_INVALID;
  }
#endif

  /* Decode handle to get slot index */
  slot_index = DECODE_INDEX(handle);

  /* Read BSP tick before entering the SafeTimer critical section to avoid
   * nested interrupt masking inside bsp_get_ticks(). */
  start_tick = bsp_get_ticks();

  bsp_enter_critical();

  /* Update expiration time */
  update_expire_time(slot_index, start_tick);

  /* Mark as active */
  SLOT_SET_ACTIVE(g_timer_pool.slots[slot_index], 1);

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
  uint8_t slot_index;

#if ENABLE_PARAM_CHECK
  if (!validate_handle(handle)) {
    return TIMER_ERR_INVALID;
  }
#endif

  slot_index = DECODE_INDEX(handle);

  bsp_enter_critical();
  SLOT_SET_ACTIVE(g_timer_pool.slots[slot_index], 0);
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
  uint8_t slot_index;

#if ENABLE_PARAM_CHECK
  if (!validate_handle(handle)) {
    return TIMER_ERR_INVALID;
  }
#endif

  slot_index = DECODE_INDEX(handle);

  bsp_enter_critical();

  /* Stop timer */
  SLOT_SET_ACTIVE(g_timer_pool.slots[slot_index], 0);

  /* Release slot (generation remains, preventing handle reuse) */
  g_timer_pool.used_bitmap &= ~(BITMAP_ONE << slot_index);

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
  uint8_t slot_index;
  bsp_tick_t current_tick; /* C89: declare before statements */

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

  slot_index = DECODE_INDEX(handle);

  /* Read BSP tick before entering the SafeTimer critical section to avoid
   * nested interrupt masking inside bsp_get_ticks(). */
  current_tick = bsp_get_ticks();

  bsp_enter_critical();

  /* Update period field (explicit cast for C89 warning suppression) */
  g_timer_pool.slots[slot_index].period = (bsp_tick_t)new_period_ms;

  /* If timer is currently running, restart countdown with new period.
   * This is equivalent to "delete + create + start" but preserves handle.
   * Breaks phase-locking intentionally - documented trade-off. */
  if (SLOT_GET_ACTIVE(g_timer_pool.slots[slot_index])) {
    g_timer_pool.slots[slot_index].expire_time =
        current_tick + (bsp_tick_t)new_period_ms;
  }
  /* If timer is stopped, new period takes effect on next safetimer_start() */

  bsp_exit_critical();

  return TIMER_OK;
}

#if SAFETIMER_ENABLE_CORO
/**
 * @brief Advance timer expiration by one period (for coroutines)
 *
 * Used by coroutines to implement precise delays (SAFETIMER_CORO_WAIT/YIELD).
 * Moves the expiration time forward without accumulating drift.
 *
 * @param handle Timer handle
 * @return TIMER_OK on success, or error code
 */
timer_error_t safetimer_advance_period(safetimer_handle_t handle,
                                       uint32_t new_period_ms) {
  uint8_t slot_index;
  bsp_tick_t current_tick;        /* C89: declare before statements */
  bsp_tick_t prev_period;         /* C89: declare before statements */
  bsp_tick_t old_expire_snapshot; /* C89: declare before statements */
  uint8_t old_active_snapshot;    /* C89: declare before statements */
  bsp_tick_t last_expire;         /* C89: declare before statements */
  bsp_tick_t new_expire;          /* C89: declare before statements */
  int32_t lag;                    /* C89: declare before statements */
  uint32_t missed_periods;        /* C89: declare before statements */

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
#else
  /* Minimal validation */
  if (!validate_handle(handle)) {
    return TIMER_ERR_INVALID;
  }
#endif

  slot_index = DECODE_INDEX(handle);

  /* Read BSP tick before entering the SafeTimer critical section */
  current_tick = bsp_get_ticks();

  bsp_enter_critical();

  /* Store old period, expire_time, and active state snapshot before updating */
  prev_period = g_timer_pool.slots[slot_index].period;
  old_expire_snapshot = g_timer_pool.slots[slot_index].expire_time;
  old_active_snapshot = SLOT_GET_ACTIVE(g_timer_pool.slots[slot_index]);

  /* Update period field (explicit cast for C89 warning suppression) */
  g_timer_pool.slots[slot_index].period = (bsp_tick_t)new_period_ms;

  /* Phase-locked advance: maintain timing relationship to previous expire_time
   */
  if (SLOT_GET_ACTIVE(g_timer_pool.slots[slot_index])) {
    /* Calculate when the timer SHOULD have expired based on old period.
     * This is overflow-safe because both values are bsp_tick_t. */
    last_expire = old_expire_snapshot - prev_period;

    /* Advance from last scheduled expiration, not current time */
    new_expire = last_expire + (bsp_tick_t)new_period_ms;

    /* If coroutine execution delayed too long and new expire_time is in the
     * past, advance it to the future using REPEAT-style catch-up logic. This
     * prevents burst callbacks while maintaining zero cumulative error.
     *
     * CRITICAL: Division moved outside critical section to reduce interrupt
     * latency (fixes Advance Loop + Heavy Division risks). */
    lag = safetimer_tick_diff(current_tick, new_expire);
    if (lag >= 0) {
      /* Release critical section before expensive division */
      bsp_exit_critical();

#if ENABLE_DEBUG_ASSERT
      /* Defensive: period should never be 0 (validated in API entry) */
      if (new_period_ms == 0) {
        bsp_enter_critical(); /* Restore critical section state */
        bsp_exit_critical();
        return TIMER_ERR_INVALID; /* Fail safe */
      }
#endif

      /* Behind schedule - advance by N periods to catch up (OUTSIDE critical
       * section) */
      missed_periods = ((uint32_t)lag / new_period_ms) + 1U;
      new_expire = new_expire + (bsp_tick_t)(missed_periods * new_period_ms);

      /* Re-enter critical section to update expire_time */
      bsp_enter_critical();

      /* Verify expire_time AND active state weren't modified by ISR during
       * calculation (fixes Stop-Start Overwrite Race + ABA variant). Double
       * verification prevents extreme coincidence where ISR results in same
       * expire_time value. */
      if (g_timer_pool.slots[slot_index].expire_time == old_expire_snapshot &&
          SLOT_GET_ACTIVE(g_timer_pool.slots[slot_index]) ==
              old_active_snapshot) {
        /* ISR didn't interfere, safe to update with catch-up value */
        g_timer_pool.slots[slot_index].expire_time = new_expire;
      }
      /* else: ISR modified timer state, keep ISR's value */
    }
#if !SAFETIMER_REPEAT_ONLY
    else { /* No catch-up needed, update directly */
      g_timer_pool.slots[slot_index].expire_time = new_expire;
    }
#endif
  } else {
    /* Timer not active: no previous phase to preserve, behave like set_period()
     */
    g_timer_pool.slots[slot_index].expire_time =
        current_tick + (bsp_tick_t)new_period_ms;
  }

  bsp_exit_critical();

  return TIMER_OK;
}
#endif /* SAFETIMER_ENABLE_CORO */

#if SAFETIMER_ENABLE_CORO
/**
 * @brief Get currently executing timer handle
 *
 * Returns the handle of the timer whose callback is currently executing.
 * Used by coroutine macros for automatic handle binding.
 *
 * @return Valid handle if called from within a timer callback
 * @retval SAFETIMER_INVALID_HANDLE if called outside callback context
 */
safetimer_handle_t safetimer_get_current_handle(void) {
  return g_executing_handle;
}
#endif /* SAFETIMER_ENABLE_CORO */

/**
 * @brief Process all active timers (call periodically from main loop)
 *
 * O(n) algorithm with recursion guard to prevent stack overflow.
 */
void safetimer_process(void) {
  uint8_t i;
  bsp_tick_t current_tick;

  /* Recursion guard: prevent callback from calling safetimer_process() again
   * (fixes Trap #19: Recursive Stack Overflow). On 8-bit MCUs with ~176B RAM,
   * even 2-3 recursions can exhaust stack and cause system reset. */
  if (s_processing) {
    return; /* Already processing, silently return to prevent recursion */
  }

  s_processing = 1; /* Mark as processing */

  current_tick = bsp_get_ticks();

  for (i = 0; i < MAX_TIMERS; i++) {
    timer_callback_t callback; /* C89: declare before statements */
#if SAFETIMER_ENABLE_USER_DATA
    void *user_data; /* C89: declare before statements */
#endif
    int should_invoke;    /* C89: declare before statements */
    uint8_t captured_gen; /* C89: declare before statements */
#if !SAFETIMER_REPEAT_ONLY
    uint8_t captured_mode; /* C89: declare before statements */
#endif
    int valid; /* C89: declare before statements */

    callback = NULL;
#if SAFETIMER_ENABLE_USER_DATA
    user_data = NULL;
#endif
    should_invoke = 0;
    captured_gen = 0;
#if !SAFETIMER_REPEAT_ONLY
    captured_mode = 0;
#endif

    /*
     * Copy slot state under the BSP critical section (prevents races with
     * start/stop/delete) and only call user code after releasing the lock.
     */
    bsp_enter_critical();

    /* Skip inactive timers */
    if (!SLOT_GET_ACTIVE(g_timer_pool.slots[i])) {
      bsp_exit_critical();
      continue;
    }

    /*
     * ADR-005: Signed Difference Comparison Algorithm (updated for
     * 16-bit/32-bit)
     */
    if (safetimer_tick_diff(current_tick, g_timer_pool.slots[i].expire_time) >=
        0) {
      /* Timer expired - capture state and trigger it */
      captured_gen = SLOT_GET_GEN(g_timer_pool.slots[i]);
#if !SAFETIMER_REPEAT_ONLY
      captured_mode = SLOT_GET_MODE(g_timer_pool.slots[i]);
#endif
      trigger_timer(i, current_tick, &callback, &user_data);
      should_invoke = 1;
    }

    bsp_exit_critical();

    /* Execute callback OUTSIDE critical section */
    if (should_invoke && callback != NULL) {
      /* Double-check validity to prevent TOCTOU race (fixes Trap #6)
       * Use generation counter to detect if timer was deleted/reused.
       * For REPEAT timers: must still be active (user didn't stop it).
       * For ONE_SHOT timers: trigger_timer() set active=0, so only check gen.
       */
      bsp_enter_critical();

      valid = (SLOT_GET_GEN(g_timer_pool.slots[i]) == captured_gen);

      if (valid) {
#if !SAFETIMER_REPEAT_ONLY
        if (captured_mode == TIMER_MODE_REPEAT) {
          if (!SLOT_GET_ACTIVE(g_timer_pool.slots[i])) {
            valid = 0; /* User stopped it */
          }
        }
        /* Else: ONE_SHOT, active is 0 (by us) or 1 (user restart), both valid
         */
#else
        /* REPEAT ONLY: Must be active */
        if (!SLOT_GET_ACTIVE(g_timer_pool.slots[i])) {
          valid = 0;
        }
#endif
      }
      bsp_exit_critical();

      if (valid) {
#if SAFETIMER_ENABLE_CORO
        /* Set executing handle for coroutine auto-binding */
        g_executing_handle = ENCODE_HANDLE(captured_gen, i);
#endif
#if SAFETIMER_ENABLE_USER_DATA
        callback(user_data);
#else
        callback();
#endif
#if SAFETIMER_ENABLE_CORO
        g_executing_handle = SAFETIMER_INVALID_HANDLE;
#endif
      }
    }
  }

  s_processing = 0; /* Clear processing flag */
}

/* ========== Optional Query/Diagnostic APIs ========== */
#if ENABLE_QUERY_API

/**
 * @brief Get timer running status
 */
timer_error_t safetimer_get_status(safetimer_handle_t handle, int *is_running) {
  uint8_t slot_index;

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
  *is_running = (int)SLOT_GET_ACTIVE(g_timer_pool.slots[slot_index]);
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
  uint8_t slot_index;

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

  if (!SLOT_GET_ACTIVE(g_timer_pool.slots[slot_index])) {
    /* Stopped timer */
    *remaining_ms = 0;
    bsp_exit_critical();
    return TIMER_OK;
  }

  current_tick = bsp_get_ticks();
  diff = safetimer_tick_diff(g_timer_pool.slots[slot_index].expire_time,
                             current_tick);

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
  uint8_t i;
  int count;

  count = 0;

  bsp_enter_critical();

  /* Count set bits in bitmap */
  for (i = 0; i < MAX_TIMERS; i++) {
    if (g_timer_pool.used_bitmap & (BITMAP_ONE << i)) {
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
  uint8_t i;
  for (i = 0; i < MAX_TIMERS; i++) {
    g_timer_pool.slots[i].period = 0;
    g_timer_pool.slots[i].expire_time = 0;
    g_timer_pool.slots[i].callback = NULL;
#if SAFETIMER_ENABLE_USER_DATA
    g_timer_pool.slots[i].user_data = NULL;
#endif
    /* Use explicit memset or set meta to 0? Just set individual flag macros or
     * simpler: */
#if USE_BITFIELD_META
    g_timer_pool.slots[i].meta.mode = 0;
    g_timer_pool.slots[i].meta.active = 0;
    g_timer_pool.slots[i].meta.generation = 0;
#else
    g_timer_pool.slots[i].meta = 0;
#endif
  }
  g_timer_pool.used_bitmap = 0;
  g_timer_pool.next_generation = 1;
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
  uint8_t slot_index;
  uint8_t handle_gen;

  /* Decode handle */
  slot_index = DECODE_INDEX(handle);
  handle_gen = DECODE_GEN(handle);

  /* Range check (slot_index is uint8_t, so only check upper bound) */
  if (slot_index >= MAX_TIMERS) {
    return 0;
  }

  /* Allocation check */
  if ((g_timer_pool.used_bitmap & (BITMAP_ONE << slot_index)) == 0) {
    return 0;
  }

  /* Check generation */
  if (handle_gen != SLOT_GET_GEN(g_timer_pool.slots[slot_index])) {
    return 0; /* Generation mismatch (timer deleted/reused) */
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
  uint8_t i;

  for (i = 0; i < MAX_TIMERS; i++) {
    if ((g_timer_pool.used_bitmap & (BITMAP_ONE << i)) == 0) {
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
STATIC void update_expire_time(uint8_t slot_index, bsp_tick_t current_tick) {
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
STATIC void trigger_timer(uint8_t slot_index, bsp_tick_t current_tick,
                          timer_callback_t *callback_out,
                          void **user_data_out) {
#if !SAFETIMER_ENABLE_CATCHUP
  /* C89: declare all variables at block start */
  bsp_tick_t old_expire;
  bsp_tick_t period;
  uint8_t old_active;
  int32_t lag;
  bsp_tick_t new_expire;
  uint32_t missed_periods;
#endif

  /* Caller already holds the BSP critical section. */

  if (callback_out != NULL) {
    *callback_out = g_timer_pool.slots[slot_index].callback;
  }

#if SAFETIMER_ENABLE_USER_DATA
  if (user_data_out != NULL) {
    *user_data_out = g_timer_pool.slots[slot_index].user_data;
  }
#endif

#if SAFETIMER_REPEAT_ONLY
  /* FORCE REPEAT: advance until next expiration */
  /* Remove ONE_SHOT check to save ROM */
  {
#else
  if (SLOT_GET_MODE(g_timer_pool.slots[slot_index]) == TIMER_MODE_ONE_SHOT) {
    /* ONE_SHOT: stop timer */
    SLOT_SET_ACTIVE(g_timer_pool.slots[slot_index], 0);
  } else {
#endif
    /* REPEAT: advance until the next expiration is in the future */
    /* REPEAT: advance until the next expiration is in the future */
#if SAFETIMER_ENABLE_CATCHUP
    /* Catch-up mode: fire callbacks for each missed interval */
    g_timer_pool.slots[slot_index].expire_time +=
        g_timer_pool.slots[slot_index].period;
#else
    /* Skip mode (default): coalesce missed intervals using math instead of loop
     * to prevent watchdog timeout in critical section (fixes Trap #2)
     *
     * CRITICAL: Division moved outside critical section to reduce interrupt
     * latency on 8-bit MCUs without hardware divider (fixes Heavy Division
     * risk). 32-bit division can take 50-150μs, unacceptable in critical
     * section. */

    /* Snapshot values inside critical section */
    old_expire = g_timer_pool.slots[slot_index].expire_time;
    period = g_timer_pool.slots[slot_index].period;
    old_active = SLOT_GET_ACTIVE(g_timer_pool.slots[slot_index]);

    /* Release critical section before expensive division */
    bsp_exit_critical();

    /* Calculate how many periods we're behind (OUTSIDE critical section) */
    lag = safetimer_tick_diff(current_tick, old_expire);

    if (lag >= 0) {
      /* We're behind schedule - advance by N periods to catch up */
#if ENABLE_DEBUG_ASSERT
      /* Defensive: period should never be 0 (validated in create/set_period) */
      if (period == 0) {
        bsp_enter_critical(); /* Restore critical section state */
        return;               /* Avoid divide-by-zero, fail safe */
      }
#endif
      missed_periods = ((uint32_t)lag / (uint32_t)period) + 1U;
      new_expire = old_expire + (bsp_tick_t)(missed_periods * (uint32_t)period);
    } else {
      /* Should not happen (timer not expired yet), but handle gracefully */
      new_expire = old_expire + period;
    }

    /* Re-enter critical section to update expire_time */
    bsp_enter_critical();

    /* Verify expire_time AND active state weren't modified by ISR during
     * calculation (fixes Stop-Start Overwrite Race + ABA variant).
     *
     * Double verification prevents extreme coincidence where ISR stop+start
     * results in     * expire_time value (ABA variant). Checking both
     * expire_time and active ensures we detect any ISR interference. */
    if (g_timer_pool.slots[slot_index].expire_time == old_expire &&
        SLOT_GET_ACTIVE(g_timer_pool.slots[slot_index]) == old_active) {
      g_timer_pool.slots[slot_index].expire_time = new_expire;
    }
    /* else: ISR modified timer state, keep ISR's value */
#endif
  }
}
