/**
 * @file    bsp_default.c
 * @brief   Generic BSP Implementation for SafeTimer
 * @version 1.4.0
 * @date    2026-01-04
 *
 * @note    Only compiled when SAFETIMER_BSP_IMPLEMENTATION > 0
 * @note    Provides portable BSP for testing and simple embedded systems
 */

#include "bsp.h"
#include "safetimer_config.h"

#if (SAFETIMER_BSP_IMPLEMENTATION > 0)

/* ========================================================================== */
/*                         INTERNAL TICK COUNTER                              */
/* ========================================================================== */

/**
 * @brief Global tick counter (incremented by safetimer_tick_isr)
 *
 * This counter is maintained by the user's hardware timer ISR via the
 * safetimer_tick_isr() hook function.
 *
 * @note User MUST call safetimer_tick_isr() from hardware timer ISR
 * @note Thread-safe: single increment operation (atomic on most architectures)
 */
static volatile bsp_tick_t s_default_ticks = 0;

/* ========================================================================== */
/*                         PUBLIC HOOK API                                    */
/* ========================================================================== */

/**
 * @brief Tick hook for user's hardware timer ISR
 *
 * User must call this function from their 1ms hardware timer interrupt.
 * This is the bridge between hardware-specific timer code and SafeTimer's
 * portable BSP layer.
 *
 * @par Example Usage (SC8F072):
 * @code
 * void interrupt Timer0_Isr(void) {
 *     T0IF = 0;    // Clear hardware interrupt flag
 *     TMR0 = 0x06; // Reload timer for next 1ms
 *
 *     safetimer_tick_isr();  // Increment SafeTimer tick
 * }
 * @endcode
 *
 * @par Example Usage (STM32 HAL):
 * @code
 * void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
 *     if (htim->Instance == TIM2) {
 *         safetimer_tick_isr();
 *     }
 * }
 * @endcode
 *
 * @note This function is ISR-safe (single increment operation)
 * @note No critical section needed (atomic on 8/16/32-bit architectures)
 * @note Execution time: <1us on typical 8MHz 8-bit MCU
 */
void safetimer_tick_isr(void) {
    s_default_ticks++;
}

/* ========================================================================== */
/*                         BSP FUNCTION IMPLEMENTATIONS                       */
/* ========================================================================== */

/**
 * @brief Get current system tick count
 *
 * @return Tick count in milliseconds since power-on
 *
 * @note Returns the value maintained by safetimer_tick_isr()
 * @note On most architectures, reading bsp_tick_t is atomic
 * @note For 32-bit ticks on 8-bit MCUs, consider adding critical section
 */
bsp_tick_t bsp_get_ticks(void) {
    return s_default_ticks;
}

/**
 * @brief Enter critical section
 *
 * Behavior depends on SAFETIMER_BSP_IMPLEMENTATION:
 * - Mode 1: No-op (UNSAFE, requires SAFETIMER_ASSUME_SINGLE_THREADED)
 * - Mode 2: Runtime trap (while(1) loop)
 *
 * @warning Mode 1 is ONLY safe for single-threaded environments without ISRs
 * @warning If you have timer ISRs, use Mode 0 (custom BSP) instead
 */
void bsp_enter_critical(void) {
#if (SAFETIMER_BSP_IMPLEMENTATION == 1)
    /* No-op: Only safe for single-threaded environments */
    #ifndef SAFETIMER_ASSUME_SINGLE_THREADED
        #error "Mode 1 requires SAFETIMER_ASSUME_SINGLE_THREADED to be defined. \
                If you have ISRs, use Mode 0 (custom BSP) instead."
    #endif
    /* Intentionally empty */

#elif (SAFETIMER_BSP_IMPLEMENTATION == 2)
    /* Runtime trap: Forces user to acknowledge the issue */
    while(1) {
        /* TRAP: Default BSP cannot provide real critical sections.
         * Either:
         * 1. Define SAFETIMER_ASSUME_SINGLE_THREADED and use Mode 1, OR
         * 2. Set SAFETIMER_BSP_IMPLEMENTATION=0 and provide custom BSP
         */
    }
#endif
}

/**
 * @brief Exit critical section
 *
 * Restores interrupt state after bsp_enter_critical().
 * Must be paired with bsp_enter_critical().
 */
void bsp_exit_critical(void) {
#if (SAFETIMER_BSP_IMPLEMENTATION == 1)
    /* No-op */
#elif (SAFETIMER_BSP_IMPLEMENTATION == 2)
    /* Never reached (trapped in enter_critical) */
#endif
}

#endif /* SAFETIMER_BSP_IMPLEMENTATION > 0 */
