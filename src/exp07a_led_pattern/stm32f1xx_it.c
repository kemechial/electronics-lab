#include "stm32f1xx_hal.h"

void pattern_tick_1ms(void);

/* Required: the CMSIS startup file weak-aliases SysTick_Handler to
 * Default_Handler (an infinite loop). SysTick runs at 1 kHz (HAL_Init),
 * so it also drives the pattern update every 1 ms. */
void SysTick_Handler(void)
{
    HAL_IncTick();
    pattern_tick_1ms();
}
