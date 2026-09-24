#ifndef LCD_H
#define LCD_H

#include "stm32f4xx.h"
#include <stdbool.h>

/**
 * @brief Initialize SP1 with DMA2
 * 
 */
void SPI1_Init(void);

/**
 * @brief Send a byte of data through SPI1.
 * 
 * @param data The data to send to an external peripheral through SPI1.
 */
void SPI1_SendByte(uint8_t data);

/**
 * @brief Initialize connected LCD.
 * 
 */
void LCD_Init(void);

/**
 * @brief Update the display of the LCD with given frame buffer.
 * 
 * @param frameBuffer The frame buffer array that contains the byte data for the LCD.
 */
void LCD_UpdateScreen(uint8_t *frameBuffer);

/**
 * @brief Write byte data to the LCD through SPI1.
 * 
 * @param type Byte data that specifies what is being sent to the LCD (0xF8 = command, 0xFA = data).
 * @param value Byte value that contains either a command or data depending on the given type.
 */
void LCD_Write(uint8_t type, uint8_t value);

/**
 * @brief Write a command to the LCD.
 * 
 * @param cmd The byte command to send to the LCD.
 */
void LCD_WriteCommand(uint8_t cmd);

/**
 * @brief Write data to the LCD.
 * 
 * @param data The byte data to send to the LCD.
 */
void LCD_WriteData(uint8_t data);

/**
 * @brief Clear the given LCD frame buffer.
 * 
 * @param frameBuffer The frame buffer to clear.
 */
void LCD_ClearGraphics(uint8_t *frameBuffer);

/**
 * @brief Draw a single pixel either ON or OFF at given x, y coordinate.
 * 
 * @param x The x coordinate of the pixel to draw.
 * @param y The y coordinate of the pixel to draw.
 * @param set_pixel Turns pixel ON if true, OFF if false.
 * @param frameBuffer The frame buffer for drawing the pixel.
 */
void LCD_DrawPixel(uint8_t x, uint8_t y, bool set_pixel, uint8_t *frameBuffer);

/**
 * @brief Draw a line with given coordinates.
 * 
 * @param x1 The x coordinate of the starting point of the line.
 * @param y1 The y coordinate of the starting point of the line.
 * @param x2 The x coordinate of the ending point of the line.
 * @param y2 The y coordinate of the ending point of the line.
 * @param frameBuffer The frame buffer to update for drawing the line.
 */
void LCD_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t *frameBuffer);

/**
 * @brief Draw sine wave on the LCD.
 * 
 * @param amplitude 
 * @param cycles 
 * @param x_offset 
 * @param y_offset 
 * @param frameBuffer 
 */
void LCD_DrawSineWave(uint8_t amplitude, uint8_t cycles, uint8_t x_offset, uint8_t y_offset, uint8_t *frameBuffer);

/**
 * @brief Draw square wave on the LCD.
 * 
 * @param amplitude 
 * @param cycles 
 * @param frameBuffer 
 */
void LCD_DrawSquareWave(uint8_t amplitude, uint8_t cycles, uint8_t *frameBuffer);

#endif