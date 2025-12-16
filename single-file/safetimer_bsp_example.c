/**
 * @file    safetimer_bsp_example.c
 * @brief   Example BSP implementation for SafeTimer Single-File Version
 * @note    This is a template - adapt it to your specific MCU
 */

#include "safetimer_single.h"
#include <8051.h>  /* For EA register on SC8F072/8051 platforms */

/* ========================================================================== */
/*                         BSP IMPLEMENTATION                                 */
/* ========================================================================== */

/**
 * Global tick counter (incremented by hardware timer interrupt every 1ms)
 */
static volatile bsp_tick_t s_system_ticks = 0;

/**
 * Critical section nesting counter for re-entrant support
 * - 0 = not in critical section, interrupts may be enabled
 * - >0 = in critical section, tracks nesting depth
 */
static volatile uint8_t s_critical_nesting = 0;

/**
 * Saved interrupt state (EA register value before first enter)
 */
static volatile uint8_t s_saved_ea = 0;

/**
 * @brief Hardware timer interrupt (called every 1ms)
 * @note Must be configured by user's hardware initialization code
 */
void timer_interrupt_handler(void) {
    s_system_ticks++;
}

/**
 * @brief Get current system tick count
 * @return Tick count in milliseconds
 *
 * @note Thread-safe: Uses atomic read with interrupt protection
 *       to prevent torn reads on 8-bit MCUs reading 32-bit values.
 *       Critical on 8051 where 32-bit reads require 4 separate instructions.
 *
 *       Preserves caller's interrupt state - safe to call inside critical sections.
 */
bsp_tick_t bsp_get_ticks(void) {
    bsp_tick_t ticks;
    uint8_t saved_ea;

    /* Save current interrupt state, then disable for atomic read */
    saved_ea = EA;
    EA = 0;

    /* Atomic read: ISR cannot update s_system_ticks while EA=0 */
    ticks = s_system_ticks;

    /* Restore original interrupt state (not unconditionally enable) */
    EA = saved_ea;

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
    ea_state = EA;
    EA = 0;  /* Disable interrupts for atomic nesting counter update */

    if (s_critical_nesting == 0) {
        /* First entry: save the interrupt state from before this call */
        s_saved_ea = ea_state;
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
 */
void bsp_exit_critical(void) {
    EA = 0;  /* Ensure atomic access to nesting counter */

    if (s_critical_nesting > 0) {
        s_critical_nesting--;

        if (s_critical_nesting == 0) {
            /* Exiting outermost critical section: restore saved EA state */
            EA = s_saved_ea;
        }
        /* else: still nested, keep interrupts disabled (EA remains 0) */
    }
    /* else: unbalanced call (exit without enter), leave EA as-is */
}

/* ========================================================================== */
/*                    HARDWARE TIMER INITIALIZATION EXAMPLES                  */
/* ========================================================================== */

#if 0  /* Example code - uncomment and adapt for your platform */

/* Example 1: 8051 (SDCC) - Timer0 1ms interrupt @ 11.0592MHz */
void init_timer0(void) {
    TMOD = 0x01;  /* Timer0, Mode 1 (16-bit) */
    TH0 = 0xFC;   /* Load high byte for 1ms @ 11.0592MHz */
    TL0 = 0x18;   /* Load low byte */
    ET0 = 1;      /* Enable Timer0 interrupt */
    EA = 1;       /* Enable global interrupt */
    TR0 = 1;      /* Start Timer0 */
}

void timer0_isr(void) __interrupt(1) {
    TH0 = 0xFC;   /* Reload high byte */
    TL0 = 0x18;   /* Reload low byte */
    timer_interrupt_handler();
}

/* Example 2: STM32 (HAL) - TIM2 1ms interrupt */
void init_timer(void) {
    __HAL_RCC_TIM2_CLK_ENABLE();

    TIM_HandleTypeDef htim2;
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = (SystemCoreClock / 1000000) - 1;  /* 1µs tick */
    htim2.Init.Period = 1000 - 1;  /* 1ms period */
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;

    HAL_TIM_Base_Init(&htim2);
    HAL_TIM_Base_Start_IT(&htim2);
}

void TIM2_IRQHandler(void) {
    if (__HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_UPDATE)) {
        __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
        timer_interrupt_handler();
    }
}

/* Example 3: AVR (GCC) - Timer1 1ms interrupt @ 16MHz */
void init_timer1(void) {
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);  /* CTC mode, prescaler 64 */
    OCR1A = 249;  /* 16MHz / 64 / 250 = 1ms */
    TIMSK1 = (1 << OCIE1A);  /* Enable compare interrupt */
    sei();  /* Enable global interrupts */
}

ISR(TIMER1_COMPA_vect) {
    timer_interrupt_handler();
}

#endif

/* ========================================================================== */
/*                         USAGE EXAMPLE                                      */
/* ========================================================================== */

#if 0  /* Example application code */

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
