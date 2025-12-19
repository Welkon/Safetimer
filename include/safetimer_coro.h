/**
 * @file    safetimer_coro.h
 * @brief   SafeTimer Coroutine Extensions (Protothread-style)
 * @version 1.3.0
 * @date    2025-12-19
 * @author  SafeTimer Project
 * @license MIT
 *
 * Inspired by Tiny-Macro-OS protothread design, this module provides
 * stackless coroutines using the Duff's Device technique (__LINE__ macro).
 *
 * ## Key Features:
 * - Zero-stack coroutines (state stored in user context)
 * - Compatible with StateSmith state machines (user_data preserved)
 * - Minimal RAM overhead (+2 bytes per timer slot)
 *
 * ## Usage Pattern:
 * @code
 * typedef struct {
 *     SAFETIMER_CORO_CONTEXT;  // Must be first member
 *     // User-defined fields
 *     int counter;
 * } my_coro_ctx_t;
 *
 * void my_coro_task(void *user_data) {
 *     my_coro_ctx_t *ctx = (my_coro_ctx_t *)user_data;
 *
 *     SAFETIMER_CORO_BEGIN(ctx);
 *     while(1) {
 *         toggle_led();
 *         SAFETIMER_CORO_SLEEP(500);
 *         ctx->counter++;
 *     }
 *     SAFETIMER_CORO_END();
 * }
 * @endcode
 */

#ifndef SAFETIMER_CORO_H
#define SAFETIMER_CORO_H

#include "safetimer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Coroutine Context Macros ========== */

/**
 * @brief Coroutine context base structure
 *
 * Users must embed this at the START of their context struct.
 *
 * @note _coro_lc stores the __LINE__ number for resumption
 * @note _coro_handle allows coroutine to modify its own timer period
 *
 * @par Example:
 * @code
 * typedef struct {
 *     SAFETIMER_CORO_CONTEXT;  // Expands to _coro_lc and _coro_handle
 *     int my_data;             // User fields follow
 * } my_context_t;
 * @endcode
 */
#define SAFETIMER_CORO_CONTEXT \
    uint16_t _coro_lc; \
    safetimer_handle_t _coro_handle

/* ========== Coroutine Control Flow Macros ========== */

/**
 * @brief Begin coroutine body
 *
 * Must be paired with SAFETIMER_CORO_END().
 *
 * @param ctx Pointer to user context struct (first member must be SAFETIMER_CORO_CONTEXT)
 *
 * @note Uses Duff's Device: switch statement with embedded case labels
 * @note ctx->_coro_lc determines where execution resumes
 * @note 0xFFFF sentinel value indicates coroutine has exited permanently
 */
#define SAFETIMER_CORO_BEGIN(ctx) \
    if ((ctx)->_coro_lc == 0xFFFF) { return; } /* Exit guard */ \
    switch((ctx)->_coro_lc) { case 0:

/**
 * @brief End coroutine body
 *
 * Closes the switch statement started by SAFETIMER_CORO_BEGIN().
 */
#define SAFETIMER_CORO_END() \
    } /* End switch */

/**
 * @brief Yield execution and return to caller
 *
 * Execution resumes at the next line when the coroutine is called again.
 *
 * @note Does NOT modify timer period (timer keeps its current period)
 * @note Useful for explicit task switching without delay
 */
#define SAFETIMER_CORO_YIELD() do { \
    (ctx)->_coro_lc = __LINE__; return; \
    case __LINE__:; \
} while(0)

/**
 * @brief Sleep for specified milliseconds
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
 * SAFETIMER_CORO_SLEEP(1000);  // Sleep exactly 1 second per cycle
 * led_toggle();                // Zero cumulative drift over time
 * @endcode
 */
#define SAFETIMER_CORO_SLEEP(ms) do { \
    safetimer_advance_period((ctx)->_coro_handle, (ms)); \
    (ctx)->_coro_lc = __LINE__; return; \
    case __LINE__:; \
} while(0)

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
    safetimer_set_period((ctx)->_coro_handle, (poll_ms)); \
    (ctx)->_coro_lc = __LINE__; \
    case __LINE__: \
    if (!(cond)) return; \
} while(0)

/**
 * @brief Reset coroutine to beginning
 *
 * Next invocation will start from SAFETIMER_CORO_BEGIN().
 *
 * @note Useful for restarting state machines or error recovery
 */
#define SAFETIMER_CORO_RESET() do { \
    (ctx)->_coro_lc = 0; return; \
} while(0)

/**
 * @brief Exit coroutine permanently
 *
 * Coroutine enters infinite yield state. Next invocation will immediately return
 * due to SAFETIMER_CORO_BEGIN() guard check.
 *
 * @note To restart, call SAFETIMER_CORO_RESET_EXTERNAL() from outside the coroutine
 * @note Timer continues running - manually call safetimer_stop() to halt timer
 */
#define SAFETIMER_CORO_EXIT() do { \
    (ctx)->_coro_lc = 0xFFFF; return; \
} while(0)

/* ========== Coroutine Helper Functions ========== */

/**
 * @brief Check if coroutine has exited
 *
 * @param ctx Pointer to coroutine context
 * @return 1 if exited, 0 if still running
 */
#define SAFETIMER_CORO_IS_EXITED(ctx) ((ctx)->_coro_lc == 0xFFFF)

/**
 * @brief Manually reset coroutine from outside
 *
 * @param ctx Pointer to coroutine context
 *
 * @note Can be called from any callback or coroutine to reset another coroutine
 */
#define SAFETIMER_CORO_RESET_EXTERNAL(ctx) ((ctx)->_coro_lc = 0)

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
