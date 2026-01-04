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

/* ========== Handle Safety (ABA Protection) ========== */

/**
 * @brief ABA protection level for timer handles
 *
 * Controls how SafeTimer prevents "use-after-delete" bugs where a stale handle
 * accidentally operates on a newly created timer that reused the same slot.
 *
 * 0 = Disabled (no ABA protection)
 *     - Handle = slot index only (pure integer)
 *     - Saves 1 byte RAM (global next_generation counter)
 *     - ⚠️ Undefined behavior if handles are reused after safetimer_delete()
 *     - Use case: Static systems where timers are never deleted
 *
 * 1 = Debug-only (assertions in debug builds)
 *     - Generation counter exists, validated via assert() in validate_handle()
 *     - Requires ENABLE_PARAM_CHECK=1 to invoke validation
 *     - Flash savings in release (assert compiled out), but RAM cost same as Level 2
 *     - ✅ Recommended for production embedded systems
 *     - Catches ABA bugs during development
 *
 * 2 = Always enabled (default, full runtime protection)
 *     - Handle encodes [generation:3bit][index:5bit]
 *     - Runtime validation in all builds
 *     - 1 byte global overhead (next_generation counter)
 *     - Use case: General-purpose library, safety-critical systems
 *
 * RAM Impact (MAX_TIMERS=8):
 *   Level 0: 108 bytes (-1B global overhead)
 *   Level 1: 109 bytes (same as Level 2, Flash savings only)
 *   Level 2: 109 bytes (current implementation)
 *
 * NOTE: Per-timer RAM is unchanged across all levels because generation
 *       bits reuse the existing meta byte (bit-packed with mode/active)
 *
 * Flash Impact:
 *   Level 0: ~900 bytes
 *   Level 1: ~950 bytes (assertions only in debug)
 *   Level 2: ~1024 bytes
 *
 * @note Level 2 preserves exact v1.3.x behavior (backward compatible)
 * @note Level 1 is recommended for embedded production (safe + Flash savings)
 * @warning Level 0 requires developer discipline (like null-pointer safety)
 * @warning Level 1 requires ENABLE_PARAM_CHECK=1 to invoke assert() validation
 */
#ifndef SAFETIMER_ABA_PROTECTION
#define SAFETIMER_ABA_PROTECTION 2
#endif

/* Validate SAFETIMER_ABA_PROTECTION configuration */
#if SAFETIMER_ABA_PROTECTION == 1 && ENABLE_PARAM_CHECK == 0
#warning "SAFETIMER_ABA_PROTECTION=1 requires ENABLE_PARAM_CHECK=1 for assertions to work. Set ENABLE_PARAM_CHECK=1 or use Level 0/2 instead."
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
 * @note Modes 1/2 require user to call safetimer_tick_isr() from hardware timer ISR
 * @note See docs/porting_guide.md for detailed usage instructions
 */
#ifndef SAFETIMER_BSP_IMPLEMENTATION
#define SAFETIMER_BSP_IMPLEMENTATION 0  /* Default: user-provided */
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
#define BSP_TICK_TYPE_16BIT 1  /* Default: 16-bit for low RAM */
#endif

/**
 * @brief Timer state compression mode
 *
 * Control how timer state fields (mode, active, generation) are stored:
 *
 * 0 = Manual bit masking (Default)
 *     - Uses explicit bit operations with macros
 *     - Portable across all compilers (Keil C51, SDCC, GCC)
 *     - Explicit bit layout: bit7=mode, bit6=active, bit[2:0]=generation
 *     - Consistent with HANDLE_* macro style in codebase
 *
 * 1 = C bitfield
 *     - Uses C language bitfield syntax (uint8_t field : bits)
 *     - Cleaner code syntax (slot->active = 1)
 *     - Compiler-optimized access
 *     - Bit order is implementation-defined (may vary across compilers)
 *
 * Memory Impact (both modes):
 *   - Saves 2 bytes per timer (15 bytes -> 13 bytes)
 *   - For MAX_TIMERS=4: 65 bytes -> 57 bytes (-12.3%)
 *   - For MAX_TIMERS=8: 125 bytes -> 109 bytes (-12.8%)
 *
 * @note Default: 0 (manual masking) for maximum portability
 * @note If using single compiler only, mode 1 (bitfield) is safe and cleaner
 * @note Both modes provide identical functionality and RAM savings
 */
#ifndef USE_BITFIELD_META
#define USE_BITFIELD_META 0  /* Default: manual bit masking */
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

/* Validate SAFETIMER_ABA_PROTECTION */
#if SAFETIMER_ABA_PROTECTION < 0 || SAFETIMER_ABA_PROTECTION > 2
#error "SAFETIMER_ABA_PROTECTION must be 0, 1, or 2"
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
