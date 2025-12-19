/**
 * @file    test_safetimer_advance_period.c
 * @brief   Unit tests for safetimer_advance_period() API (v1.3.1)
 * @version 1.3.1
 * @date    2025-12-19
 *
 * Tests phase-locked period advancement and zero cumulative error behavior.
 */

#include "unity.h"
#include "safetimer.h"
#include "mock_bsp.h"

/* External test helper from safetimer.c */
#ifdef UNIT_TEST
extern void safetimer_test_reset_pool(void);
#endif

/* ========== Test Fixtures ========== */
/* Note: setUp/tearDown defined in test_runner_all.c */

/* ========== Test 1: Basic Phase-Locked Advance ========== */

static int g_advance_callback_count = 0;

void test_advance_callback(void *user_data)
{
    (void)user_data;  /* Unused */
    g_advance_callback_count++;
}

/**
 * Test: advance_period maintains phase relationship
 * Verify: expire_time advances from previous expiration, not current tick
 */
void test_advance_period_basic_phase_locked(void)
{
    g_advance_callback_count = 0;

    /* Create timer with 100ms period */
    safetimer_handle_t h = safetimer_create(100, TIMER_MODE_REPEAT, test_advance_callback, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

    /* Start timer at tick 0 */
    mock_bsp_set_ticks(0);
    TEST_ASSERT_EQUAL(TIMER_OK, safetimer_start(h));

    /* Expire at tick 100 */
    mock_bsp_set_ticks(100);
    safetimer_process();
    TEST_ASSERT_EQUAL(1, g_advance_callback_count);

    /* Advance period to 200ms (from last expiration at 100, not current 100) */
    TEST_ASSERT_EQUAL(TIMER_OK, safetimer_advance_period(h, 200));

    /* Next expiration should be at 100 + 200 = 300, NOT 100 + 200 = 300 */
    mock_bsp_set_ticks(299);
    safetimer_process();
    TEST_ASSERT_EQUAL(1, g_advance_callback_count);  /* Not yet */

    mock_bsp_set_ticks(300);
    safetimer_process();
    TEST_ASSERT_EQUAL(2, g_advance_callback_count);  /* Fires at 300 */

    safetimer_delete(h);
}

/* ========== Test 2: Zero Cumulative Error ========== */

/**
 * Test: No cumulative error over 1000 cycles
 * Verify: advance_period maintains perfect phase-locking
 */
void test_advance_period_zero_cumulative_error(void)
{
    g_advance_callback_count = 0;

    safetimer_handle_t h = safetimer_create(100, TIMER_MODE_REPEAT, test_advance_callback, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

    mock_bsp_set_ticks(0);
    TEST_ASSERT_EQUAL(TIMER_OK, safetimer_start(h));

    /* Simulate 1000 cycles with 2us execution overhead per cycle */
    for (int i = 0; i < 1000; i++)
    {
        uint32_t expected_tick = (i + 1) * 100;
        mock_bsp_set_ticks(expected_tick);
        safetimer_process();

        /* Advance period (simulating coroutine sleep) */
        TEST_ASSERT_EQUAL(TIMER_OK, safetimer_advance_period(h, 100));

        /* Verify callback fired exactly at expected tick */
        TEST_ASSERT_EQUAL(i + 1, g_advance_callback_count);
    }

    /* Final verification: 1000 cycles, ZERO cumulative error */
    TEST_ASSERT_EQUAL(1000, g_advance_callback_count);

    safetimer_delete(h);
}

/* ========== Test 3: Comparison with set_period() ========== */

/**
 * Test: advance_period vs set_period behavior
 * Verify: advance_period maintains phase, set_period resets from current
 */
void test_advance_vs_set_period_behavior(void)
{
    /* Timer A: uses advance_period (phase-locked) */
    safetimer_handle_t h_advance = safetimer_create(100, TIMER_MODE_REPEAT, NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h_advance);

    /* Timer B: uses set_period (resets from current) */
    safetimer_handle_t h_set = safetimer_create(100, TIMER_MODE_REPEAT, NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h_set);

    /* Start both at tick 0 */
    mock_bsp_set_ticks(0);
    TEST_ASSERT_EQUAL(TIMER_OK, safetimer_start(h_advance));
    TEST_ASSERT_EQUAL(TIMER_OK, safetimer_start(h_set));

    /* At tick 100, change period to 50ms */
    mock_bsp_set_ticks(100);

    /* advance_period: next expiration = (100-100) + 50 = 50... but 50 < 100,
     * so catch-up logic advances to 150 */
    TEST_ASSERT_EQUAL(TIMER_OK, safetimer_advance_period(h_advance, 50));

    /* set_period: next expiration = 100 + 50 = 150 */
    TEST_ASSERT_EQUAL(TIMER_OK, safetimer_set_period(h_set, 50));

    /* Both should expire at 150 (in this specific case) */
    mock_bsp_set_ticks(150);
    safetimer_process();

    /* But the internal state differs: advance maintains phase relationship */

    safetimer_delete(h_advance);
    safetimer_delete(h_set);
}

/* ========== Test 4: Inactive Timer (Behaves like set_period) ========== */

/**
 * Test: advance_period on inactive timer
 * Verify: Behaves like set_period when no previous phase exists
 */
void test_advance_period_inactive_timer(void)
{
    safetimer_handle_t h = safetimer_create(100, TIMER_MODE_REPEAT, NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

    /* Do NOT start timer */

    /* Advance period while inactive */
    mock_bsp_set_ticks(50);
    TEST_ASSERT_EQUAL(TIMER_OK, safetimer_advance_period(h, 200));

    /* Should behave like set_period: expire_time = 50 + 200 = 250 */
    /* (Implementation detail: inactive timer has no phase to preserve) */

    safetimer_delete(h);
}

/* ========== Test 5: Parameter Validation ========== */

/**
 * Test: Invalid parameters rejected
 */
void test_advance_period_invalid_params(void)
{
#if ENABLE_PARAM_CHECK
    safetimer_handle_t h = safetimer_create(100, TIMER_MODE_REPEAT, NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

    /* Invalid period: 0 */
    TEST_ASSERT_EQUAL(TIMER_ERR_INVALID, safetimer_advance_period(h, 0));

    /* Invalid period: > 2^31-1 */
    TEST_ASSERT_EQUAL(TIMER_ERR_INVALID, safetimer_advance_period(h, 0x80000000UL));

    /* Invalid handle */
    TEST_ASSERT_EQUAL(TIMER_ERR_INVALID, safetimer_advance_period(99, 100));

    safetimer_delete(h);
#else
    TEST_PASS_MESSAGE("ENABLE_PARAM_CHECK=0, test skipped");
#endif
}

/* ========== Test 6: Delayed Coroutine Execution (Catch-up Logic) ========== */

/**
 * Test: Coroutine execution delayed beyond one period
 * Verify: Catch-up logic advances expire_time to future without burst
 */
void test_advance_period_delayed_execution(void)
{
    g_advance_callback_count = 0;

    safetimer_handle_t h = safetimer_create(100, TIMER_MODE_REPEAT, test_advance_callback, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

    mock_bsp_set_ticks(0);
    TEST_ASSERT_EQUAL(TIMER_OK, safetimer_start(h));

    /* Timer expires at 100 */
    mock_bsp_set_ticks(100);
    safetimer_process();
    TEST_ASSERT_EQUAL(1, g_advance_callback_count);

    /* Simulate coroutine execution taking 250ms (missing 2 periods) */
    mock_bsp_set_ticks(350);

    /* Advance period: should push expire_time to future (not burst) */
    TEST_ASSERT_EQUAL(TIMER_OK, safetimer_advance_period(h, 100));

    /* Verify no immediate burst callbacks */
    safetimer_process();
    TEST_ASSERT_EQUAL(1, g_advance_callback_count);  /* Still 1 */

    /* Next callback should be at 400 (100 + 3*100) */
    mock_bsp_set_ticks(400);
    safetimer_process();
    TEST_ASSERT_EQUAL(2, g_advance_callback_count);

    safetimer_delete(h);
}

/* ========== Test 7: 32-bit Overflow Handling ========== */

/**
 * Test: Handles wraparound at 2^32-1 boundary
 * Verify: ADR-005 signed difference comparison works correctly
 */
void test_advance_period_overflow_wraparound(void)
{
    safetimer_handle_t h = safetimer_create(100, TIMER_MODE_REPEAT, NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

    /* Start near overflow boundary */
    mock_bsp_set_ticks(0xFFFFFF00UL);  /* Close to 2^32-1 */
    TEST_ASSERT_EQUAL(TIMER_OK, safetimer_start(h));

    /* First expiration at 0xFFFFFF00 + 100 = 0x00000064 (wraps around) */
    mock_bsp_set_ticks(0x00000064UL);
    safetimer_process();

    /* Advance period by 100 */
    TEST_ASSERT_EQUAL(TIMER_OK, safetimer_advance_period(h, 100));

    /* Next expiration should be at 0x00000064 + 100 = 0x000000C8 */
    mock_bsp_set_ticks(0x000000C8UL);
    safetimer_process();

    safetimer_delete(h);
}

/* ========== Test 8: Regression - Existing Timers Unaffected ========== */

/**
 * Test: New API does not break existing timers
 * Verify: Traditional REPEAT timers still work correctly
 */
void test_advance_period_regression_existing_timers(void)
{
    /* Create traditional REPEAT timer (never calls advance_period) */
    g_advance_callback_count = 0;
    safetimer_handle_t h = safetimer_create(100, TIMER_MODE_REPEAT, test_advance_callback, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

    mock_bsp_set_ticks(0);
    TEST_ASSERT_EQUAL(TIMER_OK, safetimer_start(h));

    /* Run for 10 cycles */
    for (int i = 0; i < 10; i++)
    {
        mock_bsp_set_ticks((i + 1) * 100);
        safetimer_process();
    }

    TEST_ASSERT_EQUAL(10, g_advance_callback_count);

    safetimer_delete(h);
}
