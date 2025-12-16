/**
 * @file    safetimer_single.h
 * @brief   SafeTimer - Single-File Header Version
 * @version 1.2.1-single
 * @date    2025-12-16
 * @author  SafeTimer Project
 * @license MIT
 *
 * @note This is a single-file amalgamation of SafeTimer for easy integration.
 *       For production use, consider using the multi-file version for better
 *       modularity and maintainability.
 *
 * Usage:
 *   1. Copy safetimer_single.h and safetimer_single.c to your project
 *   2. Implement BSP functions (bsp_get_ticks, bsp_enter/exit_critical)
 *   3. Include "safetimer_single.h" in your code
 *   4. Call safetimer_process() in your main loop
 *
 * Configuration:
 *   Edit the macros below to customize SafeTimer for your platform:
 *   - MAX_TIMERS: Number of concurrent timers (1-8)
 *   - ENABLE_PARAM_CHECK: Enable/disable parameter validation
 */

#ifndef SAFETIMER_SINGLE_H
#define SAFETIMER_SINGLE_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                         CONFIGURATION SECTION                              */
/* ========================================================================== */

/**
 * @brief Maximum number of concurrent timers
 * Range: 1-8 (default: 1 for SC8F072 ultra-low memory)
 * RAM Impact: ~14 bytes per timer + 2 bytes overhead
 *
 * Memory usage on SC8F072 (160 bytes total):
 *   MAX_TIMERS = 1: ~39 bytes (24%) - Ultra-low memory mode
 *   MAX_TIMERS = 2: ~53 bytes (33%)
 *   MAX_TIMERS = 3: ~67 bytes (42%)
 */
#ifndef MAX_TIMERS
#define MAX_TIMERS 1  /* Ultra-low memory: 1 timer only */
#endif

/**
 * @brief Enable optional query/diagnostic APIs
 * 0 = Disabled (saves ~200 bytes ROM), 1 = Enabled
 *
 * Affected APIs: safetimer_stop(), safetimer_get_status(),
 *                safetimer_get_remaining(), safetimer_get_pool_usage()
 *
 * Default: 0 (disabled) for minimal footprint
 * Set to 1 for development/debugging
 */
#ifndef ENABLE_QUERY_API
#define ENABLE_QUERY_API 0  /* Disabled by default for SC8F072 */
#endif

/**
 * @brief Enable parameter checking in public APIs
 * 0 = Disabled (faster, saves ~60 bytes ROM), 1 = Enabled (safer)
 *
 * ⚠️ 注意：此选项仅影响 Program space (ROM/Flash)，不影响 RAM
 * Default: 1 (enabled) for safety. Disable in production if ROM is critical.
 */
#ifndef ENABLE_PARAM_CHECK
#define ENABLE_PARAM_CHECK 1  /* Enabled by default for safety */
#endif

/**
 * @brief Use stdint.h for integer types
 * 0 = Custom typedefs (C89), 1 = stdint.h (C99)
 */
#ifndef USE_STDINT_H
#define USE_STDINT_H 0
#endif

/* ========================================================================== */
/*                         TYPE DEFINITIONS                                   */
/* ========================================================================== */

/* Integer types */
#if USE_STDINT_H
#include <stdint.h>
#else
typedef unsigned char uint8_t;
typedef unsigned int uint16_t;
typedef unsigned long uint32_t;
typedef signed long int32_t;
#endif

/* NULL definition */
#ifndef NULL
#define NULL ((void *)0)
#endif

/* BSP tick type */
#ifndef BSP_TICK_TYPE_16BIT
#define BSP_TICK_TYPE_16BIT 1  /* 1 = uint16_t (saves 8 bytes), 0 = uint32_t */
#endif

#if BSP_TICK_TYPE_16BIT
typedef uint16_t bsp_tick_t;  /* Max: 65.5 seconds, saves 8 bytes RAM */
#else
typedef uint32_t bsp_tick_t;  /* Max: 49.7 days */
#endif

/* Timer handle type */
typedef int safetimer_handle_t;
#define SAFETIMER_INVALID_HANDLE (-1)

/* Timer mode enumeration */
typedef enum {
  TIMER_MODE_ONE_SHOT = 0, /**< Timer fires once and stops */
  TIMER_MODE_REPEAT = 1    /**< Timer fires repeatedly */
} timer_mode_t;

/* Error codes */
typedef enum {
  SAFETIMER_OK = 0,
  SAFETIMER_ERR_INVALID_HANDLE = -1,
  SAFETIMER_ERR_INVALID_PARAM = -2,
  SAFETIMER_ERR_POOL_FULL = -3,
  SAFETIMER_ERR_NOT_RUNNING = -4
} safetimer_err_t;

/* Timer callback function pointer */
typedef void (*timer_callback_t)(void *user_data);

/* ========================================================================== */
/*                         BSP INTERFACE (USER MUST IMPLEMENT)                */
/* ========================================================================== */

/**
 * @brief Get current system tick count in milliseconds
 * @return Current tick value (wraps around at 2^32-1)
 * @note Must be implemented by user in bsp.c
 */
extern bsp_tick_t bsp_get_ticks(void);

/**
 * @brief Enter critical section (disable interrupts)
 * @note Must be implemented by user in bsp.c
 */
extern void bsp_enter_critical(void);

/**
 * @brief Exit critical section (enable interrupts)
 * @note Must be implemented by user in bsp.c
 */
extern void bsp_exit_critical(void);

/* ========================================================================== */
/*                         PUBLIC API FUNCTIONS                               */
/* ========================================================================== */

/**
 * @brief Create a new timer (does not start it)
 *
 * @param period_ms Timer period in milliseconds
 *                  - If BSP_TICK_TYPE_16BIT=1: Valid range is 1-65535 ms (65.5s max)
 *                    Values > 65535 will be SILENTLY TRUNCATED when parameter
 *                    checking is disabled. Enable ENABLE_PARAM_CHECK=1 for validation.
 *                  - If BSP_TICK_TYPE_16BIT=0: Valid range is 1-2147483647 ms (~24.8 days)
 * @param mode Timer mode (ONE_SHOT or REPEAT)
 * @param callback Callback function (can be NULL)
 * @param user_data User data passed to callback
 * @return Timer handle or SAFETIMER_INVALID_HANDLE on error
 *
 * @warning With 16-bit ticks and parameter checking disabled, periods > 65535ms
 *          will wrap around silently. Always validate inputs in application code
 *          or enable ENABLE_PARAM_CHECK during development.
 */
safetimer_handle_t safetimer_create(uint32_t period_ms, timer_mode_t mode,
                                    timer_callback_t callback, void *user_data);

/**
 * @brief Start a timer
 * @param handle Timer handle
 * @return SAFETIMER_OK on success, error code otherwise
 */
safetimer_err_t safetimer_start(safetimer_handle_t handle);

/**
 * @brief Delete a timer (release slot)
 * @param handle Timer handle
 * @return SAFETIMER_OK on success, error code otherwise
 */
safetimer_err_t safetimer_delete(safetimer_handle_t handle);

/**
 * @brief Process all timers (call regularly in main loop)
 * @note Must be called at least twice per shortest timer period
 */
void safetimer_process(void);

/* ========== Optional Query/Diagnostic APIs ========== */
#if ENABLE_QUERY_API

/**
 * @brief Stop a timer (requires ENABLE_QUERY_API=1)
 * @param handle Timer handle
 * @return SAFETIMER_OK on success, error code otherwise
 */
safetimer_err_t safetimer_stop(safetimer_handle_t handle);

/**
 * @brief Get timer status (requires ENABLE_QUERY_API=1)
 * @param handle Timer handle
 * @param is_running Output: 1 if running, 0 if stopped
 * @return SAFETIMER_OK on success, error code otherwise
 */
safetimer_err_t safetimer_get_status(safetimer_handle_t handle,
                                     int *is_running);

/**
 * @brief Get remaining time until timer expires (requires ENABLE_QUERY_API=1)
 * @param handle Timer handle
 * @param remaining_ms Output: remaining milliseconds
 * @return SAFETIMER_OK on success, error code otherwise
 */
safetimer_err_t safetimer_get_remaining(safetimer_handle_t handle,
                                        uint32_t *remaining_ms);

/**
 * @brief Get timer pool usage (requires ENABLE_QUERY_API=1)
 * @param used Output: number of used slots
 * @param total Output: total number of slots
 * @return SAFETIMER_OK on success, error code otherwise
 */
safetimer_err_t safetimer_get_pool_usage(int *used, int *total);

#endif /* ENABLE_QUERY_API */

/* ========================================================================== */
/*                    INTERNAL STRUCTURES (DO NOT USE DIRECTLY)               */
/* ========================================================================== */

/* Internal timer slot structure */
typedef struct {
  bsp_tick_t period;
  bsp_tick_t expire_time;
  timer_callback_t callback;
  void *user_data;
  uint8_t mode;
  uint8_t active;
} timer_slot_t;

/* Internal timer pool structure */
typedef struct {
  timer_slot_t slots[MAX_TIMERS];
  uint8_t used_bitmap;
} safetimer_pool_t;

#ifdef __cplusplus
}
#endif

#endif /* SAFETIMER_SINGLE_H */
