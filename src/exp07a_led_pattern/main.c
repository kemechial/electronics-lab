/*
 * Experiment 7a: LED brightness pattern with hardware PWM, Blue Pill (STM32F103C8T6, 72 MHz).
 *
 * PWM: TIM4_CH1 on PB6 (DS5319 Table 5 p.32: PB6 FT, default alternate function
 *      I2C1_SCL/TIM4_CH1; RM0008 Table 43 p.178: TIM4_CH1 = PB6 with TIM4_REMAP = 0).
 * TIM3_CH3 on PB0 was tried first: TIM3 ran correctly but PB0 never went high on this
 * clone (Cortex-M3 r2p0, not ST's r1p1), see the README.
 * Duty: updated every 1 ms from SysTick (stm32f1xx_it.c). No HAL_Delay, integer math only.
 * Pattern and gamma flag: config.h.
 */
#include <stdint.h>
#include "stm32f1xx_hal.h"
#include "config.h"
#if GAMMA
#include "gamma_table.h"
#endif

#if HSE_VALUE != 8000000U
#error "Blue Pill has an 8 MHz crystal: HSE_VALUE must be 8000000"
#endif

#if PWM_OUT == PWM_OUT_PB0
/* TIM3_CH3 on PB0: DS5319 Table 5 p.29; RM0008 Table 44 p.178 (TIM3_REMAP = 00) */
#define PWM_TIM             TIM3
#define PWM_TIM_CLK_ENABLE  __HAL_RCC_TIM3_CLK_ENABLE
#define PWM_CHANNEL         TIM_CHANNEL_3
#define PWM_PIN             GPIO_PIN_0
#else
/* TIM4_CH1 on PB6: DS5319 Table 5 p.32; RM0008 Table 43 p.178 (TIM4_REMAP = 0) */
#define PWM_TIM             TIM4
#define PWM_TIM_CLK_ENABLE  __HAL_RCC_TIM4_CLK_ENABLE
#define PWM_CHANNEL         TIM_CHANNEL_1
#define PWM_PIN             GPIO_PIN_6
#endif

#define N_SEGMENTS  (sizeof(ACTIVE_PATTERN) / sizeof(ACTIVE_PATTERN[0]))
#define LEVEL_MAX   10000U      /* brightness in 0.01 % units */

static TIM_HandleTypeDef htim_pwm;
static uint32_t pwm_steps;      /* ARR + 1 = duty resolution in steps */

/* Live Watch */
volatile uint32_t seg_index;
volatile uint32_t seg_elapsed_ms;

static void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;              /* 8 MHz x 9 = 72 MHz */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;        /* 72 MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV2;         /* 36 MHz (max) */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;         /* 72 MHz */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

/* TIM4 input clock per RM0008 p.94 (clock tree): if the APB1 prescaler is 1 the timer
 * clock equals PCLK1, otherwise it is twice PCLK1. */
static uint32_t pwm_tim_clock_hz(void)
{
    const uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();

    return ((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1) ? pclk1 : 2U * pclk1;
}

static void PWM_Init(void)
{
    GPIO_InitTypeDef g = {0};
    TIM_OC_InitTypeDef oc = {0};
    const uint32_t tclk = pwm_tim_clock_hz();
    /* finest resolution: smallest prescaler that keeps ARR within 16 bits */
    const uint32_t psc = (tclk / PWM_HZ + 65535U) / 65536U - 1U;

    pwm_steps = tclk / ((psc + 1U) * PWM_HZ);   /* 72 MHz: PSC 1, 36000 steps */

    __HAL_RCC_GPIOB_CLK_ENABLE();
    PWM_TIM_CLK_ENABLE();

    g.Pin = PWM_PIN;
    g.Mode = GPIO_MODE_AF_PP;                   /* timer channel, no remap needed */
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);

    htim_pwm.Instance = PWM_TIM;
    htim_pwm.Init.Prescaler = psc;
    htim_pwm.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim_pwm.Init.Period = pwm_steps - 1U;
    htim_pwm.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim_pwm.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim_pwm) != HAL_OK) {
        Error_Handler();
    }

    /* PWM mode 1, OC1M = 110 (RM0008 p.387, TIMx_CCMR1 p.413): PB6 high while CNT < CCR1.
     * HAL sets OC1PE, so a new CCR1 takes effect at the next update event. */
    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim_pwm, &oc, PWM_CHANNEL) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_TIM_PWM_Start(&htim_pwm, PWM_CHANNEL) != HAL_OK) {
        Error_Handler();
    }
}

/* Brightness level (0..LEVEL_MAX) -> CCR1 value (0..pwm_steps). */
static uint32_t level_to_ccr(uint32_t level)
{
#if GAMMA
    const uint32_t i = level / 100U;            /* percent index */
    const uint32_t frac = level % 100U;
    uint32_t g = gamma_2_2[i];

    if (i < 100U) {
        g += ((uint32_t)(gamma_2_2[i + 1U] - gamma_2_2[i]) * frac) / 100U;
    }
    return (g * pwm_steps + 32767U) / 65535U;   /* 65535 * 65536 < 2^32 */
#else
    return (level * pwm_steps + LEVEL_MAX / 2U) / LEVEL_MAX;
#endif
}

/* Called from SysTick_Handler every 1 ms. */
void pattern_tick_1ms(void)
{
    const segment_t *s;
    int64_t level;
    uint32_t n;

    if (pwm_steps == 0U) {
        return;                                 /* PWM not initialised yet */
    }
    s = &ACTIVE_PATTERN[seg_index];
    for (n = 0; seg_elapsed_ms >= s->duration_ms; n++) {   /* next segment, skips 0 ms ones */
        if (n >= N_SEGMENTS) {
            return;                             /* every segment is 0 ms: nothing to play */
        }
        seg_elapsed_ms = 0;
        seg_index = (seg_index + 1U) % N_SEGMENTS;
        s = &ACTIVE_PATTERN[seg_index];
    }

    /* linear ramp start -> end over duration_ms, in 0.01 % units */
    level = (int64_t)s->start_percent * 100 +
            ((int64_t)((int32_t)s->end_percent - (int32_t)s->start_percent) * 100 *
             (int64_t)seg_elapsed_ms) / (int64_t)s->duration_ms;
    __HAL_TIM_SET_COMPARE(&htim_pwm, PWM_CHANNEL, level_to_ccr((uint32_t)level));
    seg_elapsed_ms++;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    PWM_Init();

    for (;;) {
    }
}
