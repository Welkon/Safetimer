/**
 * @file    safetimer_coro.h
 * @brief   SafeTimer Coroutine Adapter (Timer-Integrated Coroutines)
 * @version 1.4.0
 * @date    2026-01-04
 * @author  SafeTimer Project
 * @license MIT
 *
 * This is an ADAPTER layer that integrates the standalone coro_base.h
 * coroutines with SafeTimer's timing features (WAIT, WAIT_UNTIL).
 *
 * ## Architecture:
 * - Standalone coroutines: Use coro_base.h directly (zero dependencies)
 * - Timer-integrated coroutines: Use this file (requires SafeTimer)
 *
 * ## Key Features:
 * - Extends coro_base.h with timer handle binding
 * - Automatic handle binding (no manual assignment needed)
 * - Zero-drift WAIT via safetimer_advance_period()
 * - Backward compatible with existing code
 *
 * ## Usage Pattern:
 * @code
 * typedef struct {
 *     SAFETIMER_CORO_CONTEXT;  // Extends CORO_CONTEXT with _coro_handle
 *     int counter;
 * } my_coro_ctx_t;
 *
 * void my_coro_task(void *user_data) {
 *     my_coro_ctx_t *ctx = (my_coro_ctx_t *)user_data;
 *
 *     SAFETIMER_CORO_BEGIN(ctx);  // Auto-binds handle on first run
 *     while(1) {
 *         toggle_led();
 *         SAFETIMER_CORO_WAIT(500);  // Zero-drift timing
 *         ctx->counter++;
 *     }
 *     SAFETIMER_CORO_END();
 * }
 * @endcode
 */

#ifndef SAFETIMER_CORO_H
#define SAFETIMER_CORO_H

#include "coro_base.h"
#include "safetimer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Extended Coroutine Context ========== */

/**
 * @brief SafeTimer coroutine context (extends CORO_CONTEXT)
 *
 * Adds timer handle binding to the base coroutine context.
 * Users must embed this at the START of their context struct.
 *
 * @note Inherits _coro_lc from CORO_CONTEXT (line number for resumption)
 * @note Adds _coro_handle for timer period modification (SLEEP/WAIT_UNTIL)
 * @note Auto-binding: handle is automatically set on first SAFETIMER_CORO_BEGIN()
 *
 * @par Memory Cost:
 * - 3 bytes total: 2 bytes (_coro_lc) + 1 byte (_coro_handle)
 *
 * @par Example:
 * @code
 * typedef struct {
 *     SAFETIMER_CORO_CONTEXT;  // Expands to _coro_lc + _coro_handle
 *     int my_data;             // User fields follow
 * } my_context_t;
 * @endcode
 */
#define SAFETIMER_CORO_CONTEXT \
    CORO_CONTEXT; \
    safetimer_handle_t _coro_handle

/* ========== Coroutine Control Flow (Adapter Layer) ========== */

/**
 * @brief Begin SafeTimer coroutine body (with auto-binding)
 *
 * Extends CORO_BEGIN with automatic handle binding on first execution.
 * Must be paired with SAFETIMER_CORO_END().
 *
 * @param ctx Pointer to context struct (first member must be SAFETIMER_CORO_CONTEXT)
 *
 * @note Auto-binding: Detects unbound handle (0 or -1) and captures current handle
 * @note Delegates to CORO_BEGIN for core state machine logic
 * @note Backward compatible: works with {0} initialization and manual assignment
 */
#define SAFETIMER_CORO_BEGIN(ctx) \
    if ((ctx)->_coro_handle == SAFETIMER_INVALID_HANDLE || \
        (ctx)->_coro_handle == 0) { \
        (ctx)->_coro_handle = safetimer_get_current_handle(); \
    } \
    CORO_BEGIN(ctx)

/**
 * @brief End SafeTimer coroutine body
 *
 * Delegates to CORO_END (closes switch statement).
 */
#define SAFETIMER_CORO_END() CORO_END()

/**
 * @brief Yield execution (delegates to base coroutine)
 *
 * Does NOT modify timer period (timer keeps its current period).
 */
#define SAFETIMER_CORO_YIELD() CORO_YIELD()

/**
 * @brief Wait for specified milliseconds
 *
 * Sets timer period to `ms` and yields. Timer must be in TIMER_MODE_REPEAT.
 *
 * @param ms Delay time in milliseconds (1 ~ 2^31-1)
 *
 * @note Modifies timer period, execution resumes after ~ms milliseconds
 * @note Actual delay depends on safetimer_process() call frequency
 *
 * ✅ **FIXED in v1.3.1: Zero Cumulative Error**
 *
 * This macro now uses `safetimer_advance_period()` internally, which
 * maintains phase-locking and eliminates cumulative timing error.
 *
 * **Timing Accuracy:**
 * - **v1.3.0 (OLD):** Used `safetimer_set_period()` → 0.01% drift per cycle
 *   - 1 hour: +0.36s, 1 day: +8.64s, 1 year: +52.6 minutes ❌
 * - **v1.3.1 (NEW):** Uses `safetimer_advance_period()` → ZERO drift
 *   - Infinite runtime: 0 seconds cumulative error ✅
 *
 * **Technical Details:**
 * - OLD: `expire_time = current_tick + ms` (resets from now)
 * - NEW: `expire_time += ms` (advances from last expiration)
 * - Overflow-safe: Uses ADR-005 wraparound algorithm
 * - Thread-safe: BSP critical section protection
 *
 * **Suitable for:**
 * - ✅ LED blink (any duration)
 * - ✅ Sensor polling (long-term)
 * - ✅ UART timeouts
 * - ✅ Battery-powered applications (days/months runtime)
 * - ✅ Precise PWM generation (when combined with hardware timers)
 *
 * @par Example:
 * @code
 * SAFETIMER_CORO_WAIT(1000);  // Wait exactly 1 second per cycle
 * led_toggle();               // Zero cumulative drift over time
 * @endcode
 */
#define SAFETIMER_CORO_WAIT(ms) do { \
    if ((ctx)->_coro_handle != SAFETIMER_INVALID_HANDLE && \
        (ctx)->_coro_handle != 0) { \
        safetimer_advance_period((ctx)->_coro_handle, (ms)); \
    } \
    (ctx)->_coro_lc = __LINE__; return; \
    case __LINE__:; \
} while(0)

/**
 * @brief DEPRECATED: Use SAFETIMER_CORO_WAIT instead
 * @deprecated Renamed to SAFETIMER_CORO_WAIT for semantic consistency
 * @note This alias will be removed in v2.0.0
 */
#define SAFETIMER_CORO_SLEEP(ms) SAFETIMER_CORO_WAIT(ms)

/**
 * @brief Wait until condition is true
 *
 * Polls `cond` every `poll_ms` milliseconds until it becomes true.
 *
 * @param cond Condition expression (evaluated each poll)
 * @param poll_ms Polling interval in milliseconds
 *
 * @note Blocks coroutine until condition is met
 * @note If condition never becomes true, coroutine never advances
 *
 * @par Example:
 * @code
 * SAFETIMER_CORO_WAIT_UNTIL(uart_has_data(), 10);  // Check every 10ms
 * data = uart_read();  // Executes only when data available
 * @endcode
 */
#define SAFETIMER_CORO_WAIT_UNTIL(cond, poll_ms) do { \
    if ((ctx)->_coro_handle != SAFETIMER_INVALID_HANDLE && \
        (ctx)->_coro_handle != 0) { \
        safetimer_set_period((ctx)->_coro_handle, (poll_ms)); \
    } \
    (ctx)->_coro_lc = __LINE__; \
    case __LINE__: \
    if (!(cond)) return; \
} while(0)

/**
 * @brief Reset coroutine to beginning (delegates to base coroutine)
 *
 * Next invocation will start from SAFETIMER_CORO_BEGIN().
 */
#define SAFETIMER_CORO_RESET() CORO_RESET()

/**
 * @brief Exit coroutine permanently (delegates to base coroutine)
 *
 * Coroutine enters infinite yield state.
 * @note Timer continues running - manually call safetimer_stop() to halt timer
 */
#define SAFETIMER_CORO_EXIT() CORO_EXIT()

/* ========== Coroutine Helper Functions ========== */

/**
 * @brief Check if coroutine has exited (delegates to base coroutine)
 */
#define SAFETIMER_CORO_IS_EXITED(ctx) CORO_IS_EXITED(ctx)

/**
 * @brief Manually reset coroutine from outside (delegates to base coroutine)
 */
#define SAFETIMER_CORO_RESET_EXTERNAL(ctx) CORO_RESET_EXTERNAL(ctx)

/* ========== Usage Guidelines ========== */

/**
 * @par Coroutine Limitations (Protothread Constraints):
 *
 * - Local variables are NOT preserved across yields
 *   ✗ Wrong: int x = 5; CORO_SLEEP(100); printf("%d", x);  // x is lost!
 *   ✓ Right: Store variables in context struct
 *
 * - Cannot use switch/case inside coroutine body
 *   ✗ Wrong: switch(state) { ... } inside CORO_BEGIN/END
 *   ✓ Right: Use if-else chains or call external switch functions
 *
 * - Cannot use __LINE__ macro in user code
 *   ✗ Wrong: printf("Line: %d", __LINE__);
 *   ✓ Right: Avoid __LINE__ within coroutine scope
 *
 * @par Best Practices:
 *
 * 1. Always embed SAFETIMER_CORO_CONTEXT as first member
 * 2. Store all persistent data in context struct
 * 3. Keep coroutines simple - complex logic in separate functions
 * 4. Use StateSmith for complex state machines (separate timer)
 * 5. Test edge cases: timer deletion, rapid start/stop
 *
 * @par Interaction with StateSmith:
 *
 * Coroutines and StateSmith state machines can coexist:
 * - Use separate timers for each
 * - StateSmith uses user_data for state machine context
 * - Coroutines use their own context with SAFETIMER_CORO_CONTEXT
 * - Both can run simultaneously without conflict
 */

#ifdef __cplusplus
}
#endif

#endif /* SAFETIMER_CORO_H */
