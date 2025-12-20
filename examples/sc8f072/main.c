/**
 * @file    main.c
 * @brief   SC8F072 SafeTimer Demo - LED Blink Example
 * @version 1.3.6
 * @date    2025-12-20
 *
 * @note    Complete example with BSP integration for SC8F072 MCU
 *
 * Hardware Setup:
 * - LED connected to P0.0 (active low)
 * - System clock: 8MHz internal RC
 * - Timer0: 1ms interrupt for SafeTimer tick
 *
 * Build files needed:
 * - main.c (this file)
 * - bsp_sc8f072.c (BSP + init functions)
 * - safetimer.c (from src/)
 */

#include "safetimer.h" /* SafeTimer API (已含 bsp.h) */
#include <sc.h>        /* SC8F072 寄存器定义 - 必须放最前面 */

/* BSP 函数声明 (定义在 bsp_sc8f072.c) */
void init_system(void);
void init_timer0(void);

/* ========== Hardware Configuration ========== */

#define LED_PORT P0
#define LED_PIN 0
#define LED_ON() (LED_PORT &= ~(1U << LED_PIN)) /* Active low */
#define LED_OFF() (LED_PORT |= (1U << LED_PIN))
#define LED_TOGGLE() (LED_PORT ^= (1U << LED_PIN))

/* ========== Timer Handles ========== */
static safetimer_handle_t g_led_timer;

/* ========== Callback Functions ========== */

/**
 * @brief LED blink callback - toggles LED every 500ms
 */
void led_callback(void *user_data) {
  (void)user_data; /* Unused parameter */
  LED_TOGGLE();
}

/* ========== Main Function ========== */

int main(void) {
  timer_error_t err; /* C89: declare before statements */

  /* Disable watchdog during initialization */
  asm("nop");
  asm("clrwdt");

  /* Initialize hardware (BSP functions) */
  init_system(); /* GPIO, clock configuration */
  init_timer0(); /* 1ms tick timer */

  /* Create LED blink timer: 500ms repeat */
  g_led_timer = safetimer_create(500,               /* Period: 500ms */
                                 TIMER_MODE_REPEAT, /* Mode: repeat forever */
                                 led_callback,      /* Callback function */
                                 ((void *)0)        /* No user data (NULL) */
  );

  /* Check if timer was created successfully */
  if (g_led_timer == SAFETIMER_INVALID_HANDLE) {
    /* Timer creation failed - error indication */
    while (1) {
      LED_ON();
      asm("clrwdt");
    }
  }

  /* Start the timer */
  err = safetimer_start(g_led_timer);
  if (err != TIMER_OK) {
    /* Start failed - error indication */
    while (1) {
      LED_ON();
      asm("clrwdt");
    }
  }

  /* Main loop */
  while (1) {
    asm("clrwdt");       /* Feed watchdog */
    safetimer_process(); /* Process all timers */
  }
}