/**
 * @file    safetimer_internal.h
 * @brief   SafeTimer Internal Data Structures (Private, DO NOT include in user code)
 * @version 1.2.1
 * @date    2025-12-16
 *
 * @warning This file is for SafeTimer implementation only
 * @warning User code should NEVER include this file
 */

#ifndef SAFETIMER_INTERNAL_H
#define SAFETIMER_INTERNAL_H

#include "safetimer.h"

/* ========== Internal Data Structures ========== */

/**
 * @brief Timer slot structure (14 bytes per timer)
 *
 * Memory layout (8-bit MCU, 2-byte pointers):
 *   period:          4 bytes (uint32_t)
 *   expire_time:     4 bytes (uint32_t / bsp_tick_t)
 *   callback:        2 bytes (function pointer)
 *   user_data:       2 bytes (void pointer)
 *   mode:            1 byte  (uint8_t)
 *   active:          1 byte  (uint8_t)
 *   TOTAL:          14 bytes/timer (coroutine state stored in user context)
 */
typedef struct {
    bsp_tick_t          period;        /**< Timer period in milliseconds */
    bsp_tick_t          expire_time;   /**< Expiration timestamp (for overflow algorithm) */
    timer_callback_t    callback;      /**< User callback function (can be NULL) */
    void               *user_data;     /**< User data passed to callback (StateSmith context, etc.) */
    uint8_t             mode;          /**< TIMER_MODE_ONE_SHOT or TIMER_MODE_REPEAT */
    uint8_t             active;        /**< 1=active, 0=inactive */
} timer_slot_t;

/**
 * @brief Timer pool structure (global state)
 *
 * Memory layout:
 *   slots:       MAX_TIMERS * 14 bytes (timer_slot_t array)
 *   used_bitmap: 4 bytes  (uint32_t, supports up to 32 timers)
 *   TOTAL:       MAX_TIMERS * 14 + 4 bytes
 *
 * For MAX_TIMERS=4: 4*14 + 4 = 60 bytes
 * For MAX_TIMERS=8: 8*14 + 4 = 116 bytes
 */
typedef struct {
    timer_slot_t    slots[MAX_TIMERS];  /**< Timer slot array */
    uint32_t        used_bitmap;        /**< Bitmap of used slots (bit 0 = slot 0, etc.) */
} safetimer_pool_t;

/* Compile-time validation: MAX_TIMERS must not exceed bitmap width */
#if MAX_TIMERS > 32
#error "SafeTimer bitmap only supports MAX_TIMERS <= 32"
#endif

/* ========== Internal Global State ========== */

/**
 * @brief Global timer pool (THE ONLY global variable)
 *
 * RAM Usage: ~60 bytes (for MAX_TIMERS=4), ~116 bytes (for MAX_TIMERS=8)
 *
 * @note Defined as static in safetimer.c (not exported)
 * @note Initialized to zero at startup (C standard guarantees)
 * @note Protected by BSP critical sections during modifications
 */
/* Note: g_timer_pool is defined as static in safetimer.c */

/* ========== Internal Helper Functions ========== */

/**
 * @brief Calculate signed difference between two tick values (handles wraparound)
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
 *
 * @note Internal use only - used by safetimer_process() and safetimer_get_remaining()
 */
STATIC int32_t safetimer_tick_diff(bsp_tick_t lhs, bsp_tick_t rhs);

/**
 * @brief Validate timer handle
 *
 * @param handle Timer handle to validate
 *
 * @return 1 if valid and active, 0 otherwise
 *
 * @note Internal use only - assumes ENABLE_PARAM_CHECK=1
 */
STATIC int validate_handle(safetimer_handle_t handle);

/**
 * @brief Find first free slot in timer pool
 *
 * @return Slot index (0 ~ MAX_TIMERS-1) if found, -1 if pool full
 *
 * @note Uses bitmap for O(n) search
 * @note Internal use only
 */
STATIC int find_free_slot(void);

/**
 * @brief Update expiration time for a timer slot
 *
 * @param slot_index Valid slot index (0 ~ MAX_TIMERS-1)
 * @param current_tick Current BSP tick value (captured outside critical section)
 *
 * @note Calculates: expire_time = current_tick + period
 * @note Handles 32-bit wraparound automatically
 * @note Internal use only - assumes validated input
 */
STATIC void update_expire_time(int slot_index, bsp_tick_t current_tick);

/**
 * @brief Trigger timer callback and handle mode
 *
 * @param slot_index Valid slot index (0 ~ MAX_TIMERS-1)
 * @param current_tick Current BSP tick value for repeat timer calculation
 * @param callback_out Output parameter for callback function pointer
 * @param user_data_out Output parameter for user data pointer
 *
 * @note For ONE_SHOT: stops timer after callback
 * @note For REPEAT: resets expire_time and continues
 * @note Extracts callback/user_data for caller to invoke outside critical section
 * @note Internal use only - assumes validated input and caller holds BSP lock
 */
STATIC void trigger_timer(int slot_index, bsp_tick_t current_tick,
                          timer_callback_t *callback_out, void **user_data_out);

/* ========== Configuration Validation ========== */

/**
 * @par Memory Usage Validation (Compile-Time):
 *
 * For SC8F072 (176 bytes RAM total):
 *   SafeTimer RAM: 60 bytes (MAX_TIMERS=4)
 *   Remaining:     116 bytes for user + stack
 *   Status:        ✓ Within 160-byte budget
 *
 * Calculation formula:
 *   RAM = MAX_TIMERS * sizeof(timer_slot_t) + 4
 *       = MAX_TIMERS * 14 + 4 bytes
 *
 * @warning Increasing MAX_TIMERS beyond 11 may exceed RAM budget!
 */
#if (MAX_TIMERS * 14 + 4) > 160
#error "SafeTimer RAM usage exceeds 160-byte budget!"
#endif

/**
 * @par Flash Usage Estimate (Implementation):
 *
 * Core implementation: ~800 bytes
 * Parameter checking:  ~150 bytes (if ENABLE_PARAM_CHECK=1)
 * TOTAL:               ~1.0KB (within 1.5KB budget ✓)
 */

#endif /* SAFETIMER_INTERNAL_H */
