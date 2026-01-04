/**
 * @file    example_coroutine.c
 * @brief   SafeTimer Coroutine Examples
 * @version 1.4.0
 *
 * Demonstrates four coroutine patterns:
 * 1. LED blink (basic SLEEP)
 * 2. UART timeout (WAIT_UNTIL + timeout detection)
 * 3. Semaphore synchronization (WAIT_SEM)
 * 4. Authentication handshake (Challenge-Response with exponential backoff)
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

/**
 * @brief Mock UART byte transmission (for micro-timing demonstration)
 * @param byte Byte to transmit
 * @note Production: Replace with actual UART send function
 */
void uart_send_byte_mock(uint8_t byte) {
    (void)byte; /* Mock: no actual transmission */
}

/**
 * @brief Mock microsecond blocking delay (for micro-timing demonstration)
 * @param us Delay duration in microseconds
 * @warning BLOCKS the scheduler - use only for hardware protocol timing
 * @note Production: Replace with _delay_us() or hardware-specific delay
 */
void bsp_delay_us(uint16_t us) {
    (void)us; /* Mock: no actual delay */
}

/* ========== Mock Crypto/BSP (for Authentication Example) ========== */

/**
 * @brief Mock random number generator (Linear Congruential Generator)
 * @warning NOT cryptographically secure - for demonstration only
 * @warning Fixed seed (1234) creates PREDICTABLE sequence - vulnerable to:
 *          - Boot Replay Attacks: Same challenge sequence after reboot
 *          - Precomputation Attacks: Attacker can predict all future challenges
 * @note Production systems MUST use:
 *          - Hardware RNG (TRNG)
 *          - Cryptographically Secure PRNG (CSPRNG) with entropy seeding
 *          - Unique seed source (ADC noise, uninitialized RAM, hardware ID)
 */
static uint32_t bsp_random(void) {
    static uint32_t seed = 1234;
    seed = seed * 1103515245U + 12345U;
    return seed;
}

/**
 * @brief Mock signature verification function
 * @param challenge The challenge nonce sent to peripheral
 * @param response_buf The response buffer from UART (expected: hex string)
 * @return 1 if signature valid, 0 otherwise
 * @warning This mock ALWAYS returns 1 for demonstration purposes
 * @note Production code must implement real cryptographic verification
 *       (e.g., HMAC-SHA256, RSA, or ECC signature validation)
 */
static int crypto_verify_signature(uint32_t challenge, const char *response_buf) {
    (void)challenge;     /* Unused in mock */
    (void)response_buf;  /* Unused in mock */
    return 1; /* Always succeed for demo - REPLACE IN PRODUCTION */
}

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
        SAFETIMER_CORO_WAIT(100);   /* 100ms on */

        led_off();
        SAFETIMER_CORO_WAIT(900);   /* 900ms off */
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
static volatile safetimer_sem_t auth_rx_sem;     /* Auth response semaphore */

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

        SAFETIMER_CORO_WAIT(50);  /* Brief processing delay */
    }

    SAFETIMER_CORO_END();
}

/* Producer (called from interrupt or another coroutine) */
void data_ready_isr(void)
{
    SAFETIMER_SEM_SIGNAL(data_ready_sem);
}

/**
 * @brief Mock UART RX ISR for authentication response
 * @note In production: Called from actual UART RX interrupt
 */
void uart_rx_isr_auth_mock(void)
{
    /* In production: Copy received data to g_uart_buffer here */
    SAFETIMER_SEM_SIGNAL(auth_rx_sem);
}

/* ========== Example 4: Authentication Handshake ========== */

/**
 * @brief Authentication context for Challenge-Response handshake
 *
 * State machine phases:
 * 1. Generate challenge nonce
 * 2. Wait for response (event-driven with timeout)
 * 3. Verify signature
 * 4. Success → Unlock | Failure → Exponential backoff/lockout
 */
typedef struct {
    SAFETIMER_CORO_CONTEXT;
    uint32_t challenge;           /**< Current challenge nonce */
    uint8_t retries;              /**< Failed authentication attempts */
    uint32_t lockout_duration;    /**< Dynamic lockout period (ms) */
} auth_ctx_t;

#define AUTH_MAX_RETRIES   3
#define AUTH_BASE_BACKOFF  1000  /* 1 second base backoff */
#define AUTH_TIMEOUT_MS    5000  /* 5 second response timeout */
#define AUTH_LOCKOUT_MS    10000 /* 10 second hard lockout after max retries */

/**
 * @brief Challenge-Response Authentication Handshake Coroutine
 *
 * Security features:
 * - Non-blocking multi-step flow (prevents system freeze)
 * - Timeout enforcement (5 seconds per attempt)
 * - Linear backoff (1s → 2s → 3s)
 * - Hard lockout after 3 failed attempts (10 seconds)
 * - Timeout failures count toward retry limit (anti-DoS)
 *
 * @warning This is a DEMONSTRATION. Production systems must:
 *          - Use cryptographically secure random (bsp_random → CSPRNG)
 *          - Implement real signature verification (HMAC/RSA/ECC)
 *          - Add replay attack prevention (nonce tracking)
 *          - Consider persistent lockout state (survives power cycle)
 */
void auth_handshake_task(void *user_data)
{
    auth_ctx_t *ctx = (auth_ctx_t *)user_data;

    SAFETIMER_CORO_BEGIN(ctx);

    while (1) {
        /* Phase 1: Generate & Transmit Challenge (Micro-Timing Demo) */
        ctx->challenge = bsp_random();

        /* MICRO-TIMING: Precise inter-byte spacing (blocking)
         * Transmit challenge as 4 bytes with 500µs inter-byte delay.
         * We BLOCK here because yielding would allow the scheduler to
         * interrupt the frame, potentially causing protocol violations. */
        {
            int i; /* C89: declare before statements */
            for (i = 0; i < 4; i++) {
                uint8_t byte = (uint8_t)(ctx->challenge >> (i * 8));
                uart_send_byte_mock(byte);
                bsp_delay_us(500); /* 500µs inter-byte delay - BLOCKS scheduler */
            }
        }

        /* MACRO-TIMING: Yield now that the hardware transaction is complete */
        SAFETIMER_CORO_YIELD();

        /* Phase 2: Wait for Response (Event-Driven with Timeout) */
        /* Wait for auth_rx_sem: 50ms poll × 100 polls = 5000ms timeout */
        SAFETIMER_CORO_WAIT_SEM(auth_rx_sem, 50, 100);

        if (auth_rx_sem == SAFETIMER_SEM_TIMEOUT) {
            /* Timeout - count as failed attempt (anti-DoS measure) */
            ctx->retries++;
            if (ctx->retries >= AUTH_MAX_RETRIES) {
                ctx->lockout_duration = AUTH_LOCKOUT_MS;
                ctx->retries = 0; /* Reset after lockout */
            } else {
                ctx->lockout_duration = AUTH_BASE_BACKOFF * ctx->retries;
            }
            SAFETIMER_CORO_WAIT(ctx->lockout_duration);
            continue; /* Restart challenge */
        }

        /* Phase 3: Verify Signature */
        uart_read_data(g_uart_buffer);
        if (crypto_verify_signature(ctx->challenge, g_uart_buffer)) {
            /* Success: Authenticated - unlock system */
            led_on(); /* Visual indicator */
            ctx->retries = 0; /* Reset failure counter */

            /* Maintain authenticated state for 10 seconds */
            SAFETIMER_CORO_WAIT(10000);
            led_off();
        } else {
            /* Phase 4: Failure - Linear Backoff */
            ctx->retries++;
            if (ctx->retries >= AUTH_MAX_RETRIES) {
                /* Hard lockout after max retries */
                ctx->lockout_duration = AUTH_LOCKOUT_MS;
                ctx->retries = 0; /* Reset counter after lockout */
            } else {
                /* Linear backoff: 1s, 2s, 3s */
                ctx->lockout_duration = AUTH_BASE_BACKOFF * ctx->retries;
            }
            SAFETIMER_CORO_WAIT(ctx->lockout_duration);
        }
    }

    SAFETIMER_CORO_END();
}

/* ========== Setup ========== */

void setup_coroutines(void)
{
    static led_ctx_t led_ctx = {0};
    static uart_ctx_t uart_ctx = {0};
    static consumer_ctx_t consumer_ctx = {0};
    static auth_ctx_t auth_ctx = {0};

    safetimer_handle_t h_led, h_uart, h_consumer, h_auth;

    /* Initialize semaphores */
    SAFETIMER_SEM_INIT(data_ready_sem);
    SAFETIMER_SEM_INIT(auth_rx_sem);

    /* Create timers (REPEAT mode required for coroutines) */
    h_led = safetimer_create(10, TIMER_MODE_REPEAT, led_blink_task, &led_ctx);
    h_uart = safetimer_create(10, TIMER_MODE_REPEAT, uart_task, &uart_ctx);
    h_consumer = safetimer_create(10, TIMER_MODE_REPEAT, consumer_task, &consumer_ctx);
    h_auth = safetimer_create(50, TIMER_MODE_REPEAT, auth_handshake_task, &auth_ctx);

    /* Start all timers
     * NOTE: h_uart and h_auth both consume UART data - do NOT run simultaneously!
     * Uncomment only one of the following lines based on your demo:
     * - safetimer_start(h_uart);   // For Example 2: UART Timeout demo
     * - safetimer_start(h_auth);   // For Example 4: Authentication demo
     */
    safetimer_start(h_led);
    /* safetimer_start(h_uart); */   /* Disabled to avoid conflict with h_auth */
    safetimer_start(h_consumer);
    safetimer_start(h_auth);          /* Authentication demo active */
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
