/**
 * @file    example_helpers.c
 * @brief   SafeTimer Helpers API Usage Examples
 * @date    2025-12-14
 *
 * This example demonstrates the convenience helpers (safetimer_create_started,
 * safetimer_create_started_batch, SAFETIMER_CREATE_STARTED_OR) in safetimer.h.
 */

#include "safetimer.h"
#include <stdio.h>

/* ========== Mock BSP for Example ========== */

static volatile bsp_tick_t g_system_ticks = 0;

bsp_tick_t bsp_get_ticks(void) { return g_system_ticks; }

void bsp_enter_critical(void) { /* Disable interrupts */ }

void bsp_exit_critical(void) { /* Enable interrupts */ }

void mock_advance_time(uint32_t ms) { g_system_ticks += ms; }

/* ========== Callback Functions ========== */

void led1_blink(void *user_data) {
  (void)user_data;
  printf("[%5u ms] LED1 blink (500ms period)\n", (unsigned)g_system_ticks);
}

void led2_blink(void *user_data) {
  (void)user_data;
  printf("[%5u ms] LED2 blink (1000ms period)\n", (unsigned)g_system_ticks);
}

void led3_blink(void *user_data) {
  (void)user_data;
  printf("[%5u ms] LED3 blink (2000ms period)\n", (unsigned)g_system_ticks);
}

void delayed_task(void *user_data) {
  (void)user_data;
  printf("[%5u ms] Delayed task executed!\n", (unsigned)g_system_ticks);
}

/* ========== Example 1: Core API (Explicit Control) ========== */

void example_core_api(void) {
  printf("\n========== Example 1: Core API (Explicit Control) ==========\n");
  printf("Use case: Cascaded timers (delayed start)\n\n");

  /* Create timers but don't start yet */
  safetimer_handle_t stage1 =
      safetimer_create(1000, TIMER_MODE_ONE_SHOT, delayed_task, NULL);
  safetimer_handle_t stage2 =
      safetimer_create(2000, TIMER_MODE_ONE_SHOT, delayed_task, NULL);

  if (stage1 == SAFETIMER_INVALID_HANDLE ||
      stage2 == SAFETIMER_INVALID_HANDLE) {
    printf("ERROR: Failed to create timers\n");
    return;
  }

  printf("Timers created but NOT started yet\n");

  /* Start stage1 immediately */
  printf("Starting stage1 timer...\n");
  safetimer_start(stage1);

  /* Simulate time passing */
  for (int i = 0; i < 50; i++) {
    mock_advance_time(100);
    safetimer_process();

    /* After stage1 fires, start stage2 */
    if (i == 10) {
      printf("Stage1 completed, starting stage2...\n");
      safetimer_start(stage2);
    }
  }

  /* Cleanup */
  safetimer_delete(stage1);
  safetimer_delete(stage2);
}

/* ========== Example 2: Helper API (Immediate Start) ========== */

void example_helper_api(void) {
  printf("\n========== Example 2: Helper API (Immediate Start) ==========\n");
  printf("Use case: Multiple periodic tasks started immediately\n\n");

  /* Create and start timers in one call */
  safetimer_handle_t led1 =
      safetimer_create_started(500, TIMER_MODE_REPEAT, led1_blink, NULL);
  safetimer_handle_t led2 =
      safetimer_create_started(1000, TIMER_MODE_REPEAT, led2_blink, NULL);
  safetimer_handle_t led3 =
      safetimer_create_started(2000, TIMER_MODE_REPEAT, led3_blink, NULL);

  if (led1 == SAFETIMER_INVALID_HANDLE) {
    printf("ERROR: Failed to create led1 timer\n");
    return;
  }

  printf("All timers created and started\n\n");

  /* Simulate 5 seconds */
  for (int i = 0; i < 50; i++) {
    mock_advance_time(100);
    safetimer_process();
  }

  /* Cleanup */
  safetimer_delete(led1);
  safetimer_delete(led2);
  safetimer_delete(led3);
}

/* ========== Example 3: Batch Creation ========== */

void example_batch_creation(void) {
  printf("\n========== Example 3: Batch Creation ==========\n");
  printf("Use case: Create multiple similar timers efficiently\n\n");

  safetimer_handle_t timers[3];
  timer_callback_t callbacks[] = {led1_blink, led2_blink, led3_blink};
  void *user_data[] = {NULL, NULL, NULL};

  int created = safetimer_create_started_batch(3, 500, TIMER_MODE_REPEAT,
                                               callbacks, user_data, timers);

  printf("Created %d/3 timers\n\n", created);

  if (created < 3) {
    printf("WARNING: Partial creation (pool may be full)\n");
  }

  /* Run for 3 seconds */
  for (int i = 0; i < 30; i++) {
    mock_advance_time(100);
    safetimer_process();
  }

  /* Cleanup */
  for (int i = 0; i < created; i++) {
    if (timers[i] != SAFETIMER_INVALID_HANDLE) {
      safetimer_delete(timers[i]);
    }
  }
}

/* ========== Example 4: Macro Helper with Error Checking ========== */

void example_macro_helper(void) {
  printf("\n========== Example 4: Macro Helper (Error Checking) ==========\n");
  printf("Use case: Automatic error handling with minimal code\n\n");

  safetimer_handle_t heartbeat;

  /* Create with automatic error checking */
  SAFETIMER_CREATE_STARTED_OR(
      heartbeat, 1000, TIMER_MODE_REPEAT, led1_blink, NULL, {
        printf("ERROR: Failed to create heartbeat timer\n");
        return;
      });

  printf("Heartbeat timer created successfully\n");
  printf("No explicit error checking code needed!\n\n");

  /* Run for 3 seconds */
  for (int i = 0; i < 30; i++) {
    mock_advance_time(100);
    safetimer_process();
  }

  safetimer_delete(heartbeat);
}

/* ========== Example 5: Comparison (Helper vs Core) ========== */

void example_comparison(void) {
  printf("\n========== Example 5: Code Comparison ==========\n\n");

  printf("Core API (explicit control):\n");
  printf("  safetimer_handle_t h = safetimer_create(500, TIMER_MODE_REPEAT, "
         "cb, NULL);\n");
  printf("  if (h != SAFETIMER_INVALID_HANDLE) {\n");
  printf("      safetimer_start(h);\n");
  printf("  }\n\n");

  printf("Helper API (convenience):\n");
  printf("  safetimer_handle_t h = safetimer_create_started(500, "
         "TIMER_MODE_REPEAT, cb, NULL);\n\n");

  printf("Lines of code saved: ~3 lines per timer\n");
  printf("Flash overhead: 0 bytes (inline function)\n");
}

/* ========== Main ========== */

int main(void) {
  printf("SafeTimer Helpers API Examples\n");
  printf("===============================\n");

  /* Run all examples */
  example_core_api();
  example_helper_api();
  example_batch_creation();
  example_macro_helper();
  example_comparison();

  printf("\n========== All Examples Completed ==========\n");
  return 0;
}
