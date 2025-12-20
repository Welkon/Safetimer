/**
 * @file    bsp_sc8f072.c
 * @brief   BSP Implementation for SC8F072 (SDCC)
 * @version 1.2.1
 * @date    2025-12-16
 * @note    This is a template - adapt it to your specific MCU
 */

#include "bsp.h"
#include <sc.h> /* For register on SC8F072 platforms */

/* ========================================================================== */
/*                         BSP IMPLEMENTATION                                 */
/* ========================================================================== */

/**
 * Global tick counter (incremented by hardware timer interrupt every 1ms)
 *
 * Note: If BSP_TICK_TYPE_16BIT=1, this is uint16_t and wraps at 65.5 seconds.
 *       SafeTimer handles wrapping correctly. For longer periods, use uint32_t.
 */
static volatile bsp_tick_t s_ticks = 0;

/**
 * Critical section nesting counter for re-entrant support
 * - 0 = not in critical section, interrupts may be enabled
 * - >0 = in critical section, tracks nesting depth
 */
static volatile uint8_t s_critical_nesting = 0;

/**
 * Saved interrupt state (GIE register value before first enter)
 */
static volatile bit s_saved_interrupt_state = 0;

/**
 * @brief Hardware timer interrupt (called every 1ms)
 * @note Must be configured by user's hardware initialization code
 */
void interrupt Timer0_Isr(void) {
  T0IF = 0;    /* Clear Timer0 interrupt flag */
  TMR0 = 0x06; /* Reload timer for next 1ms (adjust for your clock) */
  s_ticks++;
}

/* ========== Hardware Initialization (User-Provided) ========== */

/**
 * @brief Initialize system clock and GPIO
 * @note Called from main() before init_timer0()
 */
void init_system(void) {
  /* Configure system clock - 8MHz internal RC */
  /* Adjust based on your SC8F072 configuration */

  /* Configure GPIO as needed */
  TRISA = 0xFF; /* All PORTA as input initially */
  TRISA0 = 0;   /* P0.0 as output (LED) */
  PORTA = 0xFF; /* LED off (active low) */
}

/**
 * @brief Initialize Timer0 for 1ms interrupt
 *
 * Configuration for SC8F072 @ 8MHz:
 * - Timer0 as system tick source
 * - 1ms interrupt interval
 *
 * @note Adjust reload values based on actual clock configuration
 */
void init_timer0(void) {
  T0CON = 0x00; /* Stop timer, configure prescaler */
  TMR0 = 0x06;  /* Reload value for 1ms */
  T0IE = 1;     /* Enable Timer0 interrupt */
  T0IF = 0;     /* Clear interrupt flag */
  T0CON = 0x80; /* Enable timer */

  GIE = 1; /* Enable global interrupts */
}

/**
 * @brief Get current system tick count
 * @return Tick count in milliseconds
 *
 * @note Thread-safe: Uses atomic read with interrupt protection
 *       to prevent torn reads on 8-bit MCUs reading 32-bit values.
 *       Critical on 8051 where 32-bit reads require 4 separate instructions.
 *
 *       Preserves caller's interrupt state - safe to call inside critical
 * sections.
 */
bsp_tick_t bsp_get_ticks(void) {
  bsp_tick_t ticks;
  uint8_t saved_state; /* C89: declare before statements */

  /* Save current interrupt state, then disable for atomic read */
  saved_state = GIE ? 1U : 0U; /* Convert bit to uint8_t */
  GIE = 0;                     /* Disable for atomic read */

  /* Atomic read: ISR cannot update s_system_ticks while GIE=0 */
  ticks = s_ticks;

  /* Restore original interrupt state (not unconditionally enable) */
  GIE = saved_state;

  return ticks;
}

/**
 * @brief Enter critical section (disable interrupts)
 *
 * Implementation for SC8F072/8051 with nesting support:
 * - First call: saves current EA state and disables interrupts
 * - Nested calls: only increments counter, preserves saved state
 * - Must be paired with bsp_exit_critical()
 * - Keep critical section duration minimal (<50渭s recommended)
 *
 * @note Re-entrant: supports nested calls (tracks depth via counter)
 *       Safe to call from ISRs (preserves pre-entry interrupt state)
 */
void bsp_enter_critical(void) {
  uint8_t ea_state; /* C89: declare before statements */

  /* Read current interrupt state before disabling */
  ea_state = GIE ? 1U : 0U; /* Convert bit to uint8_t */
  GIE = 0; /* Disable interrupts for atomic nesting counter update */

  if (s_critical_nesting == 0) {
    /* First entry: save the interrupt state from before this call */
    s_saved_interrupt_state = ea_state ? 1U : 0U; /* Convert uint8_t to bit */
  }

  s_critical_nesting++;

  /* Interrupts remain disabled (EA already cleared above) */
}

/**
 * @brief Exit critical section (restore interrupts)
 *
 * Implementation for SC8F072/8051 with nesting support:
 * - Nested calls: only decrements counter, keeps interrupts disabled
 * - Final call: restores interrupt state from first bsp_enter_critical()
 * - Must be called after bsp_enter_critical()
 * - Ensure balanced calls in all code paths
 *
 * @note Re-entrant: only re-enables interrupts when outermost section exits
 *       Restores original EA state (not always "enable"), safe in ISRs
 *       Protected against unbalanced calls (exit without enter)
 */
void bsp_exit_critical(void) {
  if (s_critical_nesting > 0) {

    GIE = 0; /* Ensure atomic access to nesting counter */

    s_critical_nesting--;

    if (s_critical_nesting == 0) {
      GIE = s_saved_interrupt_state; /* Restore original state */
      /* else: GIE remains 0 (interrupts stay disabled) */
    }
    /* else: still nested, keep interrupts disabled (EA remains 0) */
  }
  /* else: unbalanced call (exit without enter), leave GIE unchanged */
}
