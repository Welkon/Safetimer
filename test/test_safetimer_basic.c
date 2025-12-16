/**
 * @file    test_safetimer_basic.c
 * @brief   Basic SafeTimer Functionality Tests
 * @version 1.0.0
 * @date    2025-12-13
 *
 * @note Compile with: gcc -std=c99 -DUNIT_TEST -I../include -I../test/mocks \
 *                     -I../test/unity ../src/safetimer.c ../test/mocks/mock_bsp.c \
 *                     ../test/unity/unity.c test_safetimer_basic.c -o test_runner
 */

#include "unity.h"
#include "safetimer.h"
#include "mock_bsp.h"

/* ========== Test Helper Functions ========== */

#ifdef UNIT_TEST
/* Declare internal test reset function */
extern void safetimer_test_reset_pool(void);
#endif

/* ========== Test Cases: Timer Creation ========== */

void test_create_valid_timer_one_shot(void)
{
    safetimer_handle_t handle;

    handle = safetimer_create(1000, TIMER_MODE_ONE_SHOT, NULL, NULL);

    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, handle);
    TEST_ASSERT_TRUE(handle >= 0 && handle < MAX_TIMERS);
}

void test_create_valid_timer_repeat(void)
{
    safetimer_handle_t handle;

    handle = safetimer_create(500, TIMER_MODE_REPEAT, NULL, NULL);

    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, handle);
    TEST_ASSERT_TRUE(handle >= 0 && handle < MAX_TIMERS);
}

void test_create_timer_zero_period_should_fail(void)
{
    safetimer_handle_t handle;

    handle = safetimer_create(0, TIMER_MODE_ONE_SHOT, NULL, NULL);

    TEST_ASSERT_EQUAL(SAFETIMER_INVALID_HANDLE, handle);
}

void test_create_timer_too_large_period_should_fail(void)
{
    safetimer_handle_t handle;

    /* Period > 2^31-1 should fail (ADR-005 constraint) */
    handle = safetimer_create(0x80000000UL, TIMER_MODE_ONE_SHOT, NULL, NULL);

    TEST_ASSERT_EQUAL(SAFETIMER_INVALID_HANDLE, handle);
}

void test_create_multiple_timers_until_pool_full(void)
{
    safetimer_handle_t handles[MAX_TIMERS + 1];
    int i;
    int used, total;

    /* Create MAX_TIMERS timers */
    for (i = 0; i < MAX_TIMERS; i++)
    {
        handles[i] = safetimer_create(100, TIMER_MODE_ONE_SHOT, NULL, NULL);
        TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, handles[i]);
    }

    /* Verify pool is full */
    safetimer_get_pool_usage(&used, &total);
    TEST_ASSERT_EQUAL_INT(MAX_TIMERS, used);
    TEST_ASSERT_EQUAL_INT(MAX_TIMERS, total);

    /* Next create should fail */
    handles[MAX_TIMERS] = safetimer_create(100, TIMER_MODE_ONE_SHOT, NULL, NULL);
    TEST_ASSERT_EQUAL(SAFETIMER_INVALID_HANDLE, handles[MAX_TIMERS]);
}

/* ========== Test Cases: Timer Start/Stop ========== */

void test_start_timer_sets_active_state(void)
{
    safetimer_handle_t handle;
    timer_error_t err;
    int is_running;

    handle = safetimer_create(1000, TIMER_MODE_ONE_SHOT, NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, handle);

    err = safetimer_start(handle);
    TEST_ASSERT_EQUAL(TIMER_OK, err);

    err = safetimer_get_status(handle, &is_running);
    TEST_ASSERT_EQUAL(TIMER_OK, err);
    TEST_ASSERT_EQUAL_INT(1, is_running);
}

void test_stop_timer_clears_active_state(void)
{
    safetimer_handle_t handle;
    timer_error_t err;
    int is_running;

    handle = safetimer_create(1000, TIMER_MODE_ONE_SHOT, NULL, NULL);
    safetimer_start(handle);

    err = safetimer_stop(handle);
    TEST_ASSERT_EQUAL(TIMER_OK, err);

    err = safetimer_get_status(handle, &is_running);
    TEST_ASSERT_EQUAL(TIMER_OK, err);
    TEST_ASSERT_EQUAL_INT(0, is_running);
}

void test_start_invalid_handle_should_fail(void)
{
    timer_error_t err;

    err = safetimer_start(SAFETIMER_INVALID_HANDLE);
    TEST_ASSERT_EQUAL(TIMER_ERR_INVALID, err);

    err = safetimer_start(MAX_TIMERS);  /* Out of range */
    TEST_ASSERT_EQUAL(TIMER_ERR_INVALID, err);
}

/* ========== Test Cases: Timer Deletion ========== */

void test_delete_timer_releases_slot(void)
{
    safetimer_handle_t handle;
    timer_error_t err;
    int used_before, used_after, total;

    safetimer_get_pool_usage(&used_before, &total);

    handle = safetimer_create(1000, TIMER_MODE_ONE_SHOT, NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, handle);

    err = safetimer_delete(handle);
    TEST_ASSERT_EQUAL(TIMER_OK, err);

    safetimer_get_pool_usage(&used_after, &total);
    TEST_ASSERT_EQUAL_INT(used_before, used_after);
}

void test_delete_timer_allows_slot_reuse(void)
{
    safetimer_handle_t handle1, handle2;

    handle1 = safetimer_create(1000, TIMER_MODE_ONE_SHOT, NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, handle1);

    safetimer_delete(handle1);

    /* Same slot should be available */
    handle2 = safetimer_create(2000, TIMER_MODE_REPEAT, NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, handle2);
    TEST_ASSERT_EQUAL(handle1, handle2);
}

/* ========== Test Cases: Timer Processing (No Callback) ========== */

void test_timer_does_not_expire_before_period(void)
{
    safetimer_handle_t handle;
    uint32_t remaining;

    handle = safetimer_create(1000, TIMER_MODE_ONE_SHOT, NULL, NULL);
    safetimer_start(handle);

    mock_bsp_advance_time(500);  /* Advance 500ms */
    safetimer_process();

    safetimer_get_remaining(handle, &remaining);
    TEST_ASSERT_TRUE(remaining > 0);  /* Should still be running */
}

void test_timer_expires_after_period(void)
{
    safetimer_handle_t handle;
    int is_running;

    handle = safetimer_create(1000, TIMER_MODE_ONE_SHOT, NULL, NULL);
    safetimer_start(handle);

    mock_bsp_advance_time(1000);  /* Advance exactly 1000ms */
    safetimer_process();

    safetimer_get_status(handle, &is_running);
    TEST_ASSERT_EQUAL_INT(0, is_running);  /* ONE_SHOT should stop */
}

void test_repeat_timer_continues_after_expiration(void)
{
    safetimer_handle_t handle;
    int is_running;

    handle = safetimer_create(500, TIMER_MODE_REPEAT, NULL, NULL);
    safetimer_start(handle);

    mock_bsp_advance_time(500);
    safetimer_process();

    safetimer_get_status(handle, &is_running);
    TEST_ASSERT_EQUAL_INT(1, is_running);  /* REPEAT should continue */
}

/* ========== Test Cases: Time Overflow (ADR-005) ========== */

void test_timer_handles_32bit_wraparound(void)
{
    safetimer_handle_t handle;
    int is_running;

    /* Set time near wraparound */
    mock_bsp_set_ticks(0xFFFFFFF0UL);  /* 16 ticks before wraparound */

    handle = safetimer_create(100, TIMER_MODE_ONE_SHOT, NULL, NULL);
    safetimer_start(handle);  /* Expire time = 0xFFFFFFF0 + 100 = 0x00000054 (wraps) */

    /* Advance past wraparound */
    mock_bsp_set_ticks(0x00000064UL);  /* 100 ticks after wraparound */
    safetimer_process();

    safetimer_get_status(handle, &is_running);
    TEST_ASSERT_EQUAL_INT(0, is_running);  /* Timer should have expired */
}
