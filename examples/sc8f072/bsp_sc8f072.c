/**
 * @file    bsp_sc8f072.c
 * @brief   BSP Implementation for SC8F072 (SDCC)
 * @version 1.2.1
 * @date    2025-12-16
 * @note    This is a template - adapt it to your specific MCU
 */

#include "safetimer_single.h"
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
static volatile bsp_tick_t s_system_ticks = 0;

/**
 * Critical section nesting counter for re-entrant support
 * - 0 = not in critical section, interrupts may be enabled
 * - >0 = in critical section, tracks nesting depth
 */
static volatile uint8_t s_critical_nesting = 0;

/**
 * Saved interrupt state (GIE register value before first enter)
 */
static volatile bit s_saved_ea = 0;

/**
 * @brief Hardware timer interrupt (called every 1ms)
 * @note Must be configured by user's hardware initialization code
 */
void interrupt Timer0_Isr(void) { s_system_ticks++; }

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
  uint8_t saved_ea;

  /* Save current interrupt state, then disable for atomic read */
  saved_ea = GIE ? 1U : 0U; /* Convert bit to uint8_t */
  GIE = 0;

  /* Atomic read: ISR cannot update s_system_ticks while EA=0 */
  ticks = s_system_ticks;

  /* Restore original interrupt state (not unconditionally enable) */
  if (saved_ea) {
    GIE = 1;
  }

  return ticks;
}

/**
 * @brief Enter critical section (disable interrupts)
 *
 * Implementation for SC8F072/8051 with nesting support:
 * - First call: saves current EA state and disables interrupts
 * - Nested calls: only increments counter, preserves saved state
 * - Must be paired with bsp_exit_critical()
 * - Keep critical section duration minimal (<50μs recommended)
 *
 * @note Re-entrant: supports nested calls (tracks depth via counter)
 *       Safe to call from ISRs (preserves pre-entry interrupt state)
 */
void bsp_enter_critical(void) {
  uint8_t ea_state;

  /* Read current interrupt state before disabling */
  ea_state = GIE ? 1U : 0U; /* Convert bit to uint8_t */
  GIE = 0; /* Disable interrupts for atomic nesting counter update */

  if (s_critical_nesting == 0) {
    /* First entry: save the interrupt state from before this call */
    s_saved_ea = ea_state ? 1U : 0U; /* Convert uint8_t to bit */
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
      /* Exiting outermost critical section: restore saved EA state */
      if (s_saved_ea) {
        GIE = 1;
      }
      /* else: GIE remains 0 (interrupts stay disabled) */
    }
    /* else: still nested, keep interrupts disabled (EA remains 0) */
  }
  /* else: unbalanced call (exit without enter), leave GIE unchanged */
}

/* ========================================================================== */
/*                    HARDWARE TIMER INITIALIZATION EXAMPLES                  */
/* ========================================================================== */
void init_system(void) {
  OSCCON = 0x70;     // 16MHz,enternal oscillator,wdt not enabled
  OPTION_REG = 0x03; // Timer0 mode, 1:16 prescaler
  /*********************************************************************
  预分频器控制寄存器 OPTION_REG（01H）
  Bit7  未用
  Bit6  INTEDG:  触发中断的边沿选择位
          1=  INT 引脚上升沿触发中断
          0=  INT 引脚下降沿触发中断
  Bit5  T0CS:  TIMER0 时钟源选择位
          0=  内部指令周期时钟（FSYS /4）
          1=	T0CKI 引脚上的跳变沿
  Bit4  T0SE:	TIMER0 时钟源边沿选择位。
          0=	在 T0CKI 引脚信号从低电平跳变到高电平时递增
          1=	在 T0CKI 引脚信号从高电平跳变到低电平时递增
  Bit3  PSA: 预分频器分配位
          0=	预分频器分配给 TIMER0 模块
          1=	预分频器分配给 WDT
  Bit2~Bit0  PS2~PS0:  预分配参数配置位
          PS2  PS1  PS0  TMR0分频比  WDT 分频比
          0    0    0  	1:2  		1:1
          0  	 0	  1		1:4			1:2
          0	   1	  0		1:8			1:4
          0	   1	  1		1:16		1:8
          1  	 0	  0		1:32		1:16
          1	   0	  1		1:64		1:32
          1	   1	  0		1:128		1:64
          1	   1	  1		1:256		1:128
  *********************************************************************/
}
void init_timer0(void) {
  TMR0 = 6; // (256-6)*4*16/16=1ms
  T0IF = 0; // clear timer0 interrupt flag
  T0IE = 1; // enable timer0 interrupt
  GIE = 1;  // enable global interrupt
}

/* ========================================================================== */
/*                         USAGE EXAMPLE                                      */
/* ========================================================================== */

#if 0 /* Example application code */

#include "safetimer_single.h"

void led_callback(void *data) {
    /* Toggle LED */
    int led_id = *(int*)data;
    toggle_led(led_id);
}

int main(void) {
    /* 1. Initialize hardware timer (1ms tick) */
    init_timer0();  /* or init_timer(), init_timer1(), etc. */

    /* 2. Create timers */
    int led1 = 1, led2 = 2;

    safetimer_handle_t timer1 = safetimer_create(
        500,                    /* 500ms period */
        TIMER_MODE_REPEAT,      /* Repeat mode */
        led_callback,           /* Callback function */
        &led1                   /* User data */
    );

    safetimer_handle_t timer2 = safetimer_create(
        1000,                   /* 1000ms period */
        TIMER_MODE_REPEAT,
        led_callback,
        &led2
    );

    /* 3. Start timers */
    safetimer_start(timer1);
    safetimer_start(timer2);

    /* 4. Main loop */
    while (1) {
        safetimer_process();  /* Process timers */

        /* Your application code here */
    }

    return 0;
}

#endif
