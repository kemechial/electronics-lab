/*
 * Experiment 6: NPN transistor (P2N2222A) as a low-side LED switch, Blue Pill (STM32F103C8T6, 72 MHz).
 * PB0 (push-pull) -> RB -> base; LED + R_LED from 3V3 to collector; emitter to GND.
 * PB0 HIGH for 2 s, LOW for 2 s, timed with HAL_GetTick (no HAL_Delay).
 * No UART output (USART1 wiring not confirmed); PC13 LED mirrors PB0 instead.
 */
#include "stm32f1xx_hal.h"

#if HSE_VALUE != 8000000U
#error "Blue Pill has an 8 MHz crystal: HSE_VALUE must be 8000000"
#endif

#define PHASE_MS    2000U

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

static void GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);   /* transistor off */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);    /* PC13 active-low: off */

    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOB, &g);
    g.Pin = GPIO_PIN_13;
    HAL_GPIO_Init(GPIOC, &g);
}

int main(void)
{
    uint32_t t_phase;
    uint8_t on = 0;

    HAL_Init();
    SystemClock_Config();
    GPIO_Init();

    t_phase = HAL_GetTick();
    for (;;) {
        if (HAL_GetTick() - t_phase >= PHASE_MS) {
            t_phase += PHASE_MS;
            on = !on;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, on ? GPIO_PIN_RESET : GPIO_PIN_SET);  /* in sync */
        }
    }
}
