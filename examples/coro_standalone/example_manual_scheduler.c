/**
 * @file    example_manual_scheduler.c
 * @brief   Manual Time-Slicing Scheduler for Standalone Coroutines
 * @version 1.4.0
 * @date    2026-01-04
 *
 * Demonstrates adding timing features to coro_base.h using manual scheduling
 * with interrupt-driven tick counters. This approach provides timing without
 * SafeTimer's overhead.
 *
 * @par Key Concepts
 * - Interrupt provides 1ms tick counter
 * - Coroutines use CORO_WAIT_UNTIL for timing delays
 * - Main loop schedules coroutines manually
 * - Zero RAM overhead compared to SafeTimer (no timer pool)
 */

#include "coro_base.h"
#include <stdio.h>
#include <stdint.h>

/* ========== Mock Hardware Timer ========== */

/**
 * @brief Global tick counter (incremented by interrupt)
 *
 * In production: Replace with actual timer interrupt handler
 * Example (STM32): void TIM2_IRQHandler(void) { g_ticks++; }
 */
volatile uint32_t g_ticks = 0;

/**
 * @brief Mock timer interrupt (simulated in this example)
 *
 * In production: This is your actual hardware timer ISR
 */
void timer_isr_mock(void) {
    g_ticks++;
}

/* ========== Mock Hardware Peripherals ========== */

static int g_led_state = 0;
static int g_sensor_value = 0;

void led_on(void)  { g_led_state = 1; printf("  [LED ON]\n"); }
void led_off(void) { g_led_state = 0; printf("  [LED OFF]\n"); }
void sensor_power_on(void)  { printf("  [Sensor powered on]\n"); }
int  sensor_read(void)      { return ++g_sensor_value; }

/* ========== Example 1: LED Blink Coroutine ========== */

typedef struct {
    CORO_CONTEXT;
    uint32_t start_time;
} led_blink_ctx_t;

void led_blink_task(led_blink_ctx_t *ctx) {
    CORO_BEGIN(ctx);

    while (1) {
        /* Turn on LED */
        led_on();

        /* Wait 200ms using CORO_WAIT_UNTIL */
        ctx->start_time = g_ticks;
        CORO_WAIT_UNTIL(g_ticks - ctx->start_time >= 200);

        /* Turn off LED */
        led_off();

        /* Wait 800ms */
        ctx->start_time = g_ticks;
        CORO_WAIT_UNTIL(g_ticks - ctx->start_time >= 800);
    }

    CORO_END();
}

/* ========== Example 2: Sensor Polling Coroutine ========== */

typedef struct {
    CORO_CONTEXT;
    uint32_t start_time;
    int sensor_value;
} sensor_poll_ctx_t;

void sensor_poll_task(sensor_poll_ctx_t *ctx) {
    CORO_BEGIN(ctx);

    while (1) {
        /* Power on sensor */
        sensor_power_on();

        /* Wait 100ms for warmup */
        ctx->start_time = g_ticks;
        CORO_WAIT_UNTIL(g_ticks - ctx->start_time >= 100);

        /* Read sensor */
        ctx->sensor_value = sensor_read();
        printf("  [Sensor read: %d]\n", ctx->sensor_value);

        /* Wait 1000ms before next reading */
        ctx->start_time = g_ticks;
        CORO_WAIT_UNTIL(g_ticks - ctx->start_time >= 1000);
    }

    CORO_END();
}

/* ========== Example 3: Periodic Report Coroutine ========== */

typedef struct {
    CORO_CONTEXT;
    uint32_t start_time;
    int report_count;
} report_ctx_t;

void report_task(report_ctx_t *ctx) {
    CORO_BEGIN(ctx);

    while (1) {
        /* Print periodic report */
        ctx->report_count++;
        printf("  [Report #%d] System running, ticks=%lu\n",
               ctx->report_count, (unsigned long)g_ticks);

        /* Wait 3000ms (3 seconds) */
        ctx->start_time = g_ticks;
        CORO_WAIT_UNTIL(g_ticks - ctx->start_time >= 3000);
    }

    CORO_END();
}

/* ========== Manual Scheduler ========== */

int main(void) {
    led_blink_ctx_t led_ctx = {0};
    sensor_poll_ctx_t sensor_ctx = {0};
    report_ctx_t report_ctx = {0};

    printf("=== Manual Time-Slicing Scheduler Demo ===\n");
    printf("Simulating interrupt-driven tick counter...\n\n");

    /* Simulate running for 10 seconds (10000 ticks) */
    while (g_ticks < 10000) {
        /* Simulate timer interrupt (1ms tick) */
        timer_isr_mock();

        /* Schedule all coroutines (cooperative multitasking) */
        led_blink_task(&led_ctx);
        sensor_poll_task(&sensor_ctx);
        report_task(&report_ctx);

        /* Optional: Add small delay to slow down simulation for readability */
        /* In production: This loop runs at full speed */
    }

    printf("\n=== Demo Complete (10 seconds simulated) ===\n");
    printf("Final stats:\n");
    printf("  - LED blinks: ~10 times (1 Hz)\n");
    printf("  - Sensor readings: ~9 times (every 1.1s)\n");
    printf("  - Reports: ~3 times (every 3s)\n");
    printf("  - Total ticks: %lu\n", (unsigned long)g_ticks);

    return 0;
}

/**
 * @par Production Implementation Notes
 *
 * 1. Replace timer_isr_mock() with actual interrupt handler:
 * @code
 * void TIM2_IRQHandler(void) {
 *     if (TIM2->SR & TIM_SR_UIF) {
 *         TIM2->SR = ~TIM_SR_UIF;  // Clear flag
 *         g_ticks++;
 *     }
 * }
 * @endcode
 *
 * 2. Configure hardware timer for 1ms period
 *
 * 3. Main loop runs continuously:
 * @code
 * int main(void) {
 *     // Initialize contexts
 *     led_blink_ctx_t led_ctx = {0};
 *     sensor_poll_ctx_t sensor_ctx = {0};
 *
 *     // Setup timer interrupt
 *     init_timer_1ms();
 *
 *     while (1) {
 *         // Schedule coroutines
 *         led_blink_task(&led_ctx);
 *         sensor_poll_task(&sensor_ctx);
 *
 *         // Optional: Sleep until next tick (power saving)
 *         // __WFI();
 *     }
 * }
 * @endcode
 *
 * 4. Comparison with SafeTimer:
 *
 * Manual Scheduling:
 *   - RAM: 0 bytes overhead (only coroutine contexts)
 *   - Code: More verbose (manual tick tracking)
 *   - Flexibility: Full control over scheduling
 *   - Best for: Ultra-low-memory systems (<1KB RAM)
 *
 * SafeTimer Scheduling:
 *   - RAM: 14 bytes per timer
 *   - Code: Clean (SAFETIMER_CORO_WAIT(100))
 *   - Flexibility: Standard periodic/one-shot patterns
 *   - Best for: General embedded applications
 */
