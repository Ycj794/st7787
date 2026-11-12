#ifndef PC_UART_H
#define PC_UART_H

#include "main.h"
#include <stddef.h>

HAL_StatusTypeDef PC_UART_Init(void);
int PC_UART_ReadLine(char *line, size_t capacity);
HAL_StatusTypeDef PC_UART_ReadBytes(uint8_t *data, uint16_t length,
                                    uint32_t timeout);

#endif
