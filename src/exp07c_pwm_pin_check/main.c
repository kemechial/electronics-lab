/*
 * exp07c: PWM pin check, Blue Pill (STM32F103C8T6, 72 MHz).
 * Constant 65 % PWM at 1 kHz, hardware PWM on two pins at once:
 *   PB0 = TIM3_CH3 (DS5319 Table 5 p.29; RM0008 Table 44 p.178, TIM3_REMAP = 00)
 *   PB6 = TIM4_CH1 (DS5319 Table 5 p.32; RM0008 Table 43 p.178, TIM4_REMAP = 0)
 * REF_PA1 build (env exp07c_c6, low-density F103C6 without TIM4, DocID15060 pp.18-19):
 *   PB0 = TIM3_CH3 (DocID15060 Table 5 p.27) and reference
 *   PA1 = TIM2_CH2 (DocID15060 Table 5 p.26; RM0008 Table 45 p.179, TIM2_REMAP = 00)
 * PWM mode 1 (RM0008 p.387). Pins: alternate function push-pull, 2 MHz
 * (CNF = 10, MODE = 10, RM0008 Tables 20-21 p.161).
 * PC13 on-board LED blinks at 1 Hz as a heartbeat (PC13 sink/source limit 3 mA, DS5319 p.64).
 * No UART output.
 */
#include <stdint.h>
#include "stm32f1xx_hal.h"

#if HSE_VALUE != 8000000U
#error "Blue Pill has an 8 MHz crystal: HSE_VALUE must be 8000000"
#endif

#define PWM_HZ          1000U
#define DUTY_PERCENT    65U
#define HEARTBEAT_MS    500U        /* toggle every 500 ms = 1 Hz blink */

#if defined(REF_PA1)
#define REF_TIM             TIM2
#define REF_TIM_CLK_ENABLE  __HAL_RCC_TIM2_CLK_ENABLE
#define REF_CHANNEL         TIM_CHANNEL_2
#define REF_PORT            GPIOA
#define REF_PIN             GPIO_PIN_1
#else
#define REF_TIM             TIM4
#define REF_TIM_CLK_ENABLE  __HAL_RCC_TIM4_CLK_ENABLE
#define REF_CHANNEL         TIM_CHANNEL_1
#define REF_PORT            GPIOB
#define REF_PIN             GPIO_PIN_6
#endif

static TIM_HandleTypeDef htim3;
static TIM_HandleTypeDef htim_ref;

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

/* TIM2/TIM3/TIM4 (APB1) input clock per RM0008 p.94: equal to PCLK1 if the APB1
 * prescaler is 1, otherwise twice PCLK1. */
static uint32_t apb1_timer_clock_hz(void)
{
    const uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();

    return ((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1) ? pclk1 : 2U * pclk1;
}

static void pwm_start(TIM_HandleTypeDef *h, TIM_TypeDef *tim, uint32_t channel)
{
    TIM_OC_InitTypeDef oc = {0};
    const uint32_t tclk = apb1_timer_clock_hz();
    const uint32_t psc = (tclk / PWM_HZ + 65535U) / 65536U - 1U;   /* 72 MHz: PSC 1 */
    const uint32_t steps = tclk / ((psc + 1U) * PWM_HZ);            /* 36000 */

    h->Instance = tim;
    h->Init.Prescaler = psc;
    h->Init.CounterMode = TIM_COUNTERMODE_UP;
    h->Init.Period = steps - 1U;
    h->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    h->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(h) != HAL_OK) {
        Error_Handler();
    }

    oc.OCMode = TIM_OCMODE_PWM1;                /* high while CNT < CCR (RM0008 p.387) */
    oc.Pulse = steps * DUTY_PERCENT / 100U;     /* 23400 of 36000 */
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(h, &oc, channel) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_TIM_PWM_Start(h, channel) != HAL_OK) {
        Error_Handler();
    }
}

static void GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    g.Pin = GPIO_PIN_0;
    g.Mode = GPIO_MODE_AF_PP;                   /* CNF = 10, MODE = 10 (RM0008 p.161) */
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);
    g.Pin = REF_PIN;
    HAL_GPIO_Init(REF_PORT, &g);

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);    /* active-low: off */
    g.Pin = GPIO_PIN_13;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &g);
}

int main(void)
{
    uint32_t t_led;

    HAL_Init();
    SystemClock_Config();
    GPIO_Init();

    __HAL_RCC_TIM3_CLK_ENABLE();
    REF_TIM_CLK_ENABLE();
    pwm_start(&htim3, TIM3, TIM_CHANNEL_3);     /* PB0 */
    pwm_start(&htim_ref, REF_TIM, REF_CHANNEL); /* reference: PB6 (C8) or PA1 (C6) */

    t_led = HAL_GetTick();
    for (;;) {
        if (HAL_GetTick() - t_led >= HEARTBEAT_MS) {
            t_led += HEARTBEAT_MS;
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }
    }
}
