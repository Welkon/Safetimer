/**
 * @file    example_coroutine.c
 * @brief   SafeTimer Coroutine Examples
 * @version 1.3.0
 *
 * Demonstrates three coroutine patterns:
 * 1. LED blink (basic SLEEP)
 * 2. UART timeout (WAIT_UNTIL + timeout detection)
 * 3. Semaphore synchronization (WAIT_SEM)
 */

#include "safetimer.h"
#include "safetimer_coro.h"
#include "safetimer_sem.h"
#include "bsp.h"

/* ========== Mock Hardware Layer ========== */

static volatile int g_led_state = 0;
static volatile int g_uart_rx_ready = 0;
static char g_uart_buffer[32];

void led_on(void) { g_led_state = 1; }
void led_off(void) { g_led_state = 0; }
int uart_has_data(void) { return g_uart_rx_ready; }
void uart_read_data(char *buf) { buf[0] = 'A'; buf[1] = '\0'; g_uart_rx_ready = 0; }

/* ========== ADR-005 Wraparound-Safe Time Helpers ========== */

/**
 * @brief Calculate elapsed time in milliseconds (wraparound-safe)
 *
 * This helper mirrors SafeTimer's ADR-005 signed difference algorithm
 * to correctly handle tick counter wraparound.
 *
 * @param start_tick Starting tick value captured earlier
 * @return Elapsed time in milliseconds (signed, handles wraparound)
 *
 * @note For 16-bit ticks: wraps every ~65 seconds
 * @note For 32-bit ticks: wraps every ~49 days
 */
static int32_t elapsed_ms(bsp_tick_t start_tick)
{
#if BSP_TICK_TYPE_16BIT
    /* 16-bit wraparound: subtract in uint16_t domain, then sign-extend */
    uint16_t diff_u16 = (uint16_t)(bsp_get_ticks() - start_tick);
    return (int32_t)(int16_t)diff_u16;
#else
    /* 32-bit wraparound: direct signed cast handles correctly */
    return (int32_t)(bsp_get_ticks() - start_tick);
#endif
}

/* ========== Example 1: LED Blink ========== */

typedef struct {
    SAFETIMER_CORO_CONTEXT;
} led_ctx_t;

void led_blink_task(void *user_data)
{
    led_ctx_t *ctx = (led_ctx_t *)user_data;

    SAFETIMER_CORO_BEGIN(ctx);

    while (1) {
        led_on();
        SAFETIMER_CORO_SLEEP(100);   /* 100ms on */

        led_off();
        SAFETIMER_CORO_SLEEP(900);   /* 900ms off */
    }

    SAFETIMER_CORO_END();
}

/* ========== Example 2: UART with Timeout ========== */

typedef struct {
    SAFETIMER_CORO_CONTEXT;
    uint32_t start_time;
    int timeout_occurred;
} uart_ctx_t;

void uart_task(void *user_data)
{
    uart_ctx_t *ctx = (uart_ctx_t *)user_data;

    SAFETIMER_CORO_BEGIN(ctx);

    while (1) {
        ctx->start_time = bsp_get_ticks();
        ctx->timeout_occurred = 0;

        /* Wait for data or timeout (5 seconds)
         * Uses wraparound-safe elapsed_ms() helper (ADR-005 compliant) */
        SAFETIMER_CORO_WAIT_UNTIL(
            uart_has_data() || (elapsed_ms(ctx->start_time) > 5000),
            10  /* Poll every 10ms */
        );

        if (uart_has_data()) {
            uart_read_data(g_uart_buffer);
            /* Process data */
        } else {
            ctx->timeout_occurred = 1;
            /* Handle timeout */
        }

        SAFETIMER_CORO_YIELD();  /* Brief pause before next loop */
    }

    SAFETIMER_CORO_END();
}

/* ========== Example 3: Semaphore Producer-Consumer ========== */

static volatile safetimer_sem_t data_ready_sem;  /* Volatile for ISR access */

typedef struct {
    SAFETIMER_CORO_CONTEXT;
    int data;
} consumer_ctx_t;

void consumer_task(void *user_data)
{
    consumer_ctx_t *ctx = (consumer_ctx_t *)user_data;

    SAFETIMER_CORO_BEGIN(ctx);

    while (1) {
        /* Wait for data with timeout (1000ms = 10ms × 100)
         * Note: timeout_count must be ≤ 126 for int8_t semaphore */
        SAFETIMER_CORO_WAIT_SEM(data_ready_sem, 10, 100);

        if (data_ready_sem == SAFETIMER_SEM_TIMEOUT) {
            /* Timeout: no data received */
            ctx->data = -1;
        } else {
            /* Data available: process it */
            ctx->data = 42;  /* Mock read */
        }

        SAFETIMER_CORO_SLEEP(50);  /* Brief processing delay */
    }

    SAFETIMER_CORO_END();
}

/* Producer (called from interrupt or another coroutine) */
void data_ready_isr(void)
{
    SAFETIMER_SEM_SIGNAL(data_ready_sem);
}

/* ========== Setup ========== */

void setup_coroutines(void)
{
    static led_ctx_t led_ctx = {0};
    static uart_ctx_t uart_ctx = {0};
    static consumer_ctx_t consumer_ctx = {0};

    safetimer_handle_t h_led, h_uart, h_consumer;

    /* Initialize semaphore */
    SAFETIMER_SEM_INIT(data_ready_sem);

    /* Create timers (REPEAT mode required for coroutines) */
    h_led = safetimer_create(10, TIMER_MODE_REPEAT, led_blink_task, &led_ctx);
    h_uart = safetimer_create(10, TIMER_MODE_REPEAT, uart_task, &uart_ctx);
    h_consumer = safetimer_create(10, TIMER_MODE_REPEAT, consumer_task, &consumer_ctx);

    /* Store handles in contexts (for SLEEP/WAIT_UNTIL) */
    led_ctx._coro_handle = h_led;
    uart_ctx._coro_handle = h_uart;
    consumer_ctx._coro_handle = h_consumer;

    /* Start all timers */
    safetimer_start(h_led);
    safetimer_start(h_uart);
    safetimer_start(h_consumer);
}

/* ========== Main Loop ========== */

int main(void)
{
    bsp_init();
    setup_coroutines();

    while (1) {
        safetimer_process();

        /* Simulate data arrival every 3 seconds */
        static uint32_t last_signal = 0;
        if (bsp_get_ticks() - last_signal > 3000) {
            data_ready_isr();
            last_signal = bsp_get_ticks();
        }
    }

    return 0;
}
