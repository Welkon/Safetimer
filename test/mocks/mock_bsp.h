/**
 * @file    mock_bsp.h
 * @brief   Mock BSP Implementation for PC-based Testing
 * @version 1.0.0
 * @date    2025-12-13
 *
 * @note This mock allows deterministic time control for unit testing
 * @warning Only for testing - DO NOT use in production firmware
 */

#ifndef MOCK_BSP_H
#define MOCK_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Include the real BSP interface */
#include "bsp.h"

/* ========== Mock Control Functions ========== */

/**
 * @brief Reset mock BSP to initial state
 *
 * Sets tick counter to 0 and clears critical section nesting.
 * Call this at the start of each test case.
 */
void mock_bsp_reset(void);

/**
 * @brief Set absolute tick value
 *
 * @param ticks New tick value
 *
 * @note Useful for testing wraparound scenarios
 *
 * @code
 * mock_bsp_set_ticks(0xFFFFFFF0);  // 16 ticks before wraparound
 * mock_bsp_advance_time(100);      // Wraps to 0x00000064
 * @endcode
 */
void mock_bsp_set_ticks(bsp_tick_t ticks);

/**
 * @brief Advance time by specified milliseconds
 *
 * @param ms Milliseconds to advance
 *
 * @note Simulates time progression without actually waiting
 */
void mock_bsp_advance_time(bsp_tick_t ms);

/**
 * @brief Get current mock tick value
 *
 * @return Current tick count
 *
 * @note For test verification only - production code should use bsp_get_ticks()
 */
bsp_tick_t mock_bsp_get_current_ticks(void);

/**
 * @brief Get critical section nesting level
 *
 * @return Nesting depth (0 = not in critical section)
 *
 * @note Used to verify critical section balance in tests
 */
int mock_bsp_get_critical_nesting(void);

/**
 * @brief Enable critical section validation
 *
 * @param enable 1 = enable validation, 0 = disable
 *
 * When enabled, detects unbalanced critical sections:
 * - Exit without enter
 * - Multiple enters without exit
 */
void mock_bsp_enable_validation(int enable);

/* ========== Mock Statistics ========== */

/**
 * @brief Mock call statistics
 */
typedef struct {
    unsigned long get_ticks_count;       /**< Number of bsp_get_ticks() calls */
    unsigned long enter_critical_count;  /**< Number of bsp_enter_critical() calls */
    unsigned long exit_critical_count;   /**< Number of bsp_exit_critical() calls */
} mock_bsp_stats_t;

/**
 * @brief Get mock call statistics
 *
 * @param stats Output statistics structure (can be NULL to reset only)
 *
 * @note Useful for verifying critical section usage
 */
void mock_bsp_get_stats(mock_bsp_stats_t *stats);

/**
 * @brief Reset statistics counters
 */
void mock_bsp_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_BSP_H */
