/**
 * @file    test_safetimer_stress.c
 * @brief   SafeTimer Stress Testing and Long-term Reliability Tests
 * @version 1.0.0
 * @date    2025-12-14
 *
 * Tests stress conditions and long-term reliability:
 * - 1000+ create/delete cycles without leaks
 * - All timers active simultaneously (pool exhaustion)
 * - Rapid process() calls (1000 consecutive invocations)
 * - Long-running timers (simulating days of uptime)
 * - Memory leak detection (pool allocation/deallocation)
 * - Timer accuracy under continuous load
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

/* ========== Test Callbacks ========== */

void stress_callback(void *user_data) {
    (void)user_data;
    g_callback_count++;
}

void counting_callback(void *user_data) {
    int *counter = (int *)user_data;
    if (counter != NULL) {
        (*counter)++;
    }
}

/* ========== Test Cases ========== */

/**
 * @test    test_stress_1000_create_delete_cycles
 * @brief   Verify no memory leaks or corruption after 1000 create/delete cycles
 * @epic    Epic C: Deterministic Reliability Proof
 * @story   Story C.3: Stress Testing
 * @ac      AC1: System handles 1000+ create/delete cycles without failure
 */
void test_stress_1000_create_delete_cycles(void) {
    const int CYCLES = 1000;
    int i;
    safetimer_handle_t h;
    int total_timers, used_timers;

    printf("\n[STRESS] Running %d create/delete cycles...\n", CYCLES);

    for (i = 0; i < CYCLES; i++) {
        /* Create timer */
        h = safetimer_create(100, TIMER_MODE_ONE_SHOT, stress_callback, NULL);
        TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);

        /* Start and stop timer */
        TEST_ASSERT_EQUAL_INT(TIMER_OK, safetimer_start(h));
        TEST_ASSERT_EQUAL_INT(TIMER_OK, safetimer_stop(h));

        /* Delete timer */
        TEST_ASSERT_EQUAL_INT(TIMER_OK, safetimer_delete(h));

        /* Progress indicator every 100 cycles */
        if ((i + 1) % 100 == 0) {
            printf("  Completed %d/%d cycles\n", i + 1, CYCLES);
        }
    }

    /* Verify pool is empty after all deletions */
    safetimer_get_pool_usage(&used_timers, &total_timers);
    TEST_ASSERT_EQUAL_INT(0, used_timers);

    printf("[STRESS] ✓ All %d cycles completed successfully\n", CYCLES);
}

/**
 * @test    test_stress_all_timers_active_simultaneously
 * @brief   Verify system handles all timer pool slots active at once
 * @epic    Epic C: Deterministic Reliability Proof
 * @story   Story C.3: Stress Testing
 * @ac      AC2: All timers can be active simultaneously without errors
 */
void test_stress_all_timers_active_simultaneously(void) {
    safetimer_handle_t handles[MAX_TIMERS];
    int i;
    int total_timers, used_timers;
    int callbacks_fired[MAX_TIMERS] = {0};

    printf("\n[STRESS] Creating %d timers simultaneously...\n", MAX_TIMERS);

    /* Create all timers */
    for (i = 0; i < MAX_TIMERS; i++) {
        /* Staggered periods to ensure different expiration times */
        uint32_t period = 100 + (i * 10);
        handles[i] = safetimer_create(period, TIMER_MODE_REPEAT,
                                      counting_callback, &callbacks_fired[i]);
        TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, handles[i]);
    }

    /* Verify pool is full */
    safetimer_get_pool_usage(&used_timers, &total_timers);
    TEST_ASSERT_EQUAL_INT(MAX_TIMERS, used_timers);

    /* Start all timers */
    for (i = 0; i < MAX_TIMERS; i++) {
        TEST_ASSERT_EQUAL_INT(TIMER_OK, safetimer_start(handles[i]));
    }

    /* Run for sufficient time to trigger all timers multiple times */
    for (i = 0; i < 200; i++) {
        mock_bsp_advance_time(10);  /* Advance 10ms */
        safetimer_process();
    }

    /* Verify all timers fired at least once */
    for (i = 0; i < MAX_TIMERS; i++) {
        TEST_ASSERT_GREATER_THAN_INT(0, callbacks_fired[i]);
    }

    /* Stop and delete all timers */
    for (i = 0; i < MAX_TIMERS; i++) {
        TEST_ASSERT_EQUAL_INT(TIMER_OK, safetimer_stop(handles[i]));
        TEST_ASSERT_EQUAL_INT(TIMER_OK, safetimer_delete(handles[i]));
    }

    /* Verify pool is empty */
    safetimer_get_pool_usage(&used_timers, &total_timers);
    TEST_ASSERT_EQUAL_INT(0, used_timers);

    printf("[STRESS] ✓ All %d timers handled simultaneously\n", MAX_TIMERS);
}

/**
 * @test    test_stress_rapid_process_calls
 * @brief   Verify system handles 1000 consecutive process() calls
 * @epic    Epic C: Deterministic Reliability Proof
 * @story   Story C.3: Stress Testing
 * @ac      AC3: Rapid process() calls do not cause instability
 */
void test_stress_rapid_process_calls(void) {
    const int RAPID_CALLS = 1000;
    int i;
    safetimer_handle_t h;

    printf("\n[STRESS] Calling safetimer_process() %d times rapidly...\n", RAPID_CALLS);

    /* Create and start a timer */
    h = safetimer_create(500, TIMER_MODE_REPEAT, stress_callback, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);
    TEST_ASSERT_EQUAL_INT(TIMER_OK, safetimer_start(h));

    g_callback_count = 0;

    /* Call process() 1000 times with small time increments */
    for (i = 0; i < RAPID_CALLS; i++) {
        mock_bsp_advance_time(1);  /* 1ms per call */
        safetimer_process();
    }

    /* Timer should have fired approximately 1000ms / 500ms = 2 times */
    TEST_ASSERT_INT_WITHIN(1, 2, g_callback_count);

    /* Cleanup */
    TEST_ASSERT_EQUAL_INT(TIMER_OK, safetimer_delete(h));

    printf("[STRESS] ✓ Handled %d rapid process() calls\n", RAPID_CALLS);
}

/**
 * @test    test_stress_long_running_timer_10_days
 * @brief   Verify timer accuracy after simulating 10 days of uptime
 * @epic    Epic C: Deterministic Reliability Proof
 * @story   Story C.3: Stress Testing
 * @ac      AC4: Timers remain accurate after long periods (no drift)
 */
void test_stress_long_running_timer_10_days(void) {
    safetimer_handle_t h;
    const uint32_t TEN_DAYS_MS = 10UL * 24UL * 60UL * 60UL * 1000UL;  /* 864,000,000 ms */
    const uint32_t TIMER_PERIOD_MS = 1000;  /* 1 second timer */
    uint32_t elapsed_ms;
    int expected_callbacks;
    const int SECONDS_IN_TEN_DAYS = 864000;  /* 10 days * 24h * 60m * 60s */
    const uint32_t STEP_MS = 1000;  /* Advance 1 second per iteration */

    printf("\n[STRESS] Simulating 10 days of uptime (%u seconds)...\n", SECONDS_IN_TEN_DAYS);

    /* Create 1-second repeat timer */
    g_callback_count = 0;
    h = safetimer_create(TIMER_PERIOD_MS, TIMER_MODE_REPEAT, stress_callback, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);
    TEST_ASSERT_EQUAL_INT(TIMER_OK, safetimer_start(h));

    /* Simulate 10 days in 1-second steps */
    printf("  Progress: ");
    for (elapsed_ms = 0; elapsed_ms < TEN_DAYS_MS; elapsed_ms += STEP_MS) {
        mock_bsp_advance_time(STEP_MS);
        safetimer_process();  /* Process after EACH time advance */

        /* Progress indicator every 1 hour */
        if (elapsed_ms % (3600000UL) == 0 && elapsed_ms > 0) {
            printf(".");
            fflush(stdout);
        }
    }
    printf(" Done!\n");

    /* Verify callback count */
    expected_callbacks = SECONDS_IN_TEN_DAYS;
    printf("  Expected callbacks: %d\n", expected_callbacks);
    printf("  Actual callbacks:   %d\n", g_callback_count);

    /* Allow ±10 callbacks tolerance (0.001% error) */
    TEST_ASSERT_INT_WITHIN(10, expected_callbacks, g_callback_count);

    /* Cleanup */
    TEST_ASSERT_EQUAL_INT(TIMER_OK, safetimer_delete(h));

    printf("[STRESS] ✓ Timer accuracy maintained over 10 days\n");
}

/**
 * @test    test_stress_rapid_create_delete_without_cleanup
 * @brief   Verify pool handles rapid create/delete without fragmentation
 * @epic    Epic C: Deterministic Reliability Proof
 * @story   Story C.3: Stress Testing
 * @ac      AC5: No pool fragmentation after rapid allocation/deallocation
 */
void test_stress_rapid_create_delete_without_cleanup(void) {
    const int ITERATIONS = 100;
    int i, j;
    safetimer_handle_t handles[MAX_TIMERS];
    int total_timers, used_timers;

    printf("\n[STRESS] Testing pool fragmentation resistance...\n");

    for (i = 0; i < ITERATIONS; i++) {
        /* Fill pool */
        for (j = 0; j < MAX_TIMERS; j++) {
            handles[j] = safetimer_create(100, TIMER_MODE_ONE_SHOT, NULL, NULL);
            TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, handles[j]);
        }

        /* Verify pool is full */
        safetimer_get_pool_usage(&used_timers, &total_timers);
        TEST_ASSERT_EQUAL_INT(MAX_TIMERS, used_timers);

        /* Delete in random order (middle-out pattern) */
        for (j = 0; j < MAX_TIMERS; j++) {
            int index = (j % 2 == 0) ? (j / 2) : (MAX_TIMERS - 1 - j / 2);
            TEST_ASSERT_EQUAL_INT(TIMER_OK, safetimer_delete(handles[index]));
        }

        /* Verify pool is empty */
        safetimer_get_pool_usage(&used_timers, &total_timers);
        TEST_ASSERT_EQUAL_INT(0, used_timers);

        if ((i + 1) % 20 == 0) {
            printf("  Completed %d/%d iterations\n", i + 1, ITERATIONS);
        }
    }

    printf("[STRESS] ✓ No pool fragmentation after %d iterations\n", ITERATIONS);
}

/**
 * @test    test_stress_memory_leak_detection
 * @brief   Verify no memory leaks through pool usage monitoring
 * @epic    Epic C: Deterministic Reliability Proof
 * @story   Story C.3: Stress Testing
 * @ac      AC6: Pool usage returns to zero after all operations
 */
void test_stress_memory_leak_detection(void) {
    const int ITERATIONS = 500;
    int i;
    int total_timers, used_timers;
    safetimer_handle_t h1, h2, h3;

    printf("\n[STRESS] Memory leak detection test (%d iterations)...\n", ITERATIONS);

    for (i = 0; i < ITERATIONS; i++) {
        /* Create 3 timers */
        h1 = safetimer_create(100, TIMER_MODE_ONE_SHOT, NULL, NULL);
        h2 = safetimer_create(200, TIMER_MODE_REPEAT, NULL, NULL);
        h3 = safetimer_create(300, TIMER_MODE_ONE_SHOT, NULL, NULL);

        TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h1);
        TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h2);
        TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h3);

        /* Start timers */
        safetimer_start(h1);
        safetimer_start(h2);
        safetimer_start(h3);

        /* Run for a while */
        mock_bsp_advance_time(50);
        safetimer_process();

        /* Stop and delete */
        safetimer_stop(h2);
        safetimer_delete(h1);
        safetimer_delete(h2);
        safetimer_delete(h3);

        /* Verify pool is clean */
        safetimer_get_pool_usage(&used_timers, &total_timers);
        TEST_ASSERT_EQUAL_INT(0, used_timers);

        if ((i + 1) % 100 == 0) {
            printf("  Completed %d/%d iterations - no leaks\n", i + 1, ITERATIONS);
        }
    }

    printf("[STRESS] ✓ No memory leaks detected\n");
}

/**
 * @test    test_stress_timer_32bit_wraparound_boundary
 * @brief   Verify correct behavior at 32-bit tick counter wraparound
 * @epic    Epic C: Deterministic Reliability Proof
 * @story   Story C.3: Stress Testing
 * @ac      AC7: Timer accuracy maintained across tick counter wraparound
 */
void test_stress_timer_32bit_wraparound_boundary(void) {
    safetimer_handle_t h;
    const uint32_t NEAR_MAX = 0xFFFFFFF0UL;  /* 16 ticks before wraparound */

    printf("\n[STRESS] Testing 32-bit wraparound boundary...\n");

    /* Set mock time near maximum */
    mock_bsp_set_ticks(NEAR_MAX);
    printf("  Starting at tick: 0x%08lX\n", (unsigned long)NEAR_MAX);

    /* Create timer that will expire after wraparound */
    g_callback_count = 0;
    h = safetimer_create(20, TIMER_MODE_ONE_SHOT, stress_callback, NULL);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h);
    TEST_ASSERT_EQUAL_INT(TIMER_OK, safetimer_start(h));

    /* Advance time past wraparound */
    mock_bsp_advance_time(20);  /* This will cause wraparound: 0xFFFFFFF0 + 20 = 0x00000004 */
    printf("  After advance: 0x%08lX\n", (unsigned long)mock_bsp_get_current_ticks());

    safetimer_process();

    /* Verify callback fired */
    TEST_ASSERT_EQUAL_INT(1, g_callback_count);

    /* Cleanup */
    safetimer_delete(h);

    printf("[STRESS] ✓ Wraparound handled correctly\n");
}

/**
 * @test    test_stress_multiple_timers_different_periods_long_run
 * @brief   Verify multiple timers with different periods maintain accuracy
 * @epic    Epic C: Deterministic Reliability Proof
 * @story   Story C.3: Stress Testing
 * @ac      AC8: Multiple timers remain synchronized over extended periods
 */
void test_stress_multiple_timers_different_periods_long_run(void) {
    safetimer_handle_t h_fast, h_medium, h_slow;
    int count_fast = 0, count_medium = 0, count_slow = 0;
    const uint32_t RUNTIME_MS = 60000;  /* 1 minute */
    uint32_t elapsed;

    printf("\n[STRESS] Multiple timers over 60 seconds...\n");

    /* Create timers with different periods */
    h_fast = safetimer_create(100, TIMER_MODE_REPEAT, counting_callback, &count_fast);
    h_medium = safetimer_create(500, TIMER_MODE_REPEAT, counting_callback, &count_medium);
    h_slow = safetimer_create(2000, TIMER_MODE_REPEAT, counting_callback, &count_slow);

    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h_fast);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h_medium);
    TEST_ASSERT_NOT_EQUAL(SAFETIMER_INVALID_HANDLE, h_slow);

    /* Start all timers */
    safetimer_start(h_fast);
    safetimer_start(h_medium);
    safetimer_start(h_slow);

    /* Run simulation */
    for (elapsed = 0; elapsed < RUNTIME_MS; elapsed += 10) {
        mock_bsp_advance_time(10);
        safetimer_process();
    }

    /* Verify callback counts */
    printf("  Fast timer (100ms):   Expected ~600, Got %d\n", count_fast);
    printf("  Medium timer (500ms): Expected ~120, Got %d\n", count_medium);
    printf("  Slow timer (2000ms):  Expected ~30, Got %d\n", count_slow);

    TEST_ASSERT_INT_WITHIN(2, 600, count_fast);    /* 60000/100 = 600 */
    TEST_ASSERT_INT_WITHIN(2, 120, count_medium);  /* 60000/500 = 120 */
    TEST_ASSERT_INT_WITHIN(1, 30, count_slow);     /* 60000/2000 = 30 */

    /* Cleanup */
    safetimer_delete(h_fast);
    safetimer_delete(h_medium);
    safetimer_delete(h_slow);

    printf("[STRESS] ✓ All timers maintained accuracy\n");
}

/* ========== End of Stress Tests ========== */
