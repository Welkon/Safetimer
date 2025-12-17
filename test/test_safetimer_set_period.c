/**
 * @file    test_safetimer_set_period.c
 * @brief   SafeTimer set_period() API Tests
 * @version 1.2.6
 * @date    2025-12-17
 *
 * Tests safetimer_set_period() API:
 * - Normal cases: update period of running/stopped timers
 * - Edge cases: boundary values, invalid handles
 * - Behavior: immediate restart vs delayed effect
 * - Phase-locking: impact on REPEAT timers
 */

#include "unity.h"
#include "safetimer.h"
#include "mock_bsp.h"

/* ========== Test Helpers ========== */

#ifdef UNIT_TEST
extern void safetimer_test_reset_pool(void);
#endif

static int g_callback_count = 0;
static void test_callback(void *user_data) {
    g_callback_count++;
}

void setUp(void) {
    mock_bsp_reset();
    safetimer_test_reset_pool();
    g_callback_count = 0;
}

void tearDown(void) {
    /* Cleanup */
}

/* ========== Normal Cases ========== */

/**
 * Test: Change period of stopped timer
 * Expected: Period updated, takes effect on next start()
 */
void test_set_period_stopped_timer(void) {
    safetimer_handle_t h;
    timer_error_t err;

    /* Create but don't start */
    h = safetimer_create(1000, TIMER_MODE_REPEAT, test_callback, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

    /* Change period while stopped */
    err = safetimer_set_period(h, 500);
    TEST_ASSERT_EQUAL(TIMER_OK, err);

    /* Start timer - should use new period (500ms) */
    mock_bsp_set_ticks(0);
    safetimer_start(h);

    /* Advance 500ms - should trigger */
    mock_bsp_set_ticks(500);
    safetimer_process();
    TEST_ASSERT_EQUAL(1, g_callback_count);

    /* Advance another 500ms - should trigger again */
    mock_bsp_set_ticks(1000);
    safetimer_process();
    TEST_ASSERT_EQUAL(2, g_callback_count);
}

/**
 * Test: Change period of running timer (immediate restart)
 * Expected: Timer restarts from current tick with new period
 */
void test_set_period_running_timer_restarts_immediately(void) {
    safetimer_handle_t h;
    timer_error_t err;

    /* Create and start timer (1000ms period) */
    mock_bsp_set_ticks(0);
    h = safetimer_create(1000, TIMER_MODE_ONE_SHOT, test_callback, NULL);
    safetimer_start(h);

    /* Advance 600ms (60% through cycle) */
    mock_bsp_set_ticks(600);

    /* Change period to 400ms - should restart from tick=600 */
    err = safetimer_set_period(h, 400);
    TEST_ASSERT_EQUAL(TIMER_OK, err);

    /* Advance to 900ms (600+300ms) - should NOT trigger yet */
    mock_bsp_set_ticks(900);
    safetimer_process();
    TEST_ASSERT_EQUAL(0, g_callback_count);

    /* Advance to 1000ms (600+400ms) - should trigger now */
    mock_bsp_set_ticks(1000);
    safetimer_process();
    TEST_ASSERT_EQUAL(1, g_callback_count);
}

/**
 * Test: Increase period of running timer
 * Expected: Next trigger delayed
 */
void test_set_period_increase(void) {
    safetimer_handle_t h;

    mock_bsp_set_ticks(0);
    h = safetimer_create(100, TIMER_MODE_ONE_SHOT, test_callback, NULL);
    safetimer_start(h);

    /* Immediately change to 500ms */
    safetimer_set_period(h, 500);

    /* Original 100ms - should NOT trigger */
    mock_bsp_set_ticks(100);
    safetimer_process();
    TEST_ASSERT_EQUAL(0, g_callback_count);

    /* New 500ms - should trigger */
    mock_bsp_set_ticks(500);
    safetimer_process();
    TEST_ASSERT_EQUAL(1, g_callback_count);
}

/**
 * Test: Decrease period of running timer (speedup)
 * Expected: Next trigger happens sooner
 */
void test_set_period_decrease(void) {
    safetimer_handle_t h;

    mock_bsp_set_ticks(0);
    h = safetimer_create(1000, TIMER_MODE_ONE_SHOT, test_callback, NULL);
    safetimer_start(h);

    /* Immediately change to 100ms */
    safetimer_set_period(h, 100);

    /* New 100ms - should trigger */
    mock_bsp_set_ticks(100);
    safetimer_process();
    TEST_ASSERT_EQUAL(1, g_callback_count);
}

/**
 * Test: Multiple period changes
 * Expected: Last change takes effect
 */
void test_set_period_multiple_changes(void) {
    safetimer_handle_t h;

    mock_bsp_set_ticks(0);
    h = safetimer_create(1000, TIMER_MODE_ONE_SHOT, test_callback, NULL);
    safetimer_start(h);

    /* Change 1: 1000 -> 200 */
    safetimer_set_period(h, 200);

    /* Change 2: 200 -> 50 */
    safetimer_set_period(h, 50);

    /* Should trigger at 50ms (last change) */
    mock_bsp_set_ticks(50);
    safetimer_process();
    TEST_ASSERT_EQUAL(1, g_callback_count);
}

/* ========== REPEAT Mode Tests ========== */

/**
 * Test: set_period() breaks phase-locking (documented behavior)
 * Expected: Timer restarts, losing original phase
 */
void test_set_period_breaks_phase_locking(void) {
    safetimer_handle_t h;

    /* Create REPEAT timer starting at tick=0 */
    mock_bsp_set_ticks(0);
    h = safetimer_create(100, TIMER_MODE_REPEAT, test_callback, NULL);
    safetimer_start(h);

    /* First trigger at 100ms */
    mock_bsp_set_ticks(100);
    safetimer_process();
    TEST_ASSERT_EQUAL(1, g_callback_count);

    /* Advance to 150ms, change period to 100ms */
    mock_bsp_set_ticks(150);
    safetimer_set_period(h, 100);

    /* Original phase would trigger at 200ms, but restart happens at 150ms */
    /* So new trigger at 150+100=250ms */
    mock_bsp_set_ticks(200);
    safetimer_process();
    TEST_ASSERT_EQUAL(1, g_callback_count);  /* No trigger */

    mock_bsp_set_ticks(250);
    safetimer_process();
    TEST_ASSERT_EQUAL(2, g_callback_count);  /* Trigger at new phase */
}

/* ========== Edge Cases ========== */

/**
 * Test: Invalid period (zero)
 * Expected: TIMER_ERR_INVALID
 */
void test_set_period_zero_period_fails(void) {
    safetimer_handle_t h;
    timer_error_t err;

    h = safetimer_create(1000, TIMER_MODE_ONE_SHOT, test_callback, NULL);
    err = safetimer_set_period(h, 0);

#if ENABLE_PARAM_CHECK
    TEST_ASSERT_EQUAL(TIMER_ERR_INVALID, err);
#else
    TEST_IGNORE_MESSAGE("ENABLE_PARAM_CHECK disabled");
#endif
}

/**
 * Test: Invalid period (exceeds 2^31-1)
 * Expected: TIMER_ERR_INVALID
 */
void test_set_period_exceeds_maximum_fails(void) {
    safetimer_handle_t h;
    timer_error_t err;

    h = safetimer_create(1000, TIMER_MODE_ONE_SHOT, test_callback, NULL);
    err = safetimer_set_period(h, 0x80000000UL);  /* 2^31 */

#if ENABLE_PARAM_CHECK
    TEST_ASSERT_EQUAL(TIMER_ERR_INVALID, err);
#else
    TEST_IGNORE_MESSAGE("ENABLE_PARAM_CHECK disabled");
#endif
}

/**
 * Test: Invalid handle (negative)
 * Expected: TIMER_ERR_INVALID
 */
void test_set_period_invalid_handle_fails(void) {
    timer_error_t err = safetimer_set_period(-1, 1000);

#if ENABLE_PARAM_CHECK
    TEST_ASSERT_EQUAL(TIMER_ERR_INVALID, err);
#else
    TEST_IGNORE_MESSAGE("ENABLE_PARAM_CHECK disabled");
#endif
}

/**
 * Test: Deleted timer handle
 * Expected: TIMER_ERR_INVALID
 */
void test_set_period_deleted_timer_fails(void) {
    safetimer_handle_t h;
    timer_error_t err;

    h = safetimer_create(1000, TIMER_MODE_ONE_SHOT, test_callback, NULL);
    safetimer_delete(h);

    err = safetimer_set_period(h, 500);

#if ENABLE_PARAM_CHECK
    TEST_ASSERT_EQUAL(TIMER_ERR_INVALID, err);
#else
    TEST_IGNORE_MESSAGE("ENABLE_PARAM_CHECK disabled");
#endif
}

/**
 * Test: Maximum valid period (2^31-1)
 * Expected: Success
 */
void test_set_period_maximum_valid_period(void) {
    safetimer_handle_t h;
    timer_error_t err;

    h = safetimer_create(1000, TIMER_MODE_ONE_SHOT, test_callback, NULL);
    err = safetimer_set_period(h, 0x7FFFFFFFUL);

    TEST_ASSERT_EQUAL(TIMER_OK, err);
}

/**
 * Test: Minimum valid period (1ms)
 * Expected: Success
 */
void test_set_period_minimum_valid_period(void) {
    safetimer_handle_t h;
    timer_error_t err;

    h = safetimer_create(1000, TIMER_MODE_ONE_SHOT, test_callback, NULL);
    err = safetimer_set_period(h, 1);

    TEST_ASSERT_EQUAL(TIMER_OK, err);

    /* Verify it works */
    mock_bsp_set_ticks(0);
    safetimer_start(h);
    mock_bsp_set_ticks(1);
    safetimer_process();
    TEST_ASSERT_EQUAL(1, g_callback_count);
}

/* ========== Test Runner ========== */

int main(void) {
    UNITY_BEGIN();

    /* Normal cases */
    RUN_TEST(test_set_period_stopped_timer);
    RUN_TEST(test_set_period_running_timer_restarts_immediately);
    RUN_TEST(test_set_period_increase);
    RUN_TEST(test_set_period_decrease);
    RUN_TEST(test_set_period_multiple_changes);

    /* REPEAT mode */
    RUN_TEST(test_set_period_breaks_phase_locking);

    /* Edge cases */
    RUN_TEST(test_set_period_zero_period_fails);
    RUN_TEST(test_set_period_exceeds_maximum_fails);
    RUN_TEST(test_set_period_invalid_handle_fails);
    RUN_TEST(test_set_period_deleted_timer_fails);
    RUN_TEST(test_set_period_maximum_valid_period);
    RUN_TEST(test_set_period_minimum_valid_period);

    return UNITY_END();
}
