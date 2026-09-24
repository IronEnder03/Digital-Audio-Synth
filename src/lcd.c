#include "lcd.h"
#include "util.h"

#include <math.h>
#include <stdlib.h>

#define LCD_WIDTH 128
#define LCD_HEIGHT 64
#define LCD_VERT_ADDR_WIDTH 32
#define LCD_HORZ_ADDR_WIDTH 16

// Pin Masks
#define RST_PIN    (1 << 0)   // PA0 (Kept for hardware reset control)
#define CS_PIN     (1 << 4)  // PA4
#define SCK_PIN    (1 << 5)  // PA5 (SPI1 SCK)
#define MOSI_PIN   (1 << 7)  // PA7 (SPI1 MOSI)

void SPI1_Init(void) {
    // Enable GPIO Port A, SPI1, and DMA2 clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA2EN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    // 2. Configure PA0 (RST) as General Purpose Output
    GPIOA->MODER &= ~(GPIO_MODER_MODER0); 
    GPIOA->MODER |=  (0x1U << GPIO_MODER_MODER0_Pos); 

    // Set GPIO pins to alternate function mode for SPI
    GPIOA->MODER &= ~(GPIO_MODER_MODER5 | GPIO_MODER_MODER7);
    GPIOA->MODER |= (0x2U << GPIO_MODER_MODER5_Pos) | (0x2U << GPIO_MODER_MODER7_Pos);

    // Set GPIO pin for Chip Select mode (General purpose output mode)
    GPIOA->MODER &= ~(GPIO_MODER_MODER4);
    GPIOA->MODER |= (0x1U << GPIO_MODER_MODER4_Pos);

    // Set GPIO pins to High Speed for stable SPI clock
    GPIOA->OSPEEDR |= (0x3U << GPIO_OSPEEDR_OSPEED5_Pos) | (0x3U << GPIO_OSPEEDR_OSPEED7_Pos);

    // Map pins to Alternate Function
    GPIOA->AFR[0] &= ~(0xF0F00000); // Clear setting
    GPIOA->AFR[0] |=  (5 << GPIO_AFRL_AFSEL5_Pos) | (5 << GPIO_AFRL_AFSEL7_Pos);  // Set setting

    /* ===== SPI Configuration ===== */
    //Reset SPI Config
    SPI1->CR1 = 0;

    // Configure serial clock baud rate (fPCLK / 8)
    SPI1->CR1 |= (2 << SPI_CR1_BR_Pos);

    // Clock phase and clock polarity (1 for both)
    SPI1->CR1 |= (SPI_CR1_CPOL | SPI_CR1_CPHA);

    // Set SPI to transmit MSB first
    SPI1->CR1 &= ~(SPI_CR1_LSBFIRST);

    // Enable Master configuration
    SPI1->CR1 |= SPI_CR1_MSTR;

    SPI1->CR1 |= (SPI_CR1_SSI | SPI_CR1_SSM);

    // Configure DMA2 Stream 3 for SPI1_TX
    DMA2_Stream3->CR = 0; // Ensure disabled before editing
    while(DMA2_Stream3->CR & DMA_SxCR_EN);

    DMA2_Stream3->PAR = (uint32_t)&(SPI1->DR);       // Peripheral data destination
    DMA2_Stream3->CR |= (3 << DMA_SxCR_CHSEL_Pos);   // Channel 3 is linked to SPI1_TX
    DMA2_Stream3->CR |= DMA_SxCR_DIR_0;              // Memory-to-peripheral direction
    DMA2_Stream3->CR |= DMA_SxCR_MINC;               // Automatically increment source memory address
    DMA2_Stream3->CR |= DMA_SxCR_PL_1;               // High priority allocation

    // Enable SPI with DMA
    SPI1->CR2 |= SPI_CR2_TXDMAEN;
    SPI1->CR1 |= SPI_CR1_SPE;

    // Enable Chip Select to LOW
    GPIOA->BSRR = 1 << (CS_PIN + 16);

    // RST High
    GPIOA->BSRR = RST_PIN;
}

void SPI1_SendByte(uint8_t data) {
    // Wait until TXE buffer is empty
    while (!(SPI1->SR & SPI_SR_TXE));
    SPI1->DR = data;
    // Wait until SPI1 is no longer busy
    while (SPI1->SR & SPI_SR_BSY);
}

void LCD_Write(uint8_t type, uint8_t value) {
    // Set CS to HIGH
    GPIOA->BSRR = CS_PIN;

    SPI1_SendByte(type);
    SPI1_SendByte(value & 0xF0); // High Nibble
    SPI1_SendByte((value << 4) & 0xF0); // Low Nibble

    // Set CS back to LOW
    GPIOA->BSRR = 1 << (CS_PIN + 16);

    delay(1);
}

void LCD_WriteCommand(uint8_t cmd) {
    LCD_Write(0xF8, cmd);
}

void LCD_WriteData(uint8_t data) {
    LCD_Write(0xFA, data);
}

void LCD_Init(void) {
    // Hardware Reset Sequence via PA0
    GPIOA->BSRR = 1 << (RST_PIN + 16); // RST Low
    delay(5);
    GPIOA->BSRR = RST_PIN;       // RST High
    delay(50);                // Wait for display driver block execution

    // Software initialization sequence
    LCD_WriteCommand(0x30); // 8-bit basic instruction set
    delay(1);
    LCD_WriteCommand(0x30); // Repeat for loop stability
    delay(1);
    LCD_WriteCommand(0x0C); // Display ON, Cursor OFF
    delay(1);
    LCD_WriteCommand(0x01); // Clear display RAM memory
    delay(15);             // Execution delay required for full screen wipe
    LCD_WriteCommand(0x06); // Cursor entry shift right
}

void LCD_UpdateScreen(uint8_t *frameBuffer) {
    // Extended mode (graphics off)
    LCD_WriteCommand(0x34);

    for (uint8_t y = 0; y < LCD_VERT_ADDR_WIDTH; y++) {

        // Top half refresh
        LCD_WriteCommand(0x80 | y);
        LCD_WriteCommand(0x80 | 0);
        for (uint8_t x = 0; x < LCD_HORZ_ADDR_WIDTH; x++) {
            LCD_WriteData(frameBuffer[x + (y *16)]); 
        }

        // Bottom half refresh
        LCD_WriteCommand(0x80 | y);
        LCD_WriteCommand(0x80 | 8);
        for (uint8_t x = 0; x < LCD_HORZ_ADDR_WIDTH; x++) {
            LCD_WriteData(frameBuffer[x + (y *16) + 512]); 
        }
    }

    // Turn graphics on
    LCD_WriteCommand(0x36);
}

void LCD_ClearGraphics(uint8_t *frameBuffer) {
    for (uint16_t i = 0; i < 1024; i++) {
        frameBuffer[i] = 0x00;
    }
}

void LCD_DrawPixel(uint8_t x, uint8_t y, bool set_pixel, uint8_t *frameBuffer) {
    // Boundary check
    if (x >= 128 || y >= 64) return;

    uint16_t byteIndex = (y << 4) + (x >> 3);

    uint8_t bitMask = 0x80 >> (x & 0x07);

    if (set_pixel) {
        frameBuffer[byteIndex] |= bitMask;   // Set pixel (Turn ON)
    } else {
        frameBuffer[byteIndex] &= ~bitMask;  // Clear pixel (Turn OFF)
    }
}

void LCD_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t *frameBuffer) {
    int16_t dx = abs(x1 - x0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = -abs(y1 - y0);
    int16_t sy = y0 < y1 ? 1 : -1; 
    int16_t error = dx + dy;
    
    while (true) {
        LCD_DrawPixel(x0, y0, true, frameBuffer);
        int16_t e2 = 2 * error;
        if (e2 >= dy) {
            if (x0 == x1) break;
            error = error + dy;
            x0 = x0 + sx;
        }

        if (e2 <= dx) {
            if (y0 == y1) break;
            error = error + dx;
            y0 = y0 + sy;
        }
    }
}

void LCD_DrawSineWave(uint8_t amplitude, uint8_t cycles, uint8_t x_offset, uint8_t y_offset, uint8_t *frameBuffer) {
    for (uint8_t x = 0; x < 128; x++) {
        uint8_t y = (uint8_t) (y_offset - (amplitude * sinf((2 * PI * cycles * (x - x_offset)) / 128)));
        LCD_DrawPixel(x, y, true, frameBuffer);
    }
}

void LCD_DrawSquareWave(uint8_t amplitude, uint8_t cycles, uint8_t *frameBuffer) {
    int8_t previousSign = 0;
    for (uint8_t x = 0; x < 128; x++) {
        int8_t sign = signf(sinf((2 * PI * cycles * x) / 128));
        uint8_t y = (uint8_t) (32 - (amplitude * sign));
        LCD_DrawPixel(x, y, true, frameBuffer);
        if (previousSign != sign && previousSign != 0) {
            LCD_DrawLine(x, y, x, y + (amplitude * sign * 2), frameBuffer);
        }
        previousSign = sign;
    }
}
