#ifndef UTIL_H
#define UTIL_H

#include "stm32f4xx.h"

#define PI 3.141592654

/**
 * @brief Create a system delay in milliseconds.
 * 
 * @param ms The delay in milliseconds.
 */
void delay_ms(uint32_t ms);

/**
 * @brief Creates a system delay in microseconds.
 * 
 * @param us The delay in microseconds.
 */
void delay_us(uint32_t us);

/**
 * @brief Returns value based on the
 * sign of the given float number.
 * 
 * @param num The number to determine the sign from.
 * @return  int8_t: 1 if the number is positive,
 *          -1 if the number is negative,
 *          0 if the number is zero.
 */
int8_t signf(float num);

#endif