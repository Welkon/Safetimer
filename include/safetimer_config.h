/**
 * @file    safetimer_config.h
 * @brief   Compile-time Configuration for SafeTimer
 * @version 1.2.5
 * @date    2025-12-16
 *
 * @note User can override these settings by:
 *       1. Editing this file directly, OR
 *       2. Defining macros before including safetimer.h, OR
 *       3. Passing compiler flags: gcc -DMAX_TIMERS=16
 */

#ifndef SAFETIMER_CONFIG_H
#define SAFETIMER_CONFIG_H

/* ========== Timer Pool Configuration ========== */

/**
 * @brief Maximum number of concurrent timers
 *
 * Range: 1 ~ 32 (limited by 8-bit bitmap in v1.0)
 * Default: 4 timers (optimized for 176-byte RAM MCUs like SC8F072)
 *
 * RAM Impact: 14 bytes per timer + 2 bytes overhead
 *   4 timers  = 58 bytes RAM  (recommended for SC8F072)
 *   8 timers  = 114 bytes RAM
 *   16 timers = 226 bytes RAM
 *   32 timers = 450 bytes RAM
 *
 * Performance Impact: O(n) processing time
 *   4 timers  @ 8MHz = ~5us
 *   8 timers  @ 8MHz = ~10us
 *   16 timers @ 8MHz = ~20us
 *
 * @note For SC8F072 (176B RAM): MAX_TIMERS=4 leaves ~74B for user code
 * @note For larger RAM MCUs: Increase to 8 or 16 as needed
 * @note Verify RAM budget before increasing (see DEC-002)
 */
#ifndef MAX_TIMERS
#define MAX_TIMERS 4
#endif

/* ========== Optional Query APIs ========== */

/**
 * @brief Enable optional query/diagnostic APIs
 *
 * 0 = Disabled (minimal footprint, production-optimized)
 * 1 = Enabled (full feature set, debugging support)
 *
 * Affected APIs:
 *   - safetimer_stop()          (~40 bytes Flash)
 *   - safetimer_get_status()    (~30 bytes Flash)
 *   - safetimer_get_remaining() (~80 bytes Flash)
 *   - safetimer_get_pool_usage() (~50 bytes Flash)
 *
 * Flash Impact:
 *   Disabled (0): Saves ~200 bytes (20% of total library size)
 *   Enabled (1):  Full API available
 *
 * Use Cases:
 *   Disabled: Static embedded applications where timer states are
 *             managed explicitly by application code (typical 8-bit MCU usage)
 *   Enabled:  Dynamic applications requiring runtime state inspection,
 *             debugging, or diagnostic features
 *
 * @note Recommended: 0 for production (SC8F072 with 1KB Flash constraint)
 * @note Recommended: 1 for development/debugging
 * @note Core APIs (create/start/delete/process) are always available
 *
 * Migration Guide (if disabling):
 *   - Replace safetimer_stop() with safetimer_delete() + recreate pattern
 *   - Track timer states in application code instead of querying
 *   - Use compile-time pool sizing (MAX_TIMERS) instead of runtime queries
 */
#ifndef ENABLE_QUERY_API
#define ENABLE_QUERY_API 0  /* Default: disabled for minimal footprint */
#endif

/* ========== REPEAT Timer Behavior ========== */

/**
 * @brief Enable catch-up behavior for REPEAT timers
 *
 * 0 = Disabled (skip missed intervals, default)
 * 1 = Enabled (fire callbacks for each missed interval)
 *
 * Behavior Comparison:
 *
 * Scenario: 100ms period timer, system blocked for 350ms
 *
 * DISABLED (0 - default):
 *   - Single callback fires when system resumes
 *   - expire_time advances to next future interval (400ms)
 *   - Missed intervals (100ms, 200ms, 300ms) are skipped
 *   - No burst callbacks, deterministic CPU usage
 *   - Suitable for: LED blink, UI updates, timeouts, heartbeats
 *
 * ENABLED (1):
 *   - Multiple callbacks fire in rapid succession (burst)
 *   - Each call to safetimer_process() triggers one callback
 *   - expire_time: 100ms → 200ms → 300ms → 400ms
 *   - Ensures exact count of callbacks over time
 *   - Suitable for: sampling counters, integrators, event statistics
 *
 * Flash Impact: ~30 bytes (while loop vs single increment)
 *
 * Performance Impact:
 *   DISABLED: O(n) where n = missed intervals (inside critical section)
 *   ENABLED:  O(1) per safetimer_process() call (but multiple calls needed)
 *
 * Embedded Systems Recommendation:
 *   Default DISABLED because burst callbacks can:
 *   - Starve cooperative schedulers
 *   - Toggle GPIOs erratically
 *   - Break communication protocols
 *   - Cause unpredictable timing in ISR-sensitive systems
 *
 * @note v1.2.4 behavior = ENABLED (1)
 * @note v1.2.5+ default = DISABLED (0) to restore pre-v1.2.4 expectations
 * @note For v1.3.0: Use TIMER_MODE_REPEAT_CATCHUP instead of this macro
 */
#ifndef SAFETIMER_ENABLE_CATCHUP
#define SAFETIMER_ENABLE_CATCHUP 0  /* Default: skip missed intervals */
#endif

/* ========== Parameter Validation ========== */

/**
 * @brief Enable parameter checking in public APIs
 *
 * 0 = Disabled (faster, smaller code)
 * 1 = Enabled (safer, recommended for development)
 *
 * Flash Impact:
 *   Enabled:  ~150 bytes additional code
 *   Disabled: ~0 bytes (checks compiled out)
 *
 * Performance Impact:
 *   Enabled:  ~5-10 CPU cycles per API call
 *   Disabled: 0 cycles
 *
 * @note Recommended: 1 for debug builds, 0 for release builds
 * @warning Disabling removes all handle/parameter validation!
 */
#ifndef ENABLE_PARAM_CHECK
#define ENABLE_PARAM_CHECK 1
#endif

/* ========== Compiler Compatibility ========== */

/**
 * @brief BSP tick type width configuration
 *
 * 0 = 32-bit tick (uint32_t) - Standard, supports up to 49.7 days
 * 1 = 16-bit tick (uint16_t) - Ultra-low memory, supports up to 65.5 seconds
 *
 * RAM Impact:
 *   32-bit: Uses 4 bytes per tick variable
 *   16-bit: Uses 2 bytes per tick variable (saves 8 bytes total)
 *
 * Memory Savings (16-bit vs 32-bit):
 *   - Per timer slot: 4 bytes (period + expire_time)
 *   - BSP tick counter: 4 bytes (s_system_ticks)
 *   - Total for 4 timers: ~20 bytes saved
 *
 * Timer Period Limits:
 *   32-bit: 1ms ~ 2,147,483,647ms (~24.8 days)
 *   16-bit: 1ms ~ 65,535ms (65.5 seconds)
 *
 * @note For SC8F072 (160B RAM): Use 16-bit to maximize available memory
 * @note For standard MCUs (>256B RAM): Use 32-bit for longer periods
 * @note Default: 0 (32-bit) for maximum compatibility
 *
 * @warning With 16-bit ticks, periods > 65535ms will be truncated!
 *          Always validate period_ms in application code or enable
 *          ENABLE_PARAM_CHECK to catch this at runtime.
 */
#ifndef BSP_TICK_TYPE_16BIT
#define BSP_TICK_TYPE_16BIT 0  /* Default: 32-bit for compatibility */
#endif

/**
 * @brief Use stdint.h for integer types
 *
 * 0 = Use custom typedefs (for old compilers without stdint.h)
 * 1 = Use stdint.h (C99/C11 compilers)
 *
 * @note SDCC 4.x, GCC 9+, Keil C51 9.x support stdint.h
 * @note Older Keil versions may require 0
 * @note For PC testing (UNIT_TEST), always use stdint.h
 */
#ifndef USE_STDINT_H
#ifdef UNIT_TEST
#define USE_STDINT_H 1  /* PC tests always use stdint.h */
#else
#define USE_STDINT_H 0
#endif
#endif

#if USE_STDINT_H
#include <stdint.h>
#else
/* Custom typedefs for C89 compatibility */
typedef unsigned char  uint8_t;
typedef unsigned int   uint16_t;
typedef unsigned long  uint32_t;
typedef signed char    int8_t;
typedef signed int     int16_t;
typedef signed long    int32_t;
#endif

/* ========== Debugging & Testing ========== */

/**
 * @brief Enable debug assertions
 *
 * 0 = Disabled (production)
 * 1 = Enabled (development/testing)
 *
 * @note Only affects internal consistency checks
 * @note Does NOT affect ENABLE_PARAM_CHECK
 */
#ifndef ENABLE_DEBUG_ASSERT
#define ENABLE_DEBUG_ASSERT 0
#endif

/**
 * @brief Expose static functions for unit testing
 *
 * Defines STATIC as empty for testing, or static for production.
 *
 * Usage in source code:
 *   STATIC int find_free_slot(void)  // static in production, public in test
 *
 * @note Set to 1 when compiling unit tests
 */
#ifdef UNIT_TEST
#define STATIC
#else
#define STATIC static
#endif

/* ========== Configuration Validation ========== */

/* Validate MAX_TIMERS range */
#if MAX_TIMERS < 1
#error "MAX_TIMERS must be >= 1"
#endif

#if MAX_TIMERS > 32
#error "MAX_TIMERS must be <= 32 (bitmap limitation in v1.0)"
#endif

/* Warn about RAM usage */
#if MAX_TIMERS > 8
#error "Current implementation only supports up to 8 timers (due to uint8_t bitmap). To use more, upgrade used_bitmap to uint32_t."
#endif

/* Validate ENABLE_PARAM_CHECK */
#if ENABLE_PARAM_CHECK != 0 && ENABLE_PARAM_CHECK != 1
#error "ENABLE_PARAM_CHECK must be 0 or 1"
#endif

/* Validate ENABLE_QUERY_API */
#if ENABLE_QUERY_API != 0 && ENABLE_QUERY_API != 1
#error "ENABLE_QUERY_API must be 0 or 1"
#endif

/* ========== Configuration Summary ========== */

/**
 * @par Current Configuration:
 *
 * Timers:    MAX_TIMERS (compile-time value shown during build)
 * RAM Usage: ~(MAX_TIMERS * 14 + 2) bytes
 * Validation: ENABLE_PARAM_CHECK (0=off, 1=on)
 * stdint.h:  USE_STDINT_H (0=custom, 1=standard)
 *
 * @par Tuning Guidelines:
 *
 * For SC8F072 (176 bytes RAM) - Default Configuration:
 *   MAX_TIMERS=4, ENABLE_PARAM_CHECK=0 (release)
 *   Result: 58 bytes RAM, ~0.9KB Flash, 74 bytes free for user
 *
 * For standard applications (256-512 bytes RAM):
 *   MAX_TIMERS=8, ENABLE_PARAM_CHECK=1
 *   Result: 114 bytes RAM, ~1.0KB Flash
 *
 * For more timers (1KB+ RAM):
 *   MAX_TIMERS=16, ENABLE_PARAM_CHECK=1, USE_STDINT_H=1
 *   Result: 226 bytes RAM, ~1.2KB Flash
 *
 * For development/testing:
 *   ENABLE_PARAM_CHECK=1, ENABLE_DEBUG_ASSERT=1
 *   Result: Safer, easier debugging
 */

#endif /* SAFETIMER_CONFIG_H */
