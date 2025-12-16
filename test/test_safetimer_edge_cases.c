/**
 * @file    test_safetimer_edge_cases.c
 * @brief   SafeTimer Edge Case and Boundary Condition Tests
 * @version 1.0.0
 * @date    2025-12-13
 *
 * Tests edge cases:
 * - Maximum period (2^31-1 ms)
 * - Minimum period (1 ms)
 * - Stopped timer query remaining time
 * - NULL pointer parameters
 * - Invalid mode parameter
 * - Delete unallocated handle
 */

#include "unity.h"
#include "safetimer.h"
#include "mock_bsp.h"

/* ========== Test Helpers ========== */

#ifdef UNIT_TEST
extern void safetimer_test_reset_pool(void);
#endif

/* ========== Boundary Condition Tests ========== */

void test_maximum_period(void) {
    safetimer_handle_t h;
    uint32_t max_period = 0x7FFFFFFFUL;  /* 2^31 - 1 */

    h = safetimer_create(max_period, TIMER_MODE_ONE_SHOT, NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

    /* Should be valid */
    int is_running;
    timer_error_t err = safetimer_get_status(h, &is_running);
    TEST_ASSERT_EQUAL(TIMER_OK, err);
}

void test_period_exceeds_maximum_should_fail(void) {
    safetimer_handle_t h;
    uint32_t invalid_period = 0x80000000UL;  /* 2^31 */

    h = safetimer_create(invalid_period, TIMER_MODE_ONE_SHOT, NULL, NULL);
    TEST_ASSERT_EQUAL(SAFETIMER_INVALID_HANDLE, h);
}

void test_minimum_period(void) {
    safetimer_handle_t h;

    h = safetimer_create(1, TIMER_MODE_ONE_SHOT, NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

    safetimer_start(h);

    /* Advance 1ms */
    mock_bsp_advance_time(1);
    safetimer_process();

    /* Timer should have expired */
    int is_running;
    safetimer_get_status(h, &is_running);
    TEST_ASSERT_EQUAL_INT(0, is_running);
}

void test_stopped_timer_remaining_time_returns_zero(void) {
    safetimer_handle_t h;
    uint32_t remaining;

    h = safetimer_create(1000, TIMER_MODE_ONE_SHOT, NULL, NULL);
    /* Don't start */

    timer_error_t err = safetimer_get_remaining(h, &remaining);
    TEST_ASSERT_EQUAL(TIMER_OK, err);
    TEST_ASSERT_EQUAL_UINT32(0, remaining);
}

void test_expired_but_not_processed_returns_zero_remaining(void) {
    safetimer_handle_t h;
    uint32_t remaining;

    h = safetimer_create(1000, TIMER_MODE_ONE_SHOT, NULL, NULL);
    safetimer_start(h);

    /* Advance past expiration without processing */
    mock_bsp_advance_time(1500);

    /* Query remaining time */
    safetimer_get_remaining(h, &remaining);
    TEST_ASSERT_EQUAL_UINT32(0, remaining);
}

/* ========== NULL Pointer Tests ========== */

void test_get_status_with_null_output_pointer(void) {
    safetimer_handle_t h;
    timer_error_t err;

    h = safetimer_create(1000, TIMER_MODE_ONE_SHOT, NULL, NULL);

    err = safetimer_get_status(h, NULL);
    TEST_ASSERT_EQUAL(TIMER_ERR_INVALID, err);
}

void test_get_remaining_with_null_output_pointer(void) {
    safetimer_handle_t h;
    timer_error_t err;

    h = safetimer_create(1000, TIMER_MODE_ONE_SHOT, NULL, NULL);

    err = safetimer_get_remaining(h, NULL);
    TEST_ASSERT_EQUAL(TIMER_ERR_INVALID, err);
}

void test_get_pool_usage_with_null_pointers(void) {
    timer_error_t err;

    /* Both NULL should succeed */
    err = safetimer_get_pool_usage(NULL, NULL);
    TEST_ASSERT_EQUAL(TIMER_OK, err);

    /* One NULL should work */
    int used;
    err = safetimer_get_pool_usage(&used, NULL);
    TEST_ASSERT_EQUAL(TIMER_OK, err);

    int total;
    err = safetimer_get_pool_usage(NULL, &total);
    TEST_ASSERT_EQUAL(TIMER_OK, err);
}

/* ========== Invalid Parameter Tests ========== */

void test_create_with_invalid_mode(void) {
    safetimer_handle_t h;

    h = safetimer_create(1000, 99, NULL, NULL);  /* Invalid mode */
    TEST_ASSERT_EQUAL(SAFETIMER_INVALID_HANDLE, h);
}

void test_start_with_invalid_handle_negative(void) {
    timer_error_t err = safetimer_start(-1);
    TEST_ASSERT_EQUAL(TIMER_ERR_INVALID, err);
}

void test_start_with_invalid_handle_out_of_range(void) {
    timer_error_t err = safetimer_start(MAX_TIMERS);
    TEST_ASSERT_EQUAL(TIMER_ERR_INVALID, err);
}

void test_stop_unallocated_timer(void) {
    /* Try to stop a never-allocated handle */
    timer_error_t err = safetimer_stop(0);  /* Handle 0 not allocated */
    TEST_ASSERT_EQUAL(TIMER_ERR_NOT_FOUND, err);
}

void test_delete_unallocated_timer(void) {
    timer_error_t err = safetimer_delete(0);  /* Handle 0 not allocated */
    TEST_ASSERT_EQUAL(TIMER_ERR_NOT_FOUND, err);
}

/* ========== Rapid Operations Tests ========== */

void test_rapid_start_stop_cycles(void) {
    safetimer_handle_t h;

    h = safetimer_create(1000, TIMER_MODE_REPEAT, NULL, NULL);

    /* Rapidly start/stop */
    for (int i = 0; i < 10; i++) {
        safetimer_start(h);
        safetimer_stop(h);
    }

    /* Should still be valid */
    int is_running;
    timer_error_t err = safetimer_get_status(h, &is_running);
    TEST_ASSERT_EQUAL(TIMER_OK, err);
    TEST_ASSERT_EQUAL_INT(0, is_running);
}

void test_delete_while_running(void) {
    safetimer_handle_t h;

    h = safetimer_create(1000, TIMER_MODE_REPEAT, NULL, NULL);
    safetimer_start(h);

    /* Delete while running - should succeed */
    timer_error_t err = safetimer_delete(h);
    TEST_ASSERT_EQUAL(TIMER_OK, err);
}

void test_restart_running_timer_resets_expiration(void) {
    safetimer_handle_t h;
    uint32_t remaining1, remaining2;

    h = safetimer_create(2000, TIMER_MODE_ONE_SHOT, NULL, NULL);
    safetimer_start(h);

    /* Advance 500ms */
    mock_bsp_advance_time(500);
    safetimer_get_remaining(h, &remaining1);
    /* Should be ~1500ms remaining */

    /* Restart timer */
    safetimer_start(h);
    safetimer_get_remaining(h, &remaining2);

    /* Should be ~2000ms remaining (reset) */
    TEST_ASSERT_TRUE(remaining2 > remaining1);
    TEST_ASSERT_UINT32_WITHIN(100, 2000, remaining2);
}
