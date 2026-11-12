#include "pc_uart.h"
#include "stm32f1xx_hal_uart.h"
#include <string.h>

#define PC_UART_LINE_CAPACITY 96U

UART_HandleTypeDef huart1;

HAL_StatusTypeDef PC_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    return HAL_UART_Init(&huart1);
}

int PC_UART_ReadLine(char *line, size_t capacity)
{
    static char partial_line[PC_UART_LINE_CAPACITY];
    static size_t partial_length;
    static uint8_t discard_line;
    uint8_t value;
    HAL_StatusTypeDef status;

    if ((line == NULL) || (capacity == 0))
    {
        return -1;
    }

    status = HAL_UART_Receive(&huart1, &value, 1, 1);
    if (status == HAL_TIMEOUT)
    {
        return 0;
    }
    if (status != HAL_OK)
    {
        return -1;
    }

    if (value == '\n')
    {
        if (discard_line || (partial_length >= capacity))
        {
            partial_length = 0;
            discard_line = 0;
            return -1;
        }

        partial_line[partial_length] = '\0';
        memcpy(line, partial_line, partial_length + 1U);
        partial_length = 0;
        return 1;
    }

    if ((value == '\r') || discard_line)
    {
        return 0;
    }

    if ((value < 32U) || (value > 126U) ||
        (partial_length >= sizeof(partial_line) - 1U))
    {
        partial_length = 0;
        discard_line = 1;
        return 0;
    }

    partial_line[partial_length++] = (char)value;
    return 0;
}

HAL_StatusTypeDef PC_UART_ReadBytes(uint8_t *data, uint16_t length,
                                    uint32_t timeout)
{
    if ((data == NULL) || (length == 0U))
    {
        return HAL_ERROR;
    }

    return HAL_UART_Receive(&huart1, data, length, timeout);
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio = {0};

    if (huart->Instance == USART1)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_USART1_CLK_ENABLE();

        gpio.Pin = GPIO_PIN_9;
        gpio.Mode = GPIO_MODE_AF_PP;
        gpio.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &gpio);

        gpio.Pin = GPIO_PIN_10;
        gpio.Mode = GPIO_MODE_INPUT;
        gpio.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &gpio);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        __HAL_RCC_USART1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
    }
}
