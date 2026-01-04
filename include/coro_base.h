/**
 * @file    coro_base.h
 * @brief   Standalone Stackless Coroutine Library (Zero Dependencies)
 * @version 1.4.0
 * @date    2026-01-04
 * @author  SafeTimer Project
 * @license MIT
 *
 * @par Overview
 * Pure C89 stackless coroutines using Duff's Device technique.
 * Can be used independently in ANY project without SafeTimer.
 *
 * @par Features
 * - Zero dependencies (no includes, pure macros)
 * - C89 compatible
 * - Minimal RAM (2 bytes per coroutine)
 * - Supports YIELD, RESET, EXIT operations
 *
 * @par Usage Example (Standalone)
 * @code
 * typedef struct {
 *     CORO_CONTEXT;  // Expands to: uint16_t _coro_lc
 *     int counter;
 * } my_coro_t;
 *
 * void my_coroutine(my_coro_t *ctx) {
 *     CORO_BEGIN(ctx);
 *
 *     ctx->counter = 0;
 *     while (ctx->counter < 10) {
 *         ctx->counter++;
 *         CORO_YIELD();  // Return here, resume next call
 *     }
 *
 *     CORO_END();
 * }
 * @endcode
 */

#ifndef CORO_BASE_H
#define CORO_BASE_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Coroutine Context ========== */

/**
 * @brief Coroutine context base structure
 *
 * Embed this at the START of your coroutine context struct.
 *
 * @note _coro_lc stores the __LINE__ number for resumption
 * @note 0xFFFF is sentinel value indicating permanent exit
 *
 * @par Memory Cost
 * - 2 bytes per coroutine
 *
 * @par C89 Compatibility
 * - C99+: Uses uint16_t from <stdint.h>
 * - C89: Uses unsigned short (guaranteed >= 16 bits by C standard)
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    /* C99+ with stdint.h support */
    #include <stdint.h>
    #define CORO_CONTEXT uint16_t _coro_lc
#else
    /* C89 fallback: unsigned short is guaranteed >= 16 bits */
    #define CORO_CONTEXT unsigned short _coro_lc
#endif

/* ========== Coroutine Control Flow ========== */

/**
 * @brief Begin coroutine body
 *
 * Must be paired with CORO_END().
 *
 * @param ctx Pointer to context struct (first member must be CORO_CONTEXT)
 *
 * @note Uses Duff's Device: switch statement with embedded case labels
 * @note ctx->_coro_lc determines where execution resumes
 * @note 0xFFFF sentinel value indicates coroutine has exited permanently
 */
#define CORO_BEGIN(ctx) \
    if ((ctx)->_coro_lc == 0xFFFF) { return; } \
    switch((ctx)->_coro_lc) { case 0:

/**
 * @brief End coroutine body
 *
 * Closes the switch statement started by CORO_BEGIN().
 */
#define CORO_END() \
    }

/**
 * @brief Yield execution and return to caller
 *
 * Execution resumes at the next line when the coroutine is called again.
 *
 * @note Does NOT modify any external state
 * @note Useful for explicit task switching
 */
#define CORO_YIELD() do { \
    (ctx)->_coro_lc = __LINE__; return; \
    case __LINE__:; \
} while(0)

/**
 * @brief Reset coroutine to beginning
 *
 * Next invocation will start from CORO_BEGIN().
 *
 * @note Useful for restarting state machines or error recovery
 */
#define CORO_RESET() do { \
    (ctx)->_coro_lc = 0; return; \
} while(0)

/**
 * @brief Exit coroutine permanently
 *
 * Coroutine enters infinite yield state. Next invocation will immediately return
 * due to CORO_BEGIN() guard check.
 *
 * @note To restart, call CORO_RESET_EXTERNAL() from outside the coroutine
 */
#define CORO_EXIT() do { \
    (ctx)->_coro_lc = 0xFFFF; return; \
} while(0)

/* ========== Helper Macros ========== */

/**
 * @brief Check if coroutine has exited
 *
 * @param ctx Pointer to coroutine context
 * @return 1 if exited, 0 if still running
 */
#define CORO_IS_EXITED(ctx) ((ctx)->_coro_lc == 0xFFFF)

/**
 * @brief Manually reset coroutine from outside
 *
 * @param ctx Pointer to coroutine context
 *
 * @note Can be called from any function to reset a coroutine
 */
#define CORO_RESET_EXTERNAL(ctx) ((ctx)->_coro_lc = 0)

/* ========== Usage Guidelines ========== */

/**
 * @par Restrictions
 * - Do NOT use switch/case inside coroutine body (conflicts with Duff's Device)
 * - Local variables are NOT preserved across yields (use context struct)
 * - Do NOT take address of labels (undefined behavior)
 *
 * @par Best Practices
 * - Keep coroutine functions simple and focused
 * - Store all state in context struct
 * - Use CORO_YIELD() for cooperative multitasking
 * - Use CORO_EXIT() when task is complete
 */

#ifdef __cplusplus
}
#endif

#endif /* CORO_BASE_H */
