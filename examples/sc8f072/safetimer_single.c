/**
 * @file    safetimer_single.c
 * @brief   SafeTimer - Single-File Implementation
 * @version 1.2.0-single
 * @date    2025-12-14
 */

#include "safetimer_single.h"

/* ========================================================================== */
/*                         GLOBAL STATE                                       */
/* ========================================================================== */

static safetimer_pool_t g_timer_pool = {0};

/* ========================================================================== */
/*                         INTERNAL HELPER FUNCTIONS                          */
/* ========================================================================== */

#if ENABLE_PARAM_CHECK
static int validate_handle(safetimer_handle_t handle) {
    uint8_t index;

    if (handle < 0 || handle >= MAX_TIMERS) {
        return 0;
    }

    index = (uint8_t)handle;
    if (!(g_timer_pool.used_bitmap & (uint8_t)(1u << index))) {
        return 0;
    }
    return 1;
}
#endif

static int find_free_slot(void) {
    uint8_t i;

    for (i = 0; i < (uint8_t)MAX_TIMERS; i++) {
        if (!(g_timer_pool.used_bitmap & (uint8_t)(1u << i))) {
            return (int)i;
        }
    }
    return -1;
}

static void update_expire_time(int slot_index, bsp_tick_t current_tick) {
    uint8_t index;

    index = (uint8_t)slot_index;
    g_timer_pool.slots[index].expire_time =
        current_tick + g_timer_pool.slots[index].period;
}

static void trigger_timer(int slot_index, bsp_tick_t current_tick,
                          timer_callback_t *callback_out, void **user_data_out) {
    uint8_t index;
    timer_slot_t *slot;

    index = (uint8_t)slot_index;
    slot = &g_timer_pool.slots[index];

    *callback_out = slot->callback;
    *user_data_out = slot->user_data;

    if (slot->mode == TIMER_MODE_ONE_SHOT) {
        slot->active = 0;
    } else {
        update_expire_time(slot_index, current_tick);
    }
}

/* ========================================================================== */
/*                         PUBLIC API IMPLEMENTATION                          */
/* ========================================================================== */

safetimer_handle_t safetimer_create(
    uint32_t period_ms,
    timer_mode_t mode,
    timer_callback_t callback,
    void *user_data
) {
    int slot;
    uint8_t index;
    uint8_t mask;
    timer_slot_t *timer;

#if ENABLE_PARAM_CHECK
    if (period_ms == 0 || period_ms > 0x7FFFFFFF) {
        return SAFETIMER_INVALID_HANDLE;
    }
    if (mode != TIMER_MODE_ONE_SHOT && mode != TIMER_MODE_REPEAT) {
        return SAFETIMER_INVALID_HANDLE;
    }
#endif

    bsp_enter_critical();

    slot = find_free_slot();
    if (slot < 0) {
        bsp_exit_critical();
        return SAFETIMER_INVALID_HANDLE;
    }

    index = (uint8_t)slot;
    mask = (uint8_t)(1u << index);
    g_timer_pool.used_bitmap = (uint8_t)(g_timer_pool.used_bitmap | mask);

    timer = &g_timer_pool.slots[index];
    timer->period = period_ms;
    timer->mode = (uint8_t)mode;
    timer->callback = callback;
    timer->user_data = user_data;
    timer->active = 0;
    timer->expire_time = 0;

    bsp_exit_critical();

    return slot;
}

safetimer_err_t safetimer_start(safetimer_handle_t handle) {
    uint8_t index;
    timer_slot_t *timer;
    bsp_tick_t current_tick;

#if ENABLE_PARAM_CHECK
    if (!validate_handle(handle)) {
        return SAFETIMER_ERR_INVALID_HANDLE;
    }
#endif

    bsp_enter_critical();

    index = (uint8_t)handle;
    timer = &g_timer_pool.slots[index];
    if (!timer->active) {
        current_tick = bsp_get_ticks();
        update_expire_time((int)index, current_tick);
        timer->active = 1;
    }

    bsp_exit_critical();

    return SAFETIMER_OK;
}

safetimer_err_t safetimer_stop(safetimer_handle_t handle) {
    uint8_t index;

#if ENABLE_PARAM_CHECK
    if (!validate_handle(handle)) {
        return SAFETIMER_ERR_INVALID_HANDLE;
    }
#endif

    index = (uint8_t)handle;
    bsp_enter_critical();
    g_timer_pool.slots[index].active = 0;
    bsp_exit_critical();

    return SAFETIMER_OK;
}

safetimer_err_t safetimer_delete(safetimer_handle_t handle) {
    uint8_t index;
    uint8_t mask;

#if ENABLE_PARAM_CHECK
    if (!validate_handle(handle)) {
        return SAFETIMER_ERR_INVALID_HANDLE;
    }
#endif

    bsp_enter_critical();

    index = (uint8_t)handle;
    mask = (uint8_t)(1u << index);
    g_timer_pool.slots[index].active = 0;
    g_timer_pool.used_bitmap = (uint8_t)(g_timer_pool.used_bitmap & (uint8_t)(~mask));

    bsp_exit_critical();

    return SAFETIMER_OK;
}

void safetimer_process(void) {
    bsp_tick_t current_tick;
    uint8_t i;
    timer_slot_t *timer;
    timer_callback_t callback;
    void *user_data;

    current_tick = bsp_get_ticks();

    for (i = 0; i < (uint8_t)MAX_TIMERS; i++) {
        if (!(g_timer_pool.used_bitmap & (uint8_t)(1u << i))) {
            continue;
        }

        timer = &g_timer_pool.slots[i];
        if (!timer->active) {
            continue;
        }

        if ((int32_t)(current_tick - timer->expire_time) >= 0) {
            bsp_enter_critical();
            trigger_timer((int)i, current_tick, &callback, &user_data);
            bsp_exit_critical();

            if (callback != NULL) {
                callback(user_data);
            }
        }
    }
}

safetimer_err_t safetimer_get_status(safetimer_handle_t handle, int *is_running) {
    uint8_t index;

#if ENABLE_PARAM_CHECK
    if (!validate_handle(handle) || is_running == NULL) {
        return SAFETIMER_ERR_INVALID_PARAM;
    }
#endif

    index = (uint8_t)handle;
    bsp_enter_critical();
    *is_running = g_timer_pool.slots[index].active;
    bsp_exit_critical();

    return SAFETIMER_OK;
}

safetimer_err_t safetimer_get_remaining(safetimer_handle_t handle, uint32_t *remaining_ms) {
    uint8_t index;
    timer_slot_t *timer;
    bsp_tick_t current_tick;
    int32_t diff;

#if ENABLE_PARAM_CHECK
    if (!validate_handle(handle) || remaining_ms == NULL) {
        return SAFETIMER_ERR_INVALID_PARAM;
    }
#endif

    bsp_enter_critical();

    index = (uint8_t)handle;
    timer = &g_timer_pool.slots[index];
    if (!timer->active) {
        bsp_exit_critical();
        return SAFETIMER_ERR_NOT_RUNNING;
    }

    current_tick = bsp_get_ticks();
    diff = (int32_t)(timer->expire_time - current_tick);

    *remaining_ms = (diff > 0) ? (uint32_t)diff : 0;

    bsp_exit_critical();

    return SAFETIMER_OK;
}

safetimer_err_t safetimer_get_pool_usage(int *used, int *total) {
    int count;
    uint8_t i;
    uint8_t mask;

#if ENABLE_PARAM_CHECK
    if (used == NULL || total == NULL) {
        return SAFETIMER_ERR_INVALID_PARAM;
    }
#endif

    count = 0;

    bsp_enter_critical();

    for (i = 0; i < (uint8_t)MAX_TIMERS; i++) {
        mask = (uint8_t)(1u << i);
        if (g_timer_pool.used_bitmap & mask) {
            count++;
        }
    }

    *used = count;
    *total = MAX_TIMERS;

    bsp_exit_critical();

    return SAFETIMER_OK;
}
