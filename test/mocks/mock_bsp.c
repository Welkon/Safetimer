/**
 * @file    mock_bsp.c
 * @brief   Mock BSP Implementation for PC-based Testing
 * @version 1.0.0
 * @date    2025-12-13
 */

#include "mock_bsp.h"
#include <stdio.h>
#include <stdlib.h>

/* ========== Mock State ========== */

static bsp_tick_t       s_mock_ticks = 0;
static int              s_critical_nesting = 0;
static int              s_validation_enabled = 1;
static mock_bsp_stats_t s_stats = {0};

/* ========== BSP Interface Implementation ========== */

bsp_tick_t bsp_get_ticks(void)
{
    s_stats.get_ticks_count++;
    return s_mock_ticks;
}

void bsp_enter_critical(void)
{
    s_stats.enter_critical_count++;
    s_critical_nesting++;

    if (s_validation_enabled && s_critical_nesting > 1)
    {
        fprintf(stderr, "ERROR: Nested critical section detected (depth=%d)\n",
                s_critical_nesting);
        fprintf(stderr, "       SafeTimer v1.0 does not support critical section nesting!\n");
        abort();
    }
}

void bsp_exit_critical(void)
{
    s_stats.exit_critical_count++;
    s_critical_nesting--;

    if (s_validation_enabled && s_critical_nesting < 0)
    {
        fprintf(stderr, "ERROR: bsp_exit_critical() called without matching bsp_enter_critical()\n");
        fprintf(stderr, "       Critical section balance violation!\n");
        abort();
    }
}

/* ========== Mock Control Functions ========== */

void mock_bsp_reset(void)
{
    s_mock_ticks = 0;
    s_critical_nesting = 0;
    s_validation_enabled = 1;
    s_stats.get_ticks_count = 0;
    s_stats.enter_critical_count = 0;
    s_stats.exit_critical_count = 0;
}

void mock_bsp_set_ticks(bsp_tick_t ticks)
{
    s_mock_ticks = ticks;
}

void mock_bsp_advance_time(bsp_tick_t ms)
{
    s_mock_ticks += ms;
}

bsp_tick_t mock_bsp_get_current_ticks(void)
{
    return s_mock_ticks;
}

int mock_bsp_get_critical_nesting(void)
{
    return s_critical_nesting;
}

void mock_bsp_enable_validation(int enable)
{
    s_validation_enabled = enable;
}

void mock_bsp_get_stats(mock_bsp_stats_t *stats)
{
    if (stats != NULL)
    {
        stats->get_ticks_count = s_stats.get_ticks_count;
        stats->enter_critical_count = s_stats.enter_critical_count;
        stats->exit_critical_count = s_stats.exit_critical_count;
    }
}

void mock_bsp_reset_stats(void)
{
    s_stats.get_ticks_count = 0;
    s_stats.enter_critical_count = 0;
    s_stats.exit_critical_count = 0;
}
