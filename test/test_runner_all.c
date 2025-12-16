/**
 * @file    test_runner_all.c
 * @brief   Unified Test Runner for All SafeTimer Tests
 * @version 1.0.0
 * @date    2025-12-13
 */

#include "unity.h"
#include "safetimer.h"
#include "mock_bsp.h"

/* ========== Test Helpers ========== */

#ifdef UNIT_TEST
extern void safetimer_test_reset_pool(void);
#endif

/* Reset function from test_safetimer_callbacks.c */
extern void test_callbacks_reset_state(void);

/* ========== Test Fixtures ========== */

void setUp(void) {
    /* Reset mock BSP before each test */
    mock_bsp_reset();

    /* Reset timer pool before each test */
    safetimer_test_reset_pool();

    /* Reset callback test data */
    test_callbacks_reset_state();
}

void tearDown(void) {
    /* Verify critical section balance after each test */
    TEST_ASSERT_EQUAL_INT(0, mock_bsp_get_critical_nesting());
}

/* ========== Extern Test Functions ========== */

/* From test_safetimer_basic.c */
extern void test_create_valid_timer_one_shot(void);
extern void test_create_valid_timer_repeat(void);
extern void test_create_timer_zero_period_should_fail(void);
extern void test_create_timer_too_large_period_should_fail(void);
extern void test_create_multiple_timers_until_pool_full(void);
extern void test_start_timer_sets_active_state(void);
extern void test_stop_timer_clears_active_state(void);
extern void test_start_invalid_handle_should_fail(void);
extern void test_delete_timer_releases_slot(void);
extern void test_delete_timer_allows_slot_reuse(void);
extern void test_timer_does_not_expire_before_period(void);
extern void test_timer_expires_after_period(void);
extern void test_repeat_timer_continues_after_expiration(void);
extern void test_timer_handles_32bit_wraparound(void);

/* From test_safetimer_callbacks.c */
extern void test_oneshot_callback_called_once(void);
extern void test_repeat_callback_called_multiple_times(void);
extern void test_callback_receives_user_data(void);
extern void test_null_callback_does_not_crash(void);
extern void test_callback_executed_outside_critical_section(void);
extern void test_multiple_callbacks_in_one_process(void);
extern void test_callback_with_different_user_data(void);

/* From test_safetimer_edge_cases.c */
extern void test_maximum_period(void);
extern void test_period_exceeds_maximum_should_fail(void);
extern void test_minimum_period(void);
extern void test_stopped_timer_remaining_time_returns_zero(void);
extern void test_expired_but_not_processed_returns_zero_remaining(void);
extern void test_get_status_with_null_output_pointer(void);
extern void test_get_remaining_with_null_output_pointer(void);
extern void test_get_pool_usage_with_null_pointers(void);
extern void test_create_with_invalid_mode(void);
extern void test_start_with_invalid_handle_negative(void);
extern void test_start_with_invalid_handle_out_of_range(void);
extern void test_stop_unallocated_timer(void);
extern void test_delete_unallocated_timer(void);
extern void test_rapid_start_stop_cycles(void);
extern void test_delete_while_running(void);
extern void test_restart_running_timer_resets_expiration(void);

/* From test_safetimer_stress.c */
extern void test_stress_1000_create_delete_cycles(void);
extern void test_stress_all_timers_active_simultaneously(void);
extern void test_stress_rapid_process_calls(void);
extern void test_stress_long_running_timer_10_days(void);
extern void test_stress_rapid_create_delete_without_cleanup(void);
extern void test_stress_memory_leak_detection(void);
extern void test_stress_timer_32bit_wraparound_boundary(void);
extern void test_stress_multiple_timers_different_periods_long_run(void);

/* From test_safetimer_helpers.c */
extern void test_create_started_success(void);
extern void test_create_started_invalid_parameters(void);
extern void test_create_started_pool_exhaustion(void);
extern void test_create_started_no_resource_leak(void);
extern void test_create_started_multiple_timers(void);
extern void test_create_started_batch_success(void);
extern void test_create_started_batch_partial_failure(void);
extern void test_create_started_batch_null_checks(void);
extern void test_macro_create_started_or_success(void);
extern void test_macro_create_started_or_failure(void);

/* ========== Main Test Runner ========== */

int main(void) {
    UNITY_BEGIN();

    printf("\n========== Basic Tests ==========\n");
    RUN_TEST(test_create_valid_timer_one_shot);
    RUN_TEST(test_create_valid_timer_repeat);
    RUN_TEST(test_create_timer_zero_period_should_fail);
    RUN_TEST(test_create_timer_too_large_period_should_fail);
    RUN_TEST(test_create_multiple_timers_until_pool_full);
    RUN_TEST(test_start_timer_sets_active_state);
    RUN_TEST(test_stop_timer_clears_active_state);
    RUN_TEST(test_start_invalid_handle_should_fail);
    RUN_TEST(test_delete_timer_releases_slot);
    RUN_TEST(test_delete_timer_allows_slot_reuse);
    RUN_TEST(test_timer_does_not_expire_before_period);
    RUN_TEST(test_timer_expires_after_period);
    RUN_TEST(test_repeat_timer_continues_after_expiration);
    RUN_TEST(test_timer_handles_32bit_wraparound);

    printf("\n========== Callback Tests ==========\n");
    RUN_TEST(test_oneshot_callback_called_once);
    RUN_TEST(test_repeat_callback_called_multiple_times);
    RUN_TEST(test_callback_receives_user_data);
    RUN_TEST(test_null_callback_does_not_crash);
    RUN_TEST(test_callback_executed_outside_critical_section);
    RUN_TEST(test_multiple_callbacks_in_one_process);
    RUN_TEST(test_callback_with_different_user_data);

    printf("\n========== Edge Case Tests ==========\n");
    RUN_TEST(test_maximum_period);
    RUN_TEST(test_period_exceeds_maximum_should_fail);
    RUN_TEST(test_minimum_period);
    RUN_TEST(test_stopped_timer_remaining_time_returns_zero);
    RUN_TEST(test_expired_but_not_processed_returns_zero_remaining);
    RUN_TEST(test_get_status_with_null_output_pointer);
    RUN_TEST(test_get_remaining_with_null_output_pointer);
    RUN_TEST(test_get_pool_usage_with_null_pointers);
    RUN_TEST(test_create_with_invalid_mode);
    RUN_TEST(test_start_with_invalid_handle_negative);
    RUN_TEST(test_start_with_invalid_handle_out_of_range);
    RUN_TEST(test_stop_unallocated_timer);
    RUN_TEST(test_delete_unallocated_timer);
    RUN_TEST(test_rapid_start_stop_cycles);
    RUN_TEST(test_delete_while_running);
    RUN_TEST(test_restart_running_timer_resets_expiration);

    printf("\n========== Stress Tests ==========\n");
    RUN_TEST(test_stress_1000_create_delete_cycles);
    RUN_TEST(test_stress_all_timers_active_simultaneously);
    RUN_TEST(test_stress_rapid_process_calls);
    RUN_TEST(test_stress_long_running_timer_10_days);
    RUN_TEST(test_stress_rapid_create_delete_without_cleanup);
    RUN_TEST(test_stress_memory_leak_detection);
    RUN_TEST(test_stress_timer_32bit_wraparound_boundary);
    RUN_TEST(test_stress_multiple_timers_different_periods_long_run);

    printf("\n========== Helper API Tests ==========\n");
    RUN_TEST(test_create_started_success);
    RUN_TEST(test_create_started_invalid_parameters);
    RUN_TEST(test_create_started_pool_exhaustion);
    RUN_TEST(test_create_started_no_resource_leak);
    RUN_TEST(test_create_started_multiple_timers);
    RUN_TEST(test_create_started_batch_success);
    RUN_TEST(test_create_started_batch_partial_failure);
    RUN_TEST(test_create_started_batch_null_checks);
    RUN_TEST(test_macro_create_started_or_success);
    RUN_TEST(test_macro_create_started_or_failure);

    return UNITY_END();
}
