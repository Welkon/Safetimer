/**
 * @file    safetimer.c
 * @brief   SafeTimer Core Implementation
 * @version 1.2.5
 * @date    2025-12-16
 * @author  SafeTimer Project
 * @license MIT
 *
 * Implementation follows:
 * - DEC-001: BSP Interface (3 functions)
 * - DEC-002: Data Structure (114 bytes RAM)
 * - DEC-003: API Naming (safetimer_ prefix)
 * - ADR-005: Overflow Handling (signed difference algorithm)
 * - Step 5: Implementation Patterns (code style, MISRA-C, etc.)
 */

/* ========== Include Files ========== */
#include <stddef.h>  /* For NULL definition */
#include "safetimer.h"
#include "safetimer_internal.h"

/* ========== Global Variables ========== */

/**
 * @brief Global timer pool (114 bytes for MAX_TIMERS=8)
 *
 * Initialized to zero by C standard (all timers inactive at startup).
 * Protected by BSP critical sections during modifications.
 */
static safetimer_pool_t g_timer_pool = {0};

/* ========== Static Function Prototypes ========== */

STATIC int32_t safetimer_tick_diff(bsp_tick_t lhs, bsp_tick_t rhs);
STATIC int validate_handle(safetimer_handle_t handle);
STATIC int find_free_slot(void);
STATIC void update_expire_time(int slot_index, bsp_tick_t current_tick);
STATIC void trigger_timer(int slot_index, bsp_tick_t current_tick,
                          timer_callback_t *callback_out, void **user_data_out);

/* ========== Internal Helper Functions ========== */

/**
 * @brief Calculate signed difference between two tick values (handles wraparound)
 *
 * This function correctly handles tick counter wraparound for both 16-bit and
 * 32-bit tick types. The subtraction MUST be performed in the native tick width
 * before sign extension to preserve wraparound semantics.
 *
 * Why this is critical:
 * - Wrong: (int32_t)(lhs - rhs)  // Subtraction in wrong width!
 * - Right: (int32_t)(signed short)(uint16_t)(lhs - rhs)  // Correct for 16-bit
 *
 * Example (16-bit wraparound):
 *   current_tick = 1, expire_time = 65535
 *   Unsigned subtraction: (1 - 65535) = 2 (in uint16_t)
 *   Sign extension: (signed short)2 = 2
 *   Result: Timer expired 2ms ago ✓
 *
 * @param lhs Left-hand side tick value
 * @param rhs Right-hand side tick value
 * @return Signed difference (lhs - rhs), correctly handling wraparound
 */
STATIC int32_t safetimer_tick_diff(bsp_tick_t lhs, bsp_tick_t rhs)
{
#if BSP_TICK_TYPE_16BIT
    /* For 16-bit ticks: subtract in uint16_t domain, then sign-extend via signed short */
    uint16_t diff_u16 = (uint16_t)(lhs - rhs);
    int16_t diff_s16 = (int16_t)diff_u16;
    return (int32_t)diff_s16;
#else
    /* For 32-bit ticks: direct signed cast handles wraparound correctly */
    return (int32_t)(lhs - rhs);
#endif
}

/* ========== Public API Implementation ========== */

/**
 * @brief Create a new timer
 *
 * Implementation details:
 * - Uses bitmap to find free slot (O(n) search)
 * - Critical section protects slot allocation
 * - Timer initialized but NOT started (user must call safetimer_start)
 */
safetimer_handle_t safetimer_create(
    uint32_t            period_ms,
    timer_mode_t        mode,
    timer_callback_t    callback,
    void               *user_data
)
{
    int slot_index;

#if ENABLE_PARAM_CHECK
    /* Validate parameters */
    if (period_ms == 0 || period_ms > 0x7FFFFFFFUL)
    {
        return SAFETIMER_INVALID_HANDLE;  /* Period must be 1 ~ 2^31-1 */
    }

    if (mode != TIMER_MODE_ONE_SHOT && mode != TIMER_MODE_REPEAT)
    {
        return SAFETIMER_INVALID_HANDLE;  /* Invalid mode */
    }
#endif

    /* Find free slot */
    bsp_enter_critical();
    slot_index = find_free_slot();

    if (slot_index < 0)
    {
        bsp_exit_critical();
        return SAFETIMER_INVALID_HANDLE;  /* Pool full */
    }

    /* Initialize timer slot */
    g_timer_pool.slots[slot_index].period = period_ms;
    g_timer_pool.slots[slot_index].mode = (uint8_t)mode;
    g_timer_pool.slots[slot_index].callback = callback;
    g_timer_pool.slots[slot_index].user_data = user_data;
    g_timer_pool.slots[slot_index].active = 0;  /* Not started yet */
    g_timer_pool.used_bitmap |= (1U << slot_index);

    bsp_exit_critical();

    return (safetimer_handle_t)slot_index;
}

/**
 * @brief Start a timer
 *
 * Implementation details:
 * - Sets expire_time = current_tick + period
 * - Activates timer (sets active=1)
 * - Critical section protects state modification
 */
timer_error_t safetimer_start(safetimer_handle_t handle)
{
#if ENABLE_PARAM_CHECK
    if (!validate_handle(handle))
    {
        return TIMER_ERR_INVALID;
    }
#endif

    bsp_tick_t start_tick;

    /* Read BSP tick before entering the SafeTimer critical section to avoid
     * nested interrupt masking inside bsp_get_ticks(). */
    start_tick = bsp_get_ticks();

    bsp_enter_critical();

    /* Update expiration time */
    update_expire_time((int)handle, start_tick);

    /* Mark as active */
    g_timer_pool.slots[handle].active = 1;

    bsp_exit_critical();

    return TIMER_OK;
}

/* ========== Optional Query/Diagnostic APIs ========== */
#if ENABLE_QUERY_API

/**
 * @brief Stop a timer
 *
 * Implementation details:
 * - Marks timer as inactive (active=0)
 * - Does NOT delete timer (slot remains allocated)
 * - Critical section protects state modification
 */
timer_error_t safetimer_stop(safetimer_handle_t handle)
{
#if ENABLE_PARAM_CHECK
    /* Range check */
    if (handle < 0 || handle >= MAX_TIMERS)
    {
        return TIMER_ERR_INVALID;
    }

    /* Check if slot is allocated */
    if ((g_timer_pool.used_bitmap & (1U << handle)) == 0)
    {
        return TIMER_ERR_NOT_FOUND;
    }
#endif

    bsp_enter_critical();
    g_timer_pool.slots[handle].active = 0;
    bsp_exit_critical();

    return TIMER_OK;
}

#endif /* ENABLE_QUERY_API */

/**
 * @brief Delete a timer
 *
 * Implementation details:
 * - Clears used_bitmap bit (releases slot)
 * - Stops timer if running
 * - Critical section protects bitmap modification
 */
timer_error_t safetimer_delete(safetimer_handle_t handle)
{
#if ENABLE_PARAM_CHECK
    if (handle < 0 || handle >= MAX_TIMERS)
    {
        return TIMER_ERR_INVALID;
    }

    /* Check if slot is allocated */
    if ((g_timer_pool.used_bitmap & (1U << handle)) == 0)
    {
        return TIMER_ERR_NOT_FOUND;
    }
#endif

    bsp_enter_critical();

    /* Stop timer */
    g_timer_pool.slots[handle].active = 0;

    /* Release slot */
    g_timer_pool.used_bitmap &= ~(1U << handle);

    bsp_exit_critical();

    return TIMER_OK;
}

/**
 * @brief Process all active timers
 *
 * Implementation details (ADR-005 CRITICAL):
 * - Uses signed difference comparison: (long)(current - expire) >= 0
 * - Handles 32-bit wraparound automatically (two's complement)
 * - O(n) algorithm: iterates all MAX_TIMERS slots
 * - Triggers callback outside critical section for safety
 * - Repeating timers automatically reset expire_time
 */
void safetimer_process(void)
{
    int i;
    bsp_tick_t current_tick;

    current_tick = bsp_get_ticks();

    for (i = 0; i < MAX_TIMERS; i++)
    {
        timer_callback_t callback = NULL;
        void *user_data = NULL;
        int should_invoke = 0;

        /*
         * Copy slot state under the BSP critical section (prevents races with
         * start/stop/delete) and only call user code after releasing the lock.
         */
        bsp_enter_critical();

        /* Skip inactive timers */
        if (!g_timer_pool.slots[i].active)
        {
            bsp_exit_critical();
            continue;
        }

        /*
         * ADR-005: Signed Difference Comparison Algorithm (updated for 16-bit/32-bit)
         *
         * Check: safetimer_tick_diff(current_tick, expire_time) >= 0
         *
         * This handles wraparound correctly for both tick widths:
         * - 32-bit: current=1000, expire=500 → diff=500 ≥ 0 ✓
         * - 32-bit wraparound: current=95, expire=4294967295 → diff=96 ≥ 0 ✓
         * - 16-bit wraparound: current=1, expire=65535 → diff=2 ≥ 0 ✓
         *
         * Why safetimer_tick_diff() is required:
         * - For 32-bit ticks: (int32_t)(current - expire) works correctly
         * - For 16-bit ticks: Must subtract in uint16_t then sign-extend to int32_t
         *   Direct cast (int32_t)(current - expire) would be wrong!
         */
        if (safetimer_tick_diff(current_tick, g_timer_pool.slots[i].expire_time) >= 0)
        {
            /* Timer expired - trigger it */
            trigger_timer(i, current_tick, &callback, &user_data);
            should_invoke = 1;
        }

        bsp_exit_critical();

        /* Execute callback OUTSIDE critical section */
        if (should_invoke && callback != NULL)
        {
            callback(user_data);
        }
    }
}

/* ========== Optional Query/Diagnostic APIs ========== */
#if ENABLE_QUERY_API

/**
 * @brief Get timer running status
 */
timer_error_t safetimer_get_status(
    safetimer_handle_t  handle,
    int                *is_running
)
{
#if ENABLE_PARAM_CHECK
    if (!validate_handle(handle))
    {
        return TIMER_ERR_INVALID;
    }

    if (is_running == NULL)
    {
        return TIMER_ERR_INVALID;
    }
#endif

    bsp_enter_critical();
    *is_running = (int)g_timer_pool.slots[handle].active;
    bsp_exit_critical();

    return TIMER_OK;
}

/**
 * @brief Get remaining time until expiration
 */
timer_error_t safetimer_get_remaining(
    safetimer_handle_t  handle,
    uint32_t           *remaining_ms
)
{
    bsp_tick_t current_tick;
    int32_t diff;  /* Use int32_t for correct wraparound handling (ADR-005) */

#if ENABLE_PARAM_CHECK
    if (!validate_handle(handle))
    {
        return TIMER_ERR_INVALID;
    }

    if (remaining_ms == NULL)
    {
        return TIMER_ERR_INVALID;
    }
#endif

    bsp_enter_critical();

    if (!g_timer_pool.slots[handle].active)
    {
        /* Stopped timer */
        *remaining_ms = 0;
        bsp_exit_critical();
        return TIMER_OK;
    }

    current_tick = bsp_get_ticks();
    diff = safetimer_tick_diff(g_timer_pool.slots[handle].expire_time, current_tick);

    if (diff < 0)
    {
        /* Already expired but not yet processed */
        *remaining_ms = 0;
    }
    else
    {
        *remaining_ms = (uint32_t)diff;
    }

    bsp_exit_critical();

    return TIMER_OK;
}

/**
 * @brief Get timer pool usage statistics
 */
timer_error_t safetimer_get_pool_usage(
    int  *used_count,
    int  *total_count
)
{
    int i;
    int count;

    count = 0;

    bsp_enter_critical();

    /* Count set bits in bitmap */
    for (i = 0; i < MAX_TIMERS; i++)
    {
        if (g_timer_pool.used_bitmap & (1U << i))
        {
            count++;
        }
    }

    bsp_exit_critical();

    if (used_count != NULL)
    {
        *used_count = count;
    }

    if (total_count != NULL)
    {
        *total_count = MAX_TIMERS;
    }

    return TIMER_OK;
}

#endif /* ENABLE_QUERY_API */

/* ========== Static Function Implementation ========== */

#ifdef UNIT_TEST
/**
 * @brief Reset timer pool (FOR TESTING ONLY)
 *
 * @warning This function is ONLY for unit testing
 * @warning DO NOT use in production code
 */
void safetimer_test_reset_pool(void)
{
    int i;
    for (i = 0; i < MAX_TIMERS; i++)
    {
        g_timer_pool.slots[i].period = 0;
        g_timer_pool.slots[i].expire_time = 0;
        g_timer_pool.slots[i].callback = NULL;
        g_timer_pool.slots[i].user_data = NULL;
        g_timer_pool.slots[i].mode = 0;
        g_timer_pool.slots[i].active = 0;
    }
    g_timer_pool.used_bitmap = 0;
    g_timer_pool.reserved = 0;
}
#endif

/**
 * @brief Validate timer handle
 *
 * @param handle Timer handle to validate
 *
 * @return 1 if valid and allocated, 0 otherwise
 *
 * @note Internal helper - assumes ENABLE_PARAM_CHECK=1
 */
STATIC int validate_handle(safetimer_handle_t handle)
{
    /* Range check */
    if (handle < 0 || handle >= MAX_TIMERS)
    {
        return 0;
    }

    /* Allocation check */
    if ((g_timer_pool.used_bitmap & (1U << handle)) == 0)
    {
        return 0;
    }

    return 1;
}

/**
 * @brief Find first free slot in timer pool
 *
 * @return Slot index (0 ~ MAX_TIMERS-1) if found, -1 if pool full
 *
 * @note Uses bitmap for O(n) search
 * @note Called inside critical section - keep fast!
 */
STATIC int find_free_slot(void)
{
    int i;

    for (i = 0; i < MAX_TIMERS; i++)
    {
        if ((g_timer_pool.used_bitmap & (1U << i)) == 0)
        {
            return i;  /* Found free slot */
        }
    }

    return -1;  /* Pool full */
}

/**
 * @brief Update expiration time for a timer slot
 *
 * @param slot_index Valid slot index (0 ~ MAX_TIMERS-1)
 *
 * @note Calculates: expire_time = current_tick + period
 * @note Handles 32-bit wraparound automatically
 * @note Called inside critical section
 */
STATIC void update_expire_time(int slot_index, bsp_tick_t current_tick)
{
    /*
     * Use the tick captured outside the SafeTimer critical section so BSP
     * implementations remain free to mask interrupts when returning ticks.
     *
     * Set expiration time (32-bit wraparound handled by ADR-005 algorithm)
     * Example: current=4294967290, period=100
     *   → expire = 4294967390 (wraps to 94)
     *   → safetimer_process() will correctly detect expiration
     */
    g_timer_pool.slots[slot_index].expire_time = current_tick + g_timer_pool.slots[slot_index].period;
}

/**
 * @brief Trigger timer callback and handle mode
 *
 * @param slot_index Valid slot index (0 ~ MAX_TIMERS-1)
 *
 * @note For ONE_SHOT: stops timer after callback
 * @note For REPEAT: advances expire_time by period (phase-locked, eliminates drift)
 * @note Calls user callback if not NULL
 * @note Called outside critical section to allow callback to run safely
 */
STATIC void trigger_timer(int slot_index, bsp_tick_t current_tick,
                          timer_callback_t *callback_out, void **user_data_out)
{
    /* Caller already holds the BSP critical section. */

    if (callback_out != NULL)
    {
        *callback_out = g_timer_pool.slots[slot_index].callback;
    }

    if (user_data_out != NULL)
    {
        *user_data_out = g_timer_pool.slots[slot_index].user_data;
    }

    if (g_timer_pool.slots[slot_index].mode == TIMER_MODE_ONE_SHOT)
    {
        /* ONE_SHOT: stop timer */
        g_timer_pool.slots[slot_index].active = 0;
    }
    else
    {
        /* REPEAT: advance until the next expiration is in the future */
#if SAFETIMER_ENABLE_CATCHUP
        /* Catch-up mode: fire callbacks for each missed interval */
        g_timer_pool.slots[slot_index].expire_time += g_timer_pool.slots[slot_index].period;
#else
        /* Skip mode (default): coalesce missed intervals */
        do
        {
            g_timer_pool.slots[slot_index].expire_time += g_timer_pool.slots[slot_index].period;
        }
        while (safetimer_tick_diff(current_tick,
                                   g_timer_pool.slots[slot_index].expire_time) >= 0);
#endif
    }
}
