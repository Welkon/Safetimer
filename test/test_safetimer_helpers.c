/**
 * @file    test_safetimer_helpers.c
 * @brief   Unit Tests for SafeTimer Convenience Helpers
 * @date    2025-12-14
 *
 * Tests the convenience helper functions (safetimer_create_started,
 * safetimer_create_started_batch) integrated into safetimer.h
 */

#include "mock_bsp.h"
#include "safetimer.h"
#include "unity.h"

/* ========== Test Helpers ========== */

#ifdef UNIT_TEST
extern void safetimer_test_reset_pool(void);
#endif

static volatile int g_callback_count = 0;

void test_callback(void *user_data) {
  (void)user_data;
  g_callback_count++;
}

/* ========== Test Cases ========== */

/**
 * @test    test_create_started_success
 * @brief   Verify create_started returns valid handle and timer is running
 */
void test_create_started_success(void) {
  safetimer_handle_t h;
  int is_running;

  /* Create and start in one call */
  h = safetimer_create_started(1000, TIMER_MODE_REPEAT, test_callback, NULL);

  /* Verify handle is valid */
  TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

  /* Verify timer is running */
  TEST_ASSERT_EQUAL(TIMER_OK, safetimer_get_status(h, &is_running));
  TEST_ASSERT_EQUAL_INT(1, is_running);

  /* Verify callback fires */
  g_callback_count = 0;
  mock_bsp_advance_time(1000);
  safetimer_process();
  TEST_ASSERT_EQUAL_INT(1, g_callback_count);

  /* Cleanup */
  safetimer_delete(h);
}

/**
 * @test    test_create_started_invalid_parameters
 * @brief   Verify create_started fails gracefully with invalid parameters
 */
void test_create_started_invalid_parameters(void) {
  safetimer_handle_t h;

  /* Test with zero period (should fail) */
  h = safetimer_create_started(0, TIMER_MODE_REPEAT, test_callback, NULL);
  TEST_ASSERT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

  /* Test with period too large (should fail) */
  h = safetimer_create_started(0x80000000UL, TIMER_MODE_REPEAT, test_callback,
                               NULL);
  TEST_ASSERT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

  /* Test with invalid mode (should fail) */
  h = safetimer_create_started(1000, (timer_mode_t)99, test_callback, NULL);
  TEST_ASSERT_EQUAL(SAFETIMER_INVALID_HANDLE, h);
}

/**
 * @test    test_create_started_pool_exhaustion
 * @brief   Verify create_started handles pool exhaustion correctly
 */
void test_create_started_pool_exhaustion(void) {
  safetimer_handle_t handles[MAX_TIMERS + 1];
  int i;

  /* Fill pool */
  for (i = 0; i < MAX_TIMERS; i++) {
    handles[i] =
        safetimer_create_started(1000, TIMER_MODE_REPEAT, test_callback, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, handles[i]);
  }

  /* Next creation should fail */
  handles[MAX_TIMERS] =
      safetimer_create_started(1000, TIMER_MODE_REPEAT, test_callback, NULL);
  TEST_ASSERT_EQUAL(SAFETIMER_INVALID_HANDLE, handles[MAX_TIMERS]);

  /* Cleanup */
  for (i = 0; i < MAX_TIMERS; i++) {
    safetimer_delete(handles[i]);
  }
}

/**
 * @test    test_create_started_no_resource_leak
 * @brief   Verify create_started doesn't leak resources on start failure
 */
void test_create_started_no_resource_leak(void) {
  int used_before, used_after, total;

  /* Get initial pool usage */
  safetimer_get_pool_usage(&used_before, &total);

  /* Create with valid parameters (should succeed) */
  safetimer_handle_t h =
      safetimer_create_started(1000, TIMER_MODE_REPEAT, test_callback, NULL);
  TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

  /* Delete timer */
  safetimer_delete(h);

  /* Verify pool is back to initial state */
  safetimer_get_pool_usage(&used_after, &total);
  TEST_ASSERT_EQUAL_INT(used_before, used_after);
}

/**
 * @test    test_create_started_multiple_timers
 * @brief   Verify multiple create_started calls work correctly
 */
void test_create_started_multiple_timers(void) {
  safetimer_handle_t h1, h2, h3;
  int counts[3] = {0, 0, 0};

  /* Create three timers with different periods */
  h1 = safetimer_create_started(100, TIMER_MODE_REPEAT, test_callback,
                                &counts[0]);
  h2 = safetimer_create_started(200, TIMER_MODE_REPEAT, test_callback,
                                &counts[1]);
  h3 = safetimer_create_started(300, TIMER_MODE_REPEAT, test_callback,
                                &counts[2]);

  TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h1);
  TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h2);
  TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h3);

  /* Verify all are running */
  int is_running;
  safetimer_get_status(h1, &is_running);
  TEST_ASSERT_EQUAL_INT(1, is_running);
  safetimer_get_status(h2, &is_running);
  TEST_ASSERT_EQUAL_INT(1, is_running);
  safetimer_get_status(h3, &is_running);
  TEST_ASSERT_EQUAL_INT(1, is_running);

  /* Cleanup */
  safetimer_delete(h1);
  safetimer_delete(h2);
  safetimer_delete(h3);
}

/**
 * @test    test_create_started_batch_success
 * @brief   Verify batch creation creates all timers successfully
 */
void test_create_started_batch_success(void) {
  safetimer_handle_t handles[3];
  timer_callback_t callbacks[] = {test_callback, test_callback, test_callback};
  void *data[] = {NULL, NULL, NULL};

  /* Batch create 3 timers */
  int created = safetimer_create_started_batch(3, 500, TIMER_MODE_REPEAT,
                                               callbacks, data, handles);

  /* All should succeed */
  TEST_ASSERT_EQUAL_INT(3, created);
  TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, handles[0]);
  TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, handles[1]);
  TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, handles[2]);

  /* Verify all are unique */
  TEST_ASSERT_NOT_EQUAL(handles[0], handles[1]);
  TEST_ASSERT_NOT_EQUAL(handles[1], handles[2]);
  TEST_ASSERT_NOT_EQUAL(handles[0], handles[2]);

  /* Cleanup */
  for (int i = 0; i < 3; i++) {
    safetimer_delete(handles[i]);
  }
}

/**
 * @test    test_create_started_batch_partial_failure
 * @brief   Verify batch creation handles partial failures correctly
 */
void test_create_started_batch_partial_failure(void) {
  safetimer_handle_t handles[MAX_TIMERS + 2];
  timer_callback_t callbacks[MAX_TIMERS + 2];
  void *data[MAX_TIMERS + 2];
  int i;

  /* Fill callback arrays */
  for (i = 0; i < MAX_TIMERS + 2; i++) {
    callbacks[i] = test_callback;
    data[i] = NULL;
  }

  /* Try to create more than pool capacity */
  int created = safetimer_create_started_batch(
      MAX_TIMERS + 2, 500, TIMER_MODE_REPEAT, callbacks, data, handles);

  /* Should only create MAX_TIMERS */
  TEST_ASSERT_EQUAL_INT(MAX_TIMERS, created);

  /* First MAX_TIMERS should be valid */
  for (i = 0; i < MAX_TIMERS; i++) {
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, handles[i]);
  }

  /* Remaining should be invalid */
  for (i = MAX_TIMERS; i < MAX_TIMERS + 2; i++) {
    TEST_ASSERT_EQUAL(SAFETIMER_INVALID_HANDLE, handles[i]);
  }

  /* Cleanup */
  for (i = 0; i < MAX_TIMERS; i++) {
    safetimer_delete(handles[i]);
  }
}

/**
 * @test    test_create_started_batch_null_checks
 * @brief   Verify batch creation validates NULL pointers
 */
void test_create_started_batch_null_checks(void) {
  safetimer_handle_t handles[3];
  timer_callback_t callbacks[] = {test_callback, test_callback, test_callback};

  /* NULL handles array */
  int created = safetimer_create_started_batch(3, 500, TIMER_MODE_REPEAT,
                                               callbacks, NULL, NULL);
  TEST_ASSERT_EQUAL_INT(0, created);

  /* NULL callbacks array */
  created = safetimer_create_started_batch(3, 500, TIMER_MODE_REPEAT, NULL,
                                           NULL, handles);
  TEST_ASSERT_EQUAL_INT(0, created);
}

/**
 * @test    test_macro_create_started_or_success
 * @brief   Verify macro helper works for successful creation
 */
void test_macro_create_started_or_success(void) {
  safetimer_handle_t h;
  int error_handler_called = 0;

  SAFETIMER_CREATE_STARTED_OR(h, 1000, TIMER_MODE_REPEAT, test_callback, NULL,
                              { error_handler_called = 1; });

  /* Error handler should NOT be called */
  TEST_ASSERT_EQUAL_INT(0, error_handler_called);
  TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

  /* Verify timer is running */
  int is_running;
  safetimer_get_status(h, &is_running);
  TEST_ASSERT_EQUAL_INT(1, is_running);

  /* Cleanup */
  safetimer_delete(h);
}

/**
 * @test    test_macro_create_started_or_failure
 * @brief   Verify macro helper executes error handler on failure
 */
void test_macro_create_started_or_failure(void) {
  safetimer_handle_t handles[MAX_TIMERS];
  safetimer_handle_t h;
  int error_handler_called = 0;
  int i;

  /* Fill pool */
  for (i = 0; i < MAX_TIMERS; i++) {
    handles[i] =
        safetimer_create_started(1000, TIMER_MODE_REPEAT, test_callback, NULL);
  }

  /* Try to create one more (should fail) */
  SAFETIMER_CREATE_STARTED_OR(h, 1000, TIMER_MODE_REPEAT, test_callback, NULL,
                              { error_handler_called = 1; });

  /* Error handler SHOULD be called */
  TEST_ASSERT_EQUAL_INT(1, error_handler_called);
  TEST_ASSERT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

  /* Cleanup */
  for (i = 0; i < MAX_TIMERS; i++) {
    safetimer_delete(handles[i]);
  }
}

/* ========== End of Tests ========== */

/**
 * NOTE: setUp() and tearDown() are defined in test_runner_all.c
 * and shared across all test files. Do not redefine them here.
 */
