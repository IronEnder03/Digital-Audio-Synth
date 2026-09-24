#ifndef UTIL_H
#define UTIL_H

#include "stm32f4xx.h"

#define PI 3.141592654

/**
 * @brief Creates a system delay.
 *
 * This function takes the given count and decrements
 * it to zero, which will end the delay.
 *
 * @param count  The starting count used for the delay.
 * @return       void
 */
void delay(volatile uint32_t count);

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