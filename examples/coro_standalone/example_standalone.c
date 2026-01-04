/**
 * @file    example_standalone.c
 * @brief   Standalone Coroutine Example (No SafeTimer Dependency)
 * @version 1.4.0
 * @date    2026-01-04
 *
 * Demonstrates using coro_base.h independently without SafeTimer.
 * This example shows pure stackless coroutines for state machines.
 */

#include "coro_base.h"
#include <stdio.h>

/* ========== Example 1: Simple Counter Coroutine ========== */

typedef struct {
    CORO_CONTEXT;  /* Only needs _coro_lc (2 bytes) */
    int counter;
} counter_coro_t;

void counter_coroutine(counter_coro_t *ctx) {
    CORO_BEGIN(ctx);

    ctx->counter = 0;
    while (ctx->counter < 5) {
        printf("Counter: %d\n", ctx->counter);
        ctx->counter++;
        CORO_YIELD();  /* Return here, resume next call */
    }

    printf("Counter finished!\n");
    CORO_EXIT();  /* Permanent exit */

    CORO_END();
}

/* ========== Example 2: State Machine Coroutine ========== */

typedef struct {
    CORO_CONTEXT;
    int state;
    int data;
} state_machine_t;

void state_machine_coroutine(state_machine_t *ctx) {
    CORO_BEGIN(ctx);

    /* State 1: Initialize */
    ctx->state = 1;
    ctx->data = 0;
    printf("State 1: Initializing...\n");
    CORO_YIELD();

    /* State 2: Processing */
    ctx->state = 2;
    ctx->data = 42;
    printf("State 2: Processing data=%d\n", ctx->data);
    CORO_YIELD();

    /* State 3: Finalize */
    ctx->state = 3;
    printf("State 3: Finalizing...\n");
    CORO_EXIT();

    CORO_END();
}

/* ========== Main Function ========== */

int main(void) {
    counter_coro_t counter_ctx = {0};
    state_machine_t sm_ctx = {0};

    printf("=== Standalone Coroutine Demo ===\n\n");

    /* Run counter coroutine */
    printf("--- Counter Coroutine ---\n");
    while (!CORO_IS_EXITED(&counter_ctx)) {
        counter_coroutine(&counter_ctx);
    }

    printf("\n--- State Machine Coroutine ---\n");
    /* Run state machine coroutine */
    while (!CORO_IS_EXITED(&sm_ctx)) {
        state_machine_coroutine(&sm_ctx);
    }

    printf("\n=== Demo Complete ===\n");
    return 0;
}
