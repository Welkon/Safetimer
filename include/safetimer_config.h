/**
 * @file    safetimer_config.h
 * @brief   Compile-time Configuration for SafeTimer
 * @version 1.2.6
 * @date    2025-12-16
 *
 * @note User can override these settings by:
 *       1. Editing this file directly, OR
 *       2. Defining macros before including safetimer.h, OR
 *       3. Passing compiler flags: gcc -DMAX_TIMERS=16
 */

#ifndef SAFETIMER_CONFIG_H
#define SAFETIMER_CONFIG_H

/* ========== RAM/ROM Optimization Configuration ========== */

/**
 * @brief Enable coroutine support
 *
 * 0 = Disabled (saves ~200 bytes ROM, removes safetimer_advance_period)
 * 1 = Enabled (default, supports safetimer_coro.h)
 */
#ifndef SAFETIMER_ENABLE_CORO
#define SAFETIMER_ENABLE_CORO 1
#endif

/**
 * @brief Enable user_data context in timer callbacks
 *
 * 0 = Disabled (saves ~2 bytes/timer RAM, modifies API signature)
 * 1 = Enabled (default, standard void* user_data in callback)
 *
 * NOTE: Disabling this changes timer_callback_t signature:
 *   Enabled:  void (*timer_callback_t)(void *user_data);
 *   Disabled: void (*timer_callback_t)(void);
 */
#ifndef SAFETIMER_ENABLE_USER_DATA
#define SAFETIMER_ENABLE_USER_DATA 1
#endif

/**
 * @brief Restrict to REPEAT mode only
 *
 * 0 = Disabled (default, supports ONE_SHOT and REPEAT)
 * 1 = Enabled (saves ~50 bytes ROM, removes ONE_SHOT logic)
 *
 * NOTE: Enabling this removes TIMER_MODE_ONE_SHOT enum.
 */
#ifndef SAFETIMER_REPEAT_ONLY
#define SAFETIMER_REPEAT_ONLY 0
#endif

/* ========== API Configuration ========== */

/* ========== Timer Pool Configuration ========== */

/**
 * @brief Maximum number of concurrent timers
 *
 * Range: 1 ~ 32
 * Default: 4 timers (optimized for 176-byte RAM MCUs like SC8F072)
 *
 * RAM Impact (persistent, global variables):
 *   - Bitmap: 1 byte (MAX_TIMERS<=8) or 4 bytes (MAX_TIMERS>8)
 *   - Per timer slot: 13 bytes (32-bit tick) or 9 bytes (16-bit tick)
 * [Compressed]
 *   - Overhead: 2 bytes (s_processing + g_executing_handle)
 *
 *   MAX_TIMERS=4:  56 bytes (32-bit) | 40 bytes (16-bit)  (Includes overhead)
 *   MAX_TIMERS=8:  108 bytes (32-bit) | 74 bytes (16-bit)
 *   MAX_TIMERS=16: 215 bytes (32-bit) | 151 bytes (16-bit)
 *   MAX_TIMERS=32: 423 bytes (32-bit) | 295 bytes (16-bit)
 *
 * Stack Usage (temporary, during function calls):
 *   - safetimer_process(): ~20-30 bytes (local variables)
 *   - safetimer_advance_period(): ~30-40 bytes (local variables)
 *   - User callback: varies (part of call chain)
 *
 * Performance Impact: O(n) processing time
 *   4 timers  @ 8MHz = ~5us
 *   8 timers  @ 8MHz = ~10us
 *   16 timers @ 8MHz = ~20us
 *
 * @note For SC8F072 (176B RAM): MAX_TIMERS=4 leaves ~110B for user code
 * @note For larger RAM MCUs: Increase to 8, 16, or 32 as needed
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
 * @note Recommended: 0 for production (SC8F072 with 2KB Flash constraint)
 * @note Recommended: 1 for development/debugging
 * @note Core APIs (create/start/delete/process) are always available
 *
 * Migration Guide (if disabling):
 *   - Replace safetimer_stop() with safetimer_delete() + recreate pattern
 *   - Track timer states in application code instead of querying
 *   - Use compile-time pool sizing (MAX_TIMERS) instead of runtime queries
 */
#ifndef ENABLE_QUERY_API
#define ENABLE_QUERY_API 0 /* Default: disabled for minimal footprint */
#endif

/* ========== Optional Helper APIs ========== */

/**
 * @brief Enable convenience helper functions
 *
 * 0 = Disabled (minimal footprint)
 * 1 = Enabled (default, provides convenience wrappers)
 *
 * Affected APIs:
 *   - safetimer_create_started()      (inline, ~0 bytes if unused)
 *   - safetimer_create_started_batch() (inline, ~0 bytes if unused)
 *   - SAFETIMER_CREATE_STARTED_OR()   (macro)
 *
 * Flash Impact:
 *   These are inline functions/macros, only compiled if actually used.
 *   Disabling removes declarations to prevent accidental usage.
 *
 * Use Cases:
 *   Enabled (1):  Most applications benefit from simpler create+start API
 *   Disabled (0): Strict minimal API, explicit control preferred
 *
 * @note Default: 1 (enabled) - convenience functions are commonly used
 * @note Core APIs (create/start/delete/process) are always available
 */
#ifndef ENABLE_HELPER_API
#define ENABLE_HELPER_API 1 /* Default: enabled for convenience */
#endif

/* ========== REPEAT Timer Behavior ========== */

/**
 * @brief Enable catch-up behavior for REPEAT timers
 *
 * 0 = Disabled (default): Skip missed intervals, single callback fires
 * 1 = Enabled: Fire callbacks for each missed interval (burst mode)
 *
 * Flash Impact: ~30 bytes
 *
 * @note Default DISABLED for deterministic CPU usage
 */
#ifndef SAFETIMER_ENABLE_CATCHUP
#define SAFETIMER_ENABLE_CATCHUP 0
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

/* ========== Handle Safety (ABA Protection) ========== */

/**
 * @brief ABA protection level for timer handles
 *
 * Prevents "use-after-delete" bugs where stale handles access reallocated
 * slots.
 *
 * 0 = Disabled: No protection, saves 1 byte RAM
 * 1 = Debug-only: Assertions in debug builds (requires ENABLE_PARAM_CHECK=1)
 * 2 = Always enabled (default): Full runtime validation
 *
 * @note Level 2 recommended for safety-critical systems
 */
#ifndef SAFETIMER_ABA_PROTECTION
#define SAFETIMER_ABA_PROTECTION 2
#endif

/* Validate SAFETIMER_ABA_PROTECTION configuration */
#if SAFETIMER_ABA_PROTECTION == 1 && ENABLE_PARAM_CHECK == 0
#warning                                                                       \
    "SAFETIMER_ABA_PROTECTION=1 requires ENABLE_PARAM_CHECK=1 for assertions to work. Set ENABLE_PARAM_CHECK=1 or use Level 0/2 instead."
#endif

/* ========== BSP Implementation Selection ========== */

/**
 * @brief BSP Implementation Mode
 *
 * Controls which BSP implementation is compiled into the library.
 * User MUST choose exactly ONE option:
 *
 * 0 = User-provided (Default)
 *     - User implements bsp_get_ticks/enter_critical/exit_critical
 *     - No default implementation compiled
 *     - Suitable for: Production embedded systems with custom BSP
 *     - Flash Impact: 0 bytes (no default BSP code)
 *     - RAM Impact: 0 bytes
 *
 * 1 = Generic hosted (Single-threaded)
 *     - Provides safetimer_tick_isr() hook for user to call
 *     - No-op critical sections (UNSAFE for multi-threaded/ISR)
 *     - Requires: SAFETIMER_ASSUME_SINGLE_THREADED to be defined
 *     - Suitable for: Unit tests, PC simulation, cooperative schedulers
 *     - Flash Impact: ~80 bytes
 *     - RAM Impact: +4 bytes (tick counter)
 *
 * 2 = Generic hosted (Runtime trap)
 *     - Same as mode 1, but critical sections trap (while(1))
 *     - Forces user to acknowledge single-threaded assumption
 *     - Suitable for: Development/debugging
 *     - Flash Impact: ~100 bytes
 *     - RAM Impact: +4 bytes (tick counter)
 *
 * @note Mode 0 is MANDATORY for compilers without weak symbol support
 *       (SC8F072, Keil C51, etc.)
 * @note Modes 1/2 require user to call safetimer_tick_isr() from hardware timer
 * ISR
 * @note See docs/porting_guide.md for detailed usage instructions
 */
#ifndef SAFETIMER_BSP_IMPLEMENTATION
#define SAFETIMER_BSP_IMPLEMENTATION 0 /* Default: user-provided */
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
 * @note Default: 1 (16-bit) for low RAM MCUs
 *
 * @warning With 16-bit ticks, periods > 65535ms will be truncated!
 *          Always validate period_ms in application code or enable
 *          ENABLE_PARAM_CHECK to catch this at runtime.
 */
#ifndef BSP_TICK_TYPE_16BIT
#define BSP_TICK_TYPE_16BIT 1 /* Default: 16-bit for low RAM */
#endif

/**
 * @brief Timer state compression mode
 *
 * 0 = Manual bit masking (default): Portable across all compilers
 * 1 = C bitfield: Cleaner syntax, compiler-dependent bit order
 *
 * @note Both modes save 2 bytes per timer
 */
#ifndef USE_BITFIELD_META
#define USE_BITFIELD_META 1
#endif

/**
 * @brief Use stdint.h for integer types
 *
 * 0 = Use custom typedefs (for old 8-bit compilers without stdint.h)
 * 1 = Use stdint.h (default for GCC/Clang/modern compilers)
 */
#ifndef USE_STDINT_H
#if defined(__GNUC__) || defined(__clang__) || defined(UNIT_TEST) ||           \
    defined(__STDC_VERSION__)
#define USE_STDINT_H 1
#else
#define USE_STDINT_H 0
#endif
#endif

#if USE_STDINT_H
#include <stdint.h>
#else
/* Custom typedefs for C89 compatibility */
typedef unsigned char uint8_t;
typedef unsigned int uint16_t;
typedef unsigned long uint32_t;
typedef signed char int8_t;
typedef signed int int16_t;
typedef signed long int32_t;
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
#error "MAX_TIMERS must be <= 32 (bitmap limitation)"
#endif

/* Validate ENABLE_PARAM_CHECK */
#if ENABLE_PARAM_CHECK != 0 && ENABLE_PARAM_CHECK != 1
#error "ENABLE_PARAM_CHECK must be 0 or 1"
#endif

/* Validate ENABLE_QUERY_API */
#if ENABLE_QUERY_API != 0 && ENABLE_QUERY_API != 1
#error "ENABLE_QUERY_API must be 0 or 1"
#endif

/* Validate ENABLE_HELPER_API */
#if ENABLE_HELPER_API != 0 && ENABLE_HELPER_API != 1
#error "ENABLE_HELPER_API must be 0 or 1"
#endif

/* Validate SAFETIMER_ABA_PROTECTION */
#if SAFETIMER_ABA_PROTECTION < 0 || SAFETIMER_ABA_PROTECTION > 2
#error "SAFETIMER_ABA_PROTECTION must be 0, 1, or 2"
#endif

/* Validate SAFETIMER_ENABLE_CORO */
#if SAFETIMER_ENABLE_CORO != 0 && SAFETIMER_ENABLE_CORO != 1
#error "SAFETIMER_ENABLE_CORO must be 0 or 1"
#endif

/* Validate SAFETIMER_ENABLE_USER_DATA */
#if SAFETIMER_ENABLE_USER_DATA != 0 && SAFETIMER_ENABLE_USER_DATA != 1
#error "SAFETIMER_ENABLE_USER_DATA must be 0 or 1"
#endif

/* Validate SAFETIMER_REPEAT_ONLY */
#if SAFETIMER_REPEAT_ONLY != 0 && SAFETIMER_REPEAT_ONLY != 1
#error "SAFETIMER_REPEAT_ONLY must be 0 or 1"
#endif

/* ========== Configuration Summary ========== */

/**
 * @par RAM Usage: ~(MAX_TIMERS * 14 + 2) bytes
 * @par Recommended: MAX_TIMERS=4 for 176B RAM MCUs, =8 for 256B+ RAM
 */

#endif /* SAFETIMER_CONFIG_H */
