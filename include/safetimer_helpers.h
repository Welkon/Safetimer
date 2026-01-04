/**
 * @file    safetimer_helpers.h
 * @brief   SafeTimer Convenience Functions (Optional Helper Layer)
 * @version 1.1.0
 * @date    2025-12-14
 * @author  SafeTimer Project
 * @license MIT
 *
 * @note This header provides optional convenience functions for common usage
 * patterns. Include this ONLY if you want simplified APIs. For explicit
 * control, use the core safetimer.h API directly.
 *
 * @par Usage Example:
 * @code
 * // Option 1: Use helpers for immediate-start scenarios (most common)
 * #include "safetimer_helpers.h"
 * led_timer = safetimer_create_started(500, TIMER_MODE_REPEAT, blink_led,
 * NULL);
 *
 * // Option 2: Use core API for delayed-start scenarios (cascaded timers)
 * #include "safetimer.h"
 * stage2_timer = safetimer_create(1000, TIMER_MODE_ONE_SHOT, callback, NULL);
 * // ... start later conditionally
 * safetimer_start(stage2_timer);
 * @endcode
 *
 * @warning These helpers are designed for bare-metal environments where
 *          safetimer_process() runs in a simple main loop. They may not be
 *          suitable for complex RTOS integration scenarios.
 */

#ifndef SAFETIMER_HELPERS_H
#define SAFETIMER_HELPERS_H

/* ========== Dependencies ========== */
#include "safetimer.h"
#include <stddef.h> /* For NULL */

/* ========== Version Information ========== */
#define SAFETIMER_HELPERS_VERSION "1.1.0"

/* ========== Convenience Functions ========== */

/**
 * @brief Create and immediately start a timer (convenience wrapper)
 *
 * Combines safetimer_create() and safetimer_start() for immediate-start
 * scenarios. If start fails, the timer is automatically deleted to prevent
 * resource leaks.
 *
 * @param period_ms  Timer period in milliseconds (1 ~ 2^31-1)
 * @param mode       TIMER_MODE_ONE_SHOT or TIMER_MODE_REPEAT
 * @param callback   Callback function (can be NULL for delay-only usage)
 * @param user_data  User data passed to callback
 *
 * @return Valid handle if BOTH create AND start succeed
 * @retval SAFETIMER_INVALID_HANDLE if creation or start fails
 *
 * @note Zero-overhead inline function
 * @note NOT suitable for cascaded/conditional timers (use core API)
 */
static inline safetimer_handle_t
safetimer_create_started(uint32_t period_ms, timer_mode_t mode,
                         timer_callback_t callback, void *user_data) {
  /* Step 1: Create timer (allocates slot, initializes state) */
  safetimer_handle_t handle =
      safetimer_create(period_ms, mode, callback, user_data);

  if (handle != SAFETIMER_INVALID_HANDLE) {
    /* Step 2: Attempt to start timer */
    timer_error_t err = safetimer_start(handle);

    if (err != TIMER_OK) {
      /*
       * Start failed - cleanup allocated timer to prevent resource leak.
       * This ensures atomicity: either you get a running timer or nothing.
       */
      safetimer_delete(handle);
      return SAFETIMER_INVALID_HANDLE;
    }
  }

  /* Success: Timer created AND started, or creation failed (handle is INVALID)
   */
  return handle;
}

/**
 * @brief Create and start multiple timers with identical parameters (batch
 * helper)
 *
 * This helper simplifies creating several timers with the same configuration
 * but different callbacks/user_data.
 *
 * @param count      Number of timers to create (0~255, must be ≤ pool slots)
 * @param period_ms  Timer period in milliseconds
 * @param mode       TIMER_MODE_ONE_SHOT or TIMER_MODE_REPEAT
 * @param callbacks  Array of callback functions (length = count)
 * @param user_data  Array of user data pointers (length = count)
 * @param handles    Output array for timer handles (length = count)
 *
 * @return Number of timers successfully created and started (uint8_t)
 *
 * @note Partial success is possible (returns < count). Check individual
 * handles.
 * @note If any creation/start fails, that handle will be
 * SAFETIMER_INVALID_HANDLE
 *
 * @code
 * // Example: Create 3 LED timers with different periods
 * safetimer_handle_t timers[3];
 * timer_callback_t callbacks[] = {led1_cb, led2_cb, led3_cb};
 * void *data[] = {NULL, NULL, NULL};
 *
 * int created = safetimer_create_started_batch(3, 500, TIMER_MODE_REPEAT,
 *                                               callbacks, data, timers);
 * if (created < 3) {
 *     // Handle partial failure
 * }
 * @endcode
 */
static inline uint8_t
safetimer_create_started_batch(uint8_t count, uint32_t period_ms,
                               timer_mode_t mode, timer_callback_t *callbacks,
                               void **user_data, safetimer_handle_t *handles) {
  uint8_t success_count = 0;
  uint8_t i;

  if (handles == NULL || callbacks == NULL) {
    return 0;
  }

  for (i = 0; i < count; i++) {
    void *data = (user_data != NULL) ? user_data[i] : NULL;
    handles[i] = safetimer_create_started(period_ms, mode, callbacks[i], data);

    if (handles[i] != SAFETIMER_INVALID_HANDLE) {
      success_count++;
    }
  }

  return success_count;
}

/* ========== Macro Helpers ========== */

/**
 * @brief Create-start-check macro for error-checked timer creation
 *
 * This macro wraps safetimer_create_started() with automatic error checking,
 * reducing boilerplate in initialization code.
 *
 * @param handle     Variable to store timer handle
 * @param period_ms  Timer period
 * @param mode       Timer mode
 * @param callback   Callback function
 * @param user_data  User data pointer
 * @param error_handler Code to execute on failure (e.g., return, goto)
 *
 * @code
 * // Example: Create timer with automatic error handling
 * safetimer_handle_t led_timer;
 * SAFETIMER_CREATE_STARTED_OR(led_timer, 500, TIMER_MODE_REPEAT, blink_led,
 * NULL, { printf("Failed to create LED timer\n"); return -1;
 * });
 * // Timer guaranteed to be valid here
 * @endcode
 */
#define SAFETIMER_CREATE_STARTED_OR(handle, period_ms, mode, callback,         \
                                    user_data, error_handler)                  \
  do {                                                                         \
    (handle) = safetimer_create_started((period_ms), (mode), (callback),       \
                                        (user_data));                          \
    if ((handle) == SAFETIMER_INVALID_HANDLE) {                                \
      error_handler                                                            \
    }                                                                          \
  } while (0)

#endif /* SAFETIMER_HELPERS_H */
