/**
 * @file    safetimer_sem.h
 * @brief   SafeTimer Semaphore Support (Tiny-Macro-OS inspired)
 * @version 1.3.0
 * @date    2025-12-19
 * @author  SafeTimer Project
 * @license MIT
 *
 * Provides lightweight semaphore primitives for inter-coroutine communication
 * and synchronization. Based on Tiny-Macro-OS semaphore design.
 *
 * ## Key Features:
 * - Counting semaphores with timeout support
 * - Interrupt-safe signal/wait operations
 * - Zero RAM overhead (user allocates semaphore variables)
 *
 * ## Usage Pattern:
 * @code
 * safetimer_sem_t data_ready_sem;
 * SAFETIMER_SEM_INIT(data_ready_sem);
 *
 * // Producer (interrupt or another coroutine)
 * void data_isr(void) {
 *     SAFETIMER_SEM_SIGNAL(data_ready_sem);
 * }
 *
 * // Consumer (coroutine)
 * SAFETIMER_CORO_WAIT_SEM(data_ready_sem, 10, 100);  // Wait max 1000ms
 * if (data_ready_sem == SAFETIMER_SEM_TIMEOUT) {
 *     handle_timeout();
 * }
 * @endcode
 */

#ifndef SAFETIMER_SEM_H
#define SAFETIMER_SEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations for BSP functions (user must include bsp.h or safetimer.h) */
extern void bsp_enter_critical(void);
extern void bsp_exit_critical(void);

/* ========== Type Definitions ========== */

/**
 * @brief Semaphore type (must be signed for timeout detection)
 *
 * Values:
 * - 0: Signaled (ready to proceed)
 * - > 0: Waiting (countdown in progress)
 * - SAFETIMER_SEM_TIMEOUT (-1): Timeout occurred
 *
 * @note Must be signed type for timeout detection
 * @note int8_t ensures atomic read/write on 8-bit MCUs (max timeout count: 126)
 */
typedef int8_t safetimer_sem_t;

/**
 * @brief Semaphore timeout value
 *
 * When SAFETIMER_CORO_WAIT_SEM times out, the semaphore is set to this value.
 */
#define SAFETIMER_SEM_TIMEOUT  (-1)

/* ========== Semaphore Primitives ========== */

/**
 * @brief Initialize semaphore
 *
 * Sets semaphore to 0 (signaled state).
 *
 * @param sem Semaphore variable
 *
 * @par Example:
 * @code
 * safetimer_sem_t my_sem;
 * SAFETIMER_SEM_INIT(my_sem);  // my_sem = 0
 * @endcode
 */
#define SAFETIMER_SEM_INIT(sem)  (sem) = 0

/**
 * @brief Signal semaphore (set to ready state)
 *
 * Wakes up waiting coroutine by setting semaphore to 0.
 *
 * @param sem Semaphore variable
 *
 * @note Can be called from interrupt context (single instruction)
 * @note If semaphore is in timeout state, this clears it
 *
 * @par Example:
 * @code
 * void uart_rx_isr(void) {
 *     SAFETIMER_SEM_SIGNAL(uart_sem);  // Wake up coroutine
 * }
 * @endcode
 */
#define SAFETIMER_SEM_SIGNAL(sem)  do { (sem) = 0; } while(0)

/**
 * @brief Safe signal semaphore (skip if timed out)
 *
 * Only signals if semaphore is NOT in timeout state.
 *
 * @param sem Semaphore variable
 *
 * @note Use when signal may arrive after timeout
 * @note Prevents overwriting timeout indication
 *
 * @par Example:
 * @code
 * void delayed_signal(void) {
 *     SAFETIMER_SEM_SIGNAL_SAFE(my_sem);  // No-op if already timed out
 * }
 * @endcode
 */
#define SAFETIMER_SEM_SIGNAL_SAFE(sem)  do { \
    if ((sem) != SAFETIMER_SEM_TIMEOUT) (sem) = 0; \
} while(0)

/* ========== Coroutine Semaphore Wait ========== */

/**
 * @brief Wait for semaphore with timeout (for use in coroutines)
 *
 * Blocks coroutine until semaphore is signaled or timeout occurs.
 *
 * @param sem Semaphore variable
 * @param poll_ms Polling interval in milliseconds
 * @param timeout_count Maximum number of polls before timeout (max 126 for int8_t)
 *
 * @note Total timeout = poll_ms × timeout_count milliseconds
 * @note After timeout, sem == SAFETIMER_SEM_TIMEOUT
 * @note Requires SAFETIMER_CORO_BEGIN/END context
 * @note Thread-safe: uses BSP critical sections
 *
 * @par Example:
 * @code
 * SAFETIMER_CORO_WAIT_SEM(uart_sem, 10, 100);  // Max 1000ms
 * if (uart_sem == SAFETIMER_SEM_TIMEOUT) {
 *     uart_timeout_handler();
 *     SAFETIMER_CORO_RESET();  // Restart coroutine
 * } else {
 *     process_uart_data();     // Semaphore signaled in time
 * }
 * @endcode
 *
 * @warning Can ONLY be used inside SAFETIMER_CORO_BEGIN/END block
 * @warning ctx variable must be defined (from SAFETIMER_CORO_BEGIN)
 * @warning timeout_count must be ≤ 126 (int8_t limitation)
 */
#define SAFETIMER_CORO_WAIT_SEM(sem, poll_ms, timeout_count) do { \
    bsp_enter_critical(); \
    if ((sem) == 0) { \
        bsp_exit_critical(); \
        break;  /* Already signaled */ \
    } \
    (sem) = (timeout_count) + 1; \
    bsp_exit_critical(); \
    safetimer_set_period((ctx)->_coro_handle, (poll_ms)); \
    (ctx)->_coro_lc = __LINE__; \
    case __LINE__: \
    bsp_enter_critical(); \
    if ((sem) == 0) { \
        bsp_exit_critical(); \
        break;  /* Signaled during yield */ \
    } \
    if ((sem) > 1) { \
        (sem)--; \
        bsp_exit_critical(); \
        return; \
    } else { \
        if ((sem) != 0) (sem) = SAFETIMER_SEM_TIMEOUT; \
        bsp_exit_critical(); \
    } \
} while(0)

/**
 * @brief Wait for semaphore indefinitely (no timeout)
 *
 * Blocks coroutine until semaphore is signaled.
 *
 * @param sem Semaphore variable
 * @param poll_ms Polling interval in milliseconds
 *
 * @note Will wait forever unless semaphore is signaled
 * @note Use with caution - can cause deadlocks
 *
 * @par Example:
 * @code
 * SAFETIMER_CORO_WAIT_SEM_FOREVER(critical_sem, 10);
 * // Execution never proceeds unless semaphore is signaled
 * @endcode
 */
#define SAFETIMER_CORO_WAIT_SEM_FOREVER(sem, poll_ms) do { \
    (sem) = 1; \
    safetimer_set_period((ctx)->_coro_handle, (poll_ms)); \
    (ctx)->_coro_lc = __LINE__; \
    case __LINE__: \
    if ((sem) > 0) return; \
} while(0)

/* ========== Usage Guidelines ========== */

/**
 * @par Semaphore Patterns:
 *
 * 1. **Producer-Consumer:**
 * @code
 * // Producer (interrupt)
 * void data_ready_isr(void) {
 *     buffer[write_idx++] = read_sensor();
 *     SAFETIMER_SEM_SIGNAL(data_sem);
 * }
 *
 * // Consumer (coroutine)
 * SAFETIMER_CORO_WAIT_SEM(data_sem, 10, 50);
 * process_buffer();
 * @endcode
 *
 * 2. **Event Synchronization:**
 * @code
 * // Event trigger
 * if (button_pressed()) {
 *     SAFETIMER_SEM_SIGNAL(button_sem);
 * }
 *
 * // Event handler
 * SAFETIMER_CORO_WAIT_SEM(button_sem, 10, 100);
 * handle_button_press();
 * @endcode
 *
 * 3. **Timeout Handling:**
 * @code
 * SAFETIMER_CORO_WAIT_SEM(ack_sem, 10, 300);  // 3 second timeout
 * if (ack_sem == SAFETIMER_SEM_TIMEOUT) {
 *     retry_transmission();
 * } else {
 *     transmission_confirmed();
 * }
 * @endcode
 *
 * @par Best Practices:
 *
 * - Always check for SAFETIMER_SEM_TIMEOUT after wait
 * - Use SIGNAL_SAFE in delayed/interrupt contexts
 * - Avoid WAIT_SEM_FOREVER unless you have external reset mechanism
 * - Keep semaphore variables global or static (persistent storage)
 * - Never use semaphore as loop counter (undefined behavior)
 *
 * @par Interrupt Safety:
 *
 * - SAFETIMER_SEM_SIGNAL is interrupt-safe (single assignment)
 * - Semaphore must be int16_t for atomic read/write on 8-bit MCUs
 * - No critical sections needed for basic signal/wait operations
 */

#ifdef __cplusplus
}
#endif

#endif /* SAFETIMER_SEM_H */
