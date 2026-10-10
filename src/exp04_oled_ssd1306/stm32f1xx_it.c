#include "stm32f1xx_hal.h"

/* Required: the CMSIS startup file weak-aliases SysTick_Handler to
 * Default_Handler (an infinite loop). */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
