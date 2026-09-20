#include "main.h"
#include "stm32f4xx_it.h"
#include "main_app.h"

extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart1;

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void EXTI15_10_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_10);
}

void UART4_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart4);
}

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_10)
    {
        APP_MotionIRQ();
    }
}
