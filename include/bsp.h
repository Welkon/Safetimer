/**
 * @file    bsp.h
 * @brief   Board Support Package (BSP) Interface for SafeTimer
 * @version 1.2.1
 * @date    2025-12-16
 *
 * @note User MUST implement exactly 3 functions for target platform
 */

#ifndef BSP_H
#define BSP_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Type Definitions ========== */

/**
 * @brief BSP tick type (milliseconds)
 *
 * Width depends on BSP_TICK_TYPE_16BIT configuration:
 * - BSP_TICK_TYPE_16BIT=0: uint32_t (default) - supports up to 49.7 days
 * - BSP_TICK_TYPE_16BIT=1: uint16_t - supports up to 65.5 seconds, saves 8
 * bytes RAM
 *
 * @note MUST represent milliseconds since power-on/reset
 * @note Automatically wraps around at max value (SafeTimer handles this)
 *
 * @warning With 16-bit ticks: max timer period is 65535ms (65.5 seconds)
 *          Use safetimer_tick_diff() for correct wraparound handling
 */
/**
 * @brief Fixed-width integer types
 *
 * Default: Uses standard <stdint.h> (preferred)
 * When NO_STDINT_H=1: Uses C89 fallback typedefs for legacy compilers
 *
 * @note Most modern compilers support stdint.h (SDCC, Keil, IAR, GCC, etc.)
 *       Only define NO_STDINT_H=1 for very old compilers without stdint.h
 */
#ifndef NO_STDINT_H
#include <stdint.h>
#else
/* C89 fallback: define standard integer types (only for legacy compilers) */
typedef signed char int8_t;     /*  8-bit signed: -128 ~ 127 */
typedef signed int int16_t;     /* 16-bit signed (assumes 16-bit int) */
typedef signed long int32_t;    /* 32-bit signed */
typedef unsigned char uint8_t;  /*  8-bit unsigned: 0 ~ 255 */
typedef unsigned int uint16_t;  /* 16-bit unsigned (assumes 16-bit int) */
typedef unsigned long uint32_t; /* 32-bit unsigned */
#endif

/**
 * @brief BSP tick type selection based on BSP_TICK_TYPE_16BIT
 */
#if BSP_TICK_TYPE_16BIT
typedef uint16_t bsp_tick_t; /* 16-bit: 0 ~ 65535 ms (65.5 seconds max) */
#else
typedef uint32_t bsp_tick_t; /* 32-bit: 0 ~ 4294967295 ms (49.7 days max) */
#endif

/* ========== BSP Functions (USER MUST IMPLEMENT) ========== */

/**
 * @brief Get current system tick in milliseconds
 *
 * @return Current tick count (milliseconds since power-on)
 *
 * @note MUST return monotonically increasing value
 * @note MUST have ≤1ms resolution for timer accuracy
 * @note Wraps around at 2^32-1 (~49.7 days) - this is NORMAL
 * @note SafeTimer automatically handles wraparound (ADR-005)
 *
 * @warning MUST be fast (<10us execution time)
 * @warning MUST be reentrant if called from interrupt context
 *
 * @par Example Implementation (SC8F072):
 * @code
 * static volatile bsp_tick_t s_system_ticks = 0;
 *
 * // Timer0 ISR: increments every 1ms
 * void timer0_isr(void) __interrupt(1)
 * {
 *     s_system_ticks++;
 * }
 *
 * bsp_tick_t bsp_get_ticks(void)
 * {
 *     return s_system_ticks;
 * }
 * @endcode
 */
bsp_tick_t bsp_get_ticks(void);

/**
 * @brief Enter critical section (disable interrupts)
 *
 * Disables interrupts to protect timer pool modifications from
 * concurrent access. MUST be paired with bsp_exit_critical().
 *
 * @note Implementation typically disables global interrupts
 * @note May use interrupt flag or RTOS mutex on multithreaded systems
 *
 * @warning Keep critical sections SHORT (<10 instructions)
 * @warning DO NOT call functions inside critical sections
 * @warning DO NOT use loops inside critical sections
 *
 * @par Example Implementation (SC8F072):
 * @code
 * void bsp_enter_critical(void)
 * {
 *     EA = 0;  // Disable global interrupts
 * }
 * @endcode
 */
void bsp_enter_critical(void);

/**
 * @brief Exit critical section (enable interrupts)
 *
 * Re-enables interrupts after critical section. MUST be paired with
 * bsp_enter_critical().
 *
 * @note MUST restore interrupt state to pre-enter_critical condition
 *
 * @par Example Implementation (SC8F072):
 * @code
 * void bsp_exit_critical(void)
 * {
 *     EA = 1;  // Enable global interrupts
 * }
 * @endcode
 */
void bsp_exit_critical(void);

/* ========== BSP Requirements Summary ========== */

/**
 * @par BSP Integration Checklist:
 *
 * Hardware Timer Setup:
 * - [ ] Configure hardware timer for 1ms interrupt
 * - [ ] Verify timer interrupt fires accurately (±1% acceptable)
 * - [ ] Implement ISR that increments tick counter
 *
 * bsp_get_ticks():
 * - [ ] Returns 32-bit unsigned milliseconds
 * - [ ] Monotonically increasing (never decrements)
 * - [ ] Allows wraparound at 2^32-1
 * - [ ] Execution time <10us
 *
 * bsp_enter_critical() / bsp_exit_critical():
 * - [ ] Disable/enable interrupts correctly
 * - [ ] Properly paired (no nesting in v1.0)
 * - [ ] Fast execution (<5 instructions each)
 *
 * Testing:
 * - [ ] Verify tick accuracy with oscilloscope/logic analyzer
 * - [ ] Test critical section protects shared data
 * - [ ] Verify no interrupt loss during critical sections
 *
 * @par Porting Guide:
 * See docs/porting_guide.md for detailed instructions.
 *
 * @par Example Platforms:
 * - examples/sc8f072/bsp_sc8f072.c - SC8F072 (SDCC)
 * - examples/stm8/bsp_stm8.c       - STM8 (SDCC)
 * - examples/stc8/bsp_stc8.c       - STC8 (Keil C51)
 */

#ifdef __cplusplus
}
#endif

#endif /* BSP_H */
