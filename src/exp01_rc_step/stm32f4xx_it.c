#include "stm32f4xx_hal.h"

/* Required: the CMSIS startup file weak-aliases SysTick_Handler to
 * Default_Handler (an infinite loop). Without this, the first SysTick after
 * HAL_Init() traps the CPU and HAL_Delay()/HAL_GetTick() never advance. */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
