/**
 * @file    example_mixed_mode.c
 * @brief   Mixed Mode Example: Callbacks + StateSmith + Coroutines
 * @version 1.3.0
 *
 * Demonstrates three timer usage patterns coexisting in one application:
 * 1. Traditional callback (simple LED toggle)
 * 2. StateSmith-style state machine (button debounce FSM)
 * 3. Coroutine (sensor polling with WAIT_UNTIL)
 */

#include "safetimer.h"
#include "safetimer_coro.h"

/* ========== Mock Hardware ========== */

static volatile int g_led = 0;
static volatile int g_button_raw = 0;
static volatile int g_sensor_ready = 0;

void led_toggle(void) { g_led = !g_led; }
int button_is_pressed(void) { return g_button_raw; }
int sensor_is_ready(void) { return g_sensor_ready; }

/* ========== Pattern 1: Traditional Callback ========== */

void simple_led_callback(void *user_data)
{
    (void)user_data;  /* Unused */
    led_toggle();
}

/* ========== Pattern 2: StateSmith-Style State Machine ========== */

typedef enum {
    BTN_IDLE,
    BTN_PRESSED,
    BTN_RELEASED
} button_state_t;

typedef struct {
    button_state_t state;
    int press_count;
} button_sm_ctx_t;

void button_state_machine(void *user_data)
{
    button_sm_ctx_t *ctx = (button_sm_ctx_t *)user_data;

    switch (ctx->state) {
        case BTN_IDLE:
            if (button_is_pressed()) {
                ctx->state = BTN_PRESSED;
                ctx->press_count++;
            }
            break;

        case BTN_PRESSED:
            if (!button_is_pressed()) {
                ctx->state = BTN_RELEASED;
            }
            break;

        case BTN_RELEASED:
            /* Debounce complete */
            ctx->state = BTN_IDLE;
            break;
    }
}

/* ========== Pattern 3: Coroutine ========== */

typedef struct {
    SAFETIMER_CORO_CONTEXT;  /* Must be first */
    int sensor_data;
    int timeout_count;
} sensor_coro_ctx_t;

void sensor_coroutine(void *user_data)
{
    sensor_coro_ctx_t *ctx = (sensor_coro_ctx_t *)user_data;

    SAFETIMER_CORO_BEGIN(ctx);

    while (1) {
        /* Wait for sensor ready (poll every 50ms) */
        SAFETIMER_CORO_WAIT_UNTIL(sensor_is_ready(), 50);

        /* Read sensor */
        ctx->sensor_data = 42;  /* Mock read */

        /* Sleep 1 second before next reading */
        SAFETIMER_CORO_WAIT(1000);
    }

    SAFETIMER_CORO_END();
}

/* ========== Setup ========== */

void setup_mixed_mode(void)
{
    static button_sm_ctx_t button_ctx = {BTN_IDLE, 0};
    static sensor_coro_ctx_t sensor_ctx = {0};

    safetimer_handle_t h_led, h_button, h_sensor;

    /* Pattern 1: Callback (500ms LED toggle) */
    h_led = safetimer_create(500, TIMER_MODE_REPEAT, simple_led_callback, NULL);
    safetimer_start(h_led);

    /* Pattern 2: StateSmith FSM (10ms debounce polling) */
    h_button = safetimer_create(10, TIMER_MODE_REPEAT, button_state_machine, &button_ctx);
    safetimer_start(h_button);

    /* Pattern 3: Coroutine (sensor polling) */
    h_sensor = safetimer_create(10, TIMER_MODE_REPEAT, sensor_coroutine, &sensor_ctx);
    safetimer_start(h_sensor);
}

/* ========== Main ========== */

int main(void)
{
    setup_mixed_mode();

    while (1) {
        safetimer_process();
    }

    return 0;
}
