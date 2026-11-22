
#include "main.h"
#include "stm32f4xx_it.h"
#include "main_app.h"

#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"

extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart1;
extern void xPortSysTickHandler(void);

/* FreeRTOS and HAL system tick */
void SysTick_Handler(void)
{
    HAL_IncTick();
    xPortSysTickHandler();
}

/* MPU6500 external interrupt */
void EXTI15_10_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_10);
}

/* GPS UART interrupt */
void UART4_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart4);
}

/* USART1 interrupt */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

/* GPIO interrupt callback */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_10)
    {
        APP_MotionIRQ();
    }
}
