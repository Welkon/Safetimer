/**
 * @file    safetimer.h
 * @brief   SafeTimer - Lightweight Embedded Timer Library
 * @version 1.2.6
 * @date    2025-12-16
 * @author  SafeTimer Project
 * @license MIT
 */

#ifndef SAFETIMER_H
#define SAFETIMER_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Version Information ========== */
#define SAFETIMER_VERSION       "1.2.6"
#define SAFETIMER_VERSION_MAJOR 1
#define SAFETIMER_VERSION_MINOR 2
#define SAFETIMER_VERSION_PATCH 5

/* ========== Configuration ========== */
#include "safetimer_config.h"

/* ========== BSP Interface ========== */
#include "bsp.h"

/* ========== Type Definitions ========== */

/**
 * @brief Timer handle type
 *
 * Valid handles are in range [0, MAX_TIMERS-1].
 * SAFETIMER_INVALID_HANDLE (-1) indicates an invalid or failed operation.
 */
typedef int safetimer_handle_t;

/**
 * @brief Invalid timer handle constant
 */
#define SAFETIMER_INVALID_HANDLE (-1)

/**
 * @brief Timer mode enumeration
 */
typedef enum {
    TIMER_MODE_ONE_SHOT = 0,  /**< Timer fires once and stops */
    TIMER_MODE_REPEAT   = 1   /**< Timer fires repeatedly */
} timer_mode_t;

/**
 * @brief Timer error codes
 */
typedef enum {
    TIMER_OK            =  0,  /**< Success */
    TIMER_ERR_FULL      = -1,  /**< Timer pool is full */
    TIMER_ERR_INVALID   = -2,  /**< Invalid parameter */
    TIMER_ERR_NOT_FOUND = -3   /**< Timer not found or inactive */
} timer_error_t;

/**
 * @brief Timer callback function type
 *
 * @param user_data User data pointer passed during timer creation
 *
 * @warning Callback MUST NOT create, delete, or modify other timers (ADR-003)
 * @warning Keep callback execution time minimal (< 100us recommended)
 */
typedef void (*timer_callback_t)(void *user_data);

/* ========== Public API ========== */

/**
 * @brief Create a new timer
 *
 * Creates and initializes a timer but does not start it. User must
 * call safetimer_start() to begin countdown.
 *
 * @param period_ms Timer period in milliseconds (1 ~ 2^31-1)
 * @param mode      TIMER_MODE_ONE_SHOT or TIMER_MODE_REPEAT
 * @param callback  Expiration callback, can be NULL (delay-only usage)
 * @param user_data User data pointer passed to callback
 *
 * @return Valid handle (0 ~ MAX_TIMERS-1) on success
 * @retval SAFETIMER_INVALID_HANDLE Pool full or invalid parameters
 *
 * @note Period must not exceed 2^31 ms (~24.8 days) due to overflow algorithm
 * @warning Callback must NOT create/delete timers (causes reentrancy issues)
 *
 * @code
 * safetimer_handle_t h = safetimer_create(1000, TIMER_MODE_REPEAT, my_callback, NULL);
 * if (h != SAFETIMER_INVALID_HANDLE)
 * {
 *     safetimer_start(h);
 * }
 * @endcode
 */
safetimer_handle_t safetimer_create(
    uint32_t            period_ms,
    timer_mode_t        mode,
    timer_callback_t    callback,
    void               *user_data
);

/**
 * @brief Start a timer
 *
 * Starts the countdown for a previously created timer.
 *
 * @param handle Valid timer handle from safetimer_create()
 *
 * @return TIMER_OK on success, error code otherwise
 * @retval TIMER_ERR_INVALID Invalid handle
 * @retval TIMER_ERR_NOT_FOUND Timer not found or deleted
 */
timer_error_t safetimer_start(safetimer_handle_t handle);

/**
 * @brief Delete a timer
 *
 * Permanently deletes a timer and releases its slot. Handle becomes
 * invalid after this call.
 *
 * @param handle Valid timer handle
 *
 * @return TIMER_OK on success, error code otherwise
 * @retval TIMER_ERR_INVALID Invalid handle
 */
timer_error_t safetimer_delete(safetimer_handle_t handle);

/**
 * @brief Dynamically change timer period
 *
 * Updates the period of an existing timer. If the timer is running, it will
 * restart immediately from the current moment with the new period. If stopped,
 * the new period takes effect on next safetimer_start().
 *
 * @param handle        Valid timer handle from safetimer_create()
 * @param new_period_ms New period in milliseconds (1 ~ 2^31-1)
 *
 * @return TIMER_OK on success, error code otherwise
 * @retval TIMER_ERR_INVALID Invalid handle or period
 * @retval TIMER_ERR_NOT_FOUND Timer not found or deleted
 *
 * @warning Behavior equivalent to "restart with new period"
 *   - Running timers: Countdown restarts from current tick + new_period
 *   - Breaks REPEAT mode phase-locking (v1.2.4 feature) intentionally
 *   - This is a design trade-off to avoid extra RAM overhead
 *
 * @note Usage Modes
 *   Mode 1 - Immediate effect (button control):
 *     Call from button handler, frequency changes instantly
 *
 *   Mode 2 - Preserve phase-locking (smooth transition):
 *     Call from timer callback, change takes effect at next trigger
 *
 * @note Common Pitfall Prevention
 *   ⚠️ safetimer_handle_t is an integer index, NOT a pointer!
 *   ❌ Wrong: timer_led->period = 500;  // CRASH! Undefined behavior
 *   ✅ Right: safetimer_set_period(timer_led, 500);
 *
 * @code
 * // Example 1: Button-controlled LED speed
 * void on_button_press(void) {
 *     current_period = (current_period > 100) ? current_period - 100 : 1000;
 *     safetimer_set_period(led_timer, current_period);  // Immediate
 * }
 *
 * // Example 2: Smooth transition (preserves phase)
 * static uint32_t target_period = 1000;
 * void led_callback(void *data) {
 *     toggle_led();
 *     safetimer_set_period(timer, target_period);  // At trigger point
 * }
 * void on_button_press(void) {
 *     target_period -= 100;  // Only update target, callback handles it
 * }
 * @endcode
 */
timer_error_t safetimer_set_period(
    safetimer_handle_t  handle,
    uint32_t            new_period_ms
);

/**
 * @brief Advance timer period (phase-locked, zero cumulative error)
 *
 * Unlike safetimer_set_period() which resets the timer countdown from
 * the current tick, safetimer_advance_period() advances the expire_time
 * by the new period while maintaining the previous timing phase. This
 * eliminates cumulative timing error in periodic tasks like coroutines.
 *
 * **Key Difference:**
 * - `safetimer_set_period()`: expire_time = current_tick + new_period (resets phase)
 * - `safetimer_advance_period()`: expire_time += new_period (preserves phase)
 *
 * **Use Cases:**
 * - Coroutine SAFETIMER_CORO_SLEEP() (called internally by macro)
 * - Any periodic task requiring zero-drift timing (LED blink, sensor polling)
 * - Long-running battery-powered applications (eliminates drift over days/months)
 *
 * **Behavior:**
 * - Active timer: Advances expire_time from last scheduled expiration
 * - Inactive timer: Behaves like set_period() (no previous phase to preserve)
 * - Overflow-safe: Uses ADR-005 wraparound algorithm
 *
 * @param handle Valid timer handle returned by safetimer_create()
 * @param new_period_ms New period in milliseconds (1 ~ 2^31-1)
 *
 * @return TIMER_OK on success, error code otherwise
 * @retval TIMER_ERR_INVALID Invalid handle or period out of range (0 or >2^31-1)
 * @retval TIMER_ERR_NOT_FOUND Timer has been deleted
 *
 * @note Thread-safe (uses BSP critical section)
 * @note If timer execution is severely delayed, advances to the next future period
 * @note Introduced in v1.3.1 to fix coroutine cumulative timing error
 *
 * @par Timing Error Comparison:
 * @code
 * // Using safetimer_set_period() (OLD):
 * // 100ms period, 1 year runtime -> ~52.6 minutes cumulative error
 *
 * // Using safetimer_advance_period() (NEW):
 * // 100ms period, infinite runtime -> ZERO cumulative error
 * @endcode
 *
 * @par Example (internal coroutine use):
 * @code
 * // User code:
 * SAFETIMER_CORO_SLEEP(100);  // Sleeps exactly 100ms per cycle
 *
 * // Macro expansion (calls advance_period internally):
 * safetimer_advance_period(ctx->_coro_handle, 100);
 * @endcode
 *
 * @see safetimer_set_period() for "reset from now" behavior
 * @see SAFETIMER_CORO_SLEEP() macro in safetimer_coro.h
 */
timer_error_t safetimer_advance_period(
    safetimer_handle_t  handle,
    uint32_t            new_period_ms
);

/**
 * @brief Process all active timers (MUST be called periodically)
 *
 * Checks all active timers against current system tick and triggers
 * callbacks for expired timers. This function MUST be called regularly
 * in the main loop or from a periodic interrupt.
 *
 * @note Call frequency should be >= 2x the shortest timer period
 * @note Execution time: ~10us for 8 timers @ 8MHz (O(n) algorithm)
 * @warning NEVER call from interrupt context if callbacks do I/O
 */
void safetimer_process(void);

/* ========== Optional Query/Diagnostic APIs ========== */
#if ENABLE_QUERY_API

/**
 * @brief Stop a timer
 *
 * Stops a running timer without deleting it. Timer can be restarted
 * with safetimer_start().
 *
 * @param handle Valid timer handle
 *
 * @return TIMER_OK on success, error code otherwise
 * @retval TIMER_ERR_INVALID Invalid handle
 * @retval TIMER_ERR_NOT_FOUND Timer not found or deleted
 *
 * @note Requires ENABLE_QUERY_API=1 in safetimer_config.h
 * @note For minimal footprint, consider safetimer_delete() instead
 */
timer_error_t safetimer_stop(safetimer_handle_t handle);

/**
 * @brief Get timer running status
 *
 * @param handle     Valid timer handle
 * @param is_running Output: 1 if running, 0 if stopped (can be NULL)
 *
 * @return TIMER_OK on success, error code otherwise
 * @retval TIMER_ERR_INVALID Invalid handle or null output pointer
 * @retval TIMER_ERR_NOT_FOUND Timer not found or deleted
 *
 * @note Requires ENABLE_QUERY_API=1 in safetimer_config.h
 */
timer_error_t safetimer_get_status(
    safetimer_handle_t  handle,
    int                *is_running
);

/**
 * @brief Get remaining time until expiration
 *
 * @param handle        Valid timer handle
 * @param remaining_ms  Output: remaining milliseconds (can be NULL)
 *
 * @return TIMER_OK on success, error code otherwise
 * @retval TIMER_ERR_INVALID Invalid handle or null output pointer
 * @retval TIMER_ERR_NOT_FOUND Timer not found or deleted
 *
 * @note Returns 0 for stopped timers
 * @note Requires ENABLE_QUERY_API=1 in safetimer_config.h
 */
timer_error_t safetimer_get_remaining(
    safetimer_handle_t  handle,
    uint32_t           *remaining_ms
);

/**
 * @brief Get timer pool usage statistics
 *
 * @param used_count  Output: number of active timers (can be NULL)
 * @param total_count Output: total pool capacity (can be NULL)
 *
 * @return TIMER_OK on success
 *
 * @note Requires ENABLE_QUERY_API=1 in safetimer_config.h
 * @note For static applications, MAX_TIMERS is known at compile time
 */
timer_error_t safetimer_get_pool_usage(
    int  *used_count,
    int  *total_count
);

#endif /* ENABLE_QUERY_API */

#ifdef __cplusplus
}
#endif

#endif /* SAFETIMER_H */
