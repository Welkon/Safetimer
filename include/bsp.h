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
/* Integer types are defined in safetimer_config.h (included via safetimer.h) */
#include "safetimer_config.h"

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
 * @brief Tick hook for default BSP implementation (optional)
 *
 * When using SAFETIMER_BSP_IMPLEMENTATION=1 or 2, user must call this
 * function from their hardware timer ISR to increment the tick counter.
 *
 * @note Only available when SAFETIMER_BSP_IMPLEMENTATION > 0
 * @note Not needed when using custom BSP (Mode 0)
 *
 * @par Example Usage:
 * @code
 * void Timer0_ISR(void) {
 *     T0IF = 0;  // Clear interrupt flag
 *     safetimer_tick_isr();  // Increment SafeTimer tick
 * }
 * @endcode
 */
#if (SAFETIMER_BSP_IMPLEMENTATION > 0)
void safetimer_tick_isr(void);
#endif

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
 * @par BSP Implementation:
 * - Set SAFETIMER_BSP_IMPLEMENTATION=1 for default BSP (quick start)
 * - Keep =0 (default) and implement 3 functions for custom BSP
 *
 * @par See docs/porting_guide.md for detailed instructions.
 */

#endif /* BSP_H */
