/**
 * @file    test_safetimer_callbacks.c
 * @brief   SafeTimer Callback Execution Tests
 * @version 1.0.0
 * @date    2025-12-13
 *
 * Tests callback execution behavior:
 * - ONE_SHOT callback called exactly once
 * - REPEAT callback called multiple times
 * - Callback receives correct user_data
 * - NULL callback does not crash
 * - Callback executed outside critical section
 */

#include "unity.h"
#include "safetimer.h"
#include "mock_bsp.h"

/* ========== Test Helpers ========== */

#ifdef UNIT_TEST
extern void safetimer_test_reset_pool(void);
#endif

/* ========== Test Data ========== */

static volatile int g_callback_count = 0;
static volatile void *g_received_data = NULL;
static volatile int g_nesting_in_callback = -1;
static int g_received_values[2] = {0};
static int g_callback_index = 0;

/* ========== Reset Function for Test Runner ========== */

void test_callbacks_reset_state(void) {
    g_callback_count = 0;
    g_received_data = NULL;
    g_nesting_in_callback = -1;
    g_callback_index = 0;
    g_received_values[0] = 0;
    g_received_values[1] = 0;
}

/* ========== Test Callbacks ========== */

void count_callback(void *user_data) {
    g_callback_count++;
    g_received_data = user_data;
}

void simple_callback(void *user_data) {
    (void)user_data;
    g_callback_count++;
}

void check_critical_callback(void *user_data) {
    g_nesting_in_callback = mock_bsp_get_critical_nesting();
    (void)user_data;
}

void tracking_callback(void *user_data) {
    g_received_values[g_callback_index++] = *(int *)user_data;
}

/* ========== Test Cases ========== */

void test_oneshot_callback_called_once(void) {
    safetimer_handle_t h;

    h = safetimer_create(1000, TIMER_MODE_ONE_SHOT, simple_callback, NULL);
    safetimer_start(h);

    /* Advance time and process */
    mock_bsp_advance_time(1000);
    safetimer_process();

    TEST_ASSERT_EQUAL_INT(1, g_callback_count);

    /* Advance more time - callback should not fire again */
    mock_bsp_advance_time(1000);
    safetimer_process();

    TEST_ASSERT_EQUAL_INT(1, g_callback_count);  /* Still 1 */
}

void test_repeat_callback_called_multiple_times(void) {
    safetimer_handle_t h;

    h = safetimer_create(500, TIMER_MODE_REPEAT, simple_callback, NULL);
    safetimer_start(h);

    /* Callback should fire 3 times */
    mock_bsp_advance_time(500);
    safetimer_process();
    TEST_ASSERT_EQUAL_INT(1, g_callback_count);

    mock_bsp_advance_time(500);
    safetimer_process();
    TEST_ASSERT_EQUAL_INT(2, g_callback_count);

    mock_bsp_advance_time(500);
    safetimer_process();
    TEST_ASSERT_EQUAL_INT(3, g_callback_count);
}

void test_callback_receives_user_data(void) {
    safetimer_handle_t h;
    int my_data = 0x1234;

    h = safetimer_create(1000, TIMER_MODE_ONE_SHOT, count_callback, &my_data);
    safetimer_start(h);

    mock_bsp_advance_time(1000);
    safetimer_process();

    TEST_ASSERT_EQUAL_INT(1, g_callback_count);
    TEST_ASSERT_EQUAL_PTR(&my_data, g_received_data);
}

void test_null_callback_does_not_crash(void) {
    safetimer_handle_t h;

    /* Create timer with NULL callback */
    h = safetimer_create(1000, TIMER_MODE_ONE_SHOT, NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

    safetimer_start(h);

    /* Advance time - should not crash */
    mock_bsp_advance_time(1000);
    safetimer_process();  /* Should return without error */

    /* Timer should stop (ONE_SHOT mode) */
    int is_running;
    safetimer_get_status(h, &is_running);
    TEST_ASSERT_EQUAL_INT(0, is_running);
}

void test_callback_executed_outside_critical_section(void) {
    safetimer_handle_t h;

    h = safetimer_create(1000, TIMER_MODE_ONE_SHOT, check_critical_callback, NULL);
    safetimer_start(h);

    mock_bsp_advance_time(1000);
    safetimer_process();

    /* Callback should be executed outside critical section */
    TEST_ASSERT_EQUAL_INT(0, g_nesting_in_callback);
}

void test_multiple_callbacks_in_one_process(void) {
    safetimer_handle_t h1, h2;

    /* Create two timers with same expiration time */
    h1 = safetimer_create(1000, TIMER_MODE_ONE_SHOT, simple_callback, NULL);
    h2 = safetimer_create(1000, TIMER_MODE_ONE_SHOT, simple_callback, NULL);

    safetimer_start(h1);
    safetimer_start(h2);

    /* Both should fire in single process() call */
    mock_bsp_advance_time(1000);
    safetimer_process();

    TEST_ASSERT_EQUAL_INT(2, g_callback_count);
}

void test_callback_with_different_user_data(void) {
    safetimer_handle_t h1, h2;
    int data1 = 111;
    int data2 = 222;

    h1 = safetimer_create(1000, TIMER_MODE_ONE_SHOT, tracking_callback, &data1);
    h2 = safetimer_create(2000, TIMER_MODE_ONE_SHOT, tracking_callback, &data2);

    safetimer_start(h1);
    safetimer_start(h2);

    /* Fire first timer */
    mock_bsp_advance_time(1000);
    safetimer_process();
    TEST_ASSERT_EQUAL_INT(111, g_received_values[0]);

    /* Fire second timer */
    mock_bsp_advance_time(1000);
    safetimer_process();
    TEST_ASSERT_EQUAL_INT(222, g_received_values[1]);
}
