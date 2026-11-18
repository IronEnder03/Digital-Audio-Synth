#include "lcd.h"
#include "util.h"

#include <math.h>
#include <stdlib.h>
#include "arm_math.h"

#define LCD_WIDTH 128
#define LCD_HEIGHT 64
#define LCD_VERT_ADDR_WIDTH 32
#define LCD_HORZ_ADDR_WIDTH 16

// Pin Masks
#define RST_PIN    (1 << 0)   // PA0 (Kept for hardware reset control)
#define CS_PIN     (1 << 4)  // PA4
#define SCK_PIN    (1 << 5)  // PA5 (SPI1 SCK)
#define MOSI_PIN   (1 << 7)  // PA7 (SPI1 MOSI)

// Each character is 5 vertical columns (bits 0..6 = top to bottom)
const uint8_t font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // ' '
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // '0'
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // '1'
    {0x42, 0x61, 0x51, 0x49, 0x46}, // '2'
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // '3'
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // '4'
    {0x27, 0x45, 0x45, 0x45, 0x39}, // '5'
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // '6'
    {0x01, 0x71, 0x09, 0x05, 0x03}, // '7'
    {0x36, 0x49, 0x49, 0x49, 0x36}, // '8'
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // '9'
    {0x00, 0x36, 0x36, 0x00, 0x00}, // ':'
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 'A'
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // 'B'
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 'C'
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 'D'
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 'E'
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // 'F'
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 'G'
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 'H'
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // 'I'
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // 'J'
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // 'K'
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // 'L'
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 'M'
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 'N'
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 'O'
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // 'P'
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 'Q'
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // 'R'
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 'S'
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // 'T'
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 'U'
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 'V'
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // 'W'
    {0x63, 0x14, 0x08, 0x14, 0x63}, // 'X'
    {0x07, 0x08, 0x70, 0x08, 0x07}, // 'Y'
    {0x61, 0x51, 0x49, 0x45, 0x43}  // 'Z'
};

volatile uint8_t currentPage = 0;

void SPI1_Init(void) {
    // Enable GPIO Port A, SPI1, and DMA2 clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    // Configure PA0 (RST) as General Purpose Output
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

    // Configure serial clock baud rate (fPCLK / 128)
    SPI1->CR1 |= (6U << SPI_CR1_BR_Pos);

    // Clock phase and clock polarity (1 for both)
    SPI1->CR1 |= (SPI_CR1_CPOL | SPI_CR1_CPHA);

    // Set SPI to transmit MSB first
    SPI1->CR1 &= ~(SPI_CR1_LSBFIRST);

    // Enable Master configuration
    SPI1->CR1 |= SPI_CR1_MSTR;

    SPI1->CR1 |= (SPI_CR1_SSI | SPI_CR1_SSM);

    // Enable SPI Transmit Buffer DMA Request
    SPI1->CR2 |= SPI_CR2_TXDMAEN;

    // Enable SPI
    SPI1->CR1 |= SPI_CR1_SPE;

    // Enable Chip Select to LOW
    GPIOA->BSRR = 1 << (CS_PIN + 16);

    // RST High
    GPIOA->BSRR = RST_PIN;

    // Enable DMA2 Stream 3 Interrupt in the NVIC
    NVIC_SetPriority(DMA2_Stream3_IRQn, 6); // Set a low priority
    NVIC_EnableIRQ(DMA2_Stream3_IRQn);

}

void SPI1_SendByte(uint8_t data) {
    // Wait until TXE buffer is empty
    while (!(SPI1->SR & SPI_SR_TXE));
    SPI1->DR = data;
    // Wait until SPI1 is no longer busy
    while (SPI1->SR & SPI_SR_BSY);
}

void SPI1_SendBuffer_DMA_IT(uint8_t *buffer, uint16_t length) {
    // Ensure the stream is disabled before configuring
    DMA2_Stream3->CR &= ~DMA_SxCR_EN;
    while (DMA2_Stream3->CR & DMA_SxCR_EN);

    // Clear all interrupt flags for Stream 3
    DMA2->LIFCR = (0x3DU << 22); // Clears FEIF3, DMEIF3, TEIF3, HTIF3, TCIF3

    // Set Addresses and Length
    DMA2_Stream3->PAR = (uint32_t)&(SPI1->DR);
    DMA2_Stream3->M0AR = (uint32_t)buffer;
    DMA2_Stream3->NDTR = length;

    // Configure DMA Stream Control Register
    DMA2_Stream3->CR = 0; 
    DMA2_Stream3->CR |= (3U << DMA_SxCR_CHSEL_Pos);  // Channel 3
    DMA2_Stream3->CR |= DMA_SxCR_MINC;               // Memory increment (on)
    DMA2_Stream3->CR |= (1U << DMA_SxCR_DIR_Pos);    // Memory-to-Peripheral
    
    // Enable Transfer Complete Interrupt (TCIE)
    DMA2_Stream3->CR |= DMA_SxCR_TCIE;               // <-- Enable the DMA interrupt

    // Pull Chip Select LOW right before transfer starts
    GPIOA->BSRR = 1 << (CS_PIN + 16); 

    // Enable the DMA Stream (CPU continues executing next lines immediately)
    DMA2_Stream3->CR |= DMA_SxCR_EN;
}

void DMA2_Stream3_IRQHandler(void) {
    // Check if the Transfer Complete Interrupt flag for Stream 3 is set
    if (DMA2->LISR & DMA_LISR_TCIF3) {
        
        // Clear the Transfer Complete interrupt flag
        DMA2->LIFCR = DMA_LIFCR_CTCIF3;
        
        // Wait until the final byte clears the SPI hardware shift register
        while (SPI1->SR & SPI_SR_BSY);
        
        // Pull Chip Select back to LOW (Idle state for ST7920 SPI mode)
        GPIOA->BSRR = (1 << (CS_PIN + 16));
    }
}


void LCD_Write(uint8_t type, uint8_t value) {
    // Set CS to HIGH
    GPIOA->BSRR = CS_PIN;

    SPI1_SendByte(type);
    SPI1_SendByte(value & 0xF0); // High Nibble
    SPI1_SendByte((value << 4) & 0xF0); // Low Nibble

    // Set CS back to LOW
    GPIOA->BSRR = 1 << (CS_PIN + 16);

    delay_ms(1);
}

void LCD_Write_DMA(uint8_t type, uint8_t value) {
    // Wait if a previous background DMA transfer is still actively running
    while (DMA2_Stream3->CR & DMA_SxCR_EN);

    // Use a static buffer so the memory remains valid after this function returns
    static uint8_t lcd_packet[3];

    // Pack the 3-byte protocol structure expected by the ST7920
    lcd_packet[0] = type;
    lcd_packet[1] = value & 0xF0;        // High Nibble
    lcd_packet[2] = (value << 4) & 0xF0; // Low Nibble

    // Trigger the non-blocking background DMA transmission
    // This function automatically pulls CS LOW and starts the hardware stream
    SPI1_SendBuffer_DMA_IT(lcd_packet, 3);
}

void LCD_WriteCommand(uint8_t cmd) {
    LCD_Write(0xF8, cmd);
}

void LCD_WriteData(uint8_t data) {
    LCD_Write(0xFA, data);
}

void LCD_WriteCommand_DMA(uint8_t cmd) {
    LCD_Write_DMA(0xF8, cmd);
}

void LCD_WriteData_DMA(uint8_t data) {
    LCD_Write_DMA(0xFA, data);
}

void LCD_Init(void) {
    // Hardware Reset Sequence via PA0
    GPIOA->BSRR = 1 << (RST_PIN + 16); // RST Low
    delay_ms(5);
    GPIOA->BSRR = RST_PIN;       // RST High
    delay_ms(50);                // Wait for display driver block execution

    // Software initialization sequence
    LCD_WriteCommand(0x30); // 8-bit basic instruction set
    delay_ms(1);
    LCD_WriteCommand(0x30); // Repeat for loop stability
    delay_ms(1);
    LCD_WriteCommand(0x0C); // Display ON, Cursor OFF
    delay_ms(1);
    LCD_WriteCommand(0x01); // Clear display RAM memory
    delay_ms(5);             // Execution delay required for full screen wipe
    LCD_WriteCommand(0x06); // Cursor entry shift right
    delay_ms(1);
    LCD_WriteCommand(0x36); // Turn graphics on
    delay_ms(2);
}

void LCD_UpdateScreen(uint8_t *frameBuffer) {
    for (uint8_t y = 0; y < LCD_VERT_ADDR_WIDTH; y++) {

        // Top half refresh
        LCD_WriteCommand_DMA(0x80 | y);
        delay_us(10);
        LCD_WriteCommand_DMA(0x80 | 0);
        delay_us(10);
        for (uint8_t x = 0; x < LCD_HORZ_ADDR_WIDTH; x++) {
            LCD_WriteData_DMA(frameBuffer[x + (y *16)]);
            delay_us(10);
        }

        // Bottom half refresh
        LCD_WriteCommand_DMA(0x80 | y);
        delay_us(10);
        LCD_WriteCommand_DMA(0x80 | 8);
        delay_us(10);
        for (uint8_t x = 0; x < LCD_HORZ_ADDR_WIDTH; x++) {
            LCD_WriteData_DMA(frameBuffer[x + (y *16) + 512]); 
            delay_us(10);
        }
    }
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

void LCD_DrawSineWave(uint8_t amplitude, uint8_t cycles, 
    uint8_t x_offset, uint8_t y_offset, uint8_t *frameBuffer) {
    for (uint8_t x = 0; x < 128; x++) {
        uint8_t y = (uint8_t) (y_offset - (amplitude * sinf((2 * PI * cycles * (x - x_offset)) / 128)));
        LCD_DrawPixel(x, y, true, frameBuffer);
    }
}

void LCD_DrawSquareWave(uint8_t amplitude, uint8_t cycles, 
    uint8_t x_offset, uint8_t y_offset, uint8_t *frameBuffer) {
    float initial_angle = (2.0f * PI * cycles * (-1 - x_offset)) / 128.0f;
    int8_t previousSign = signf(sinf(initial_angle));
    for (uint8_t x = 0; x < 128; x++) {
        int8_t sign = signf(sinf((2 * PI * cycles * (x - x_offset)) / 128));
        uint8_t y = (uint8_t) (y_offset - (amplitude * sign));
        LCD_DrawPixel(x, y, true, frameBuffer);
        if (previousSign != sign) {
            LCD_DrawLine(x, y, x, y + (amplitude * sign * 2), frameBuffer);
        }
        previousSign = sign;
    }
}

void LCD_DrawTriangleWave(uint8_t amplitude, uint8_t cycles, 
    uint8_t x_offset, uint8_t y_offset, uint8_t *frameBuffer) {
    for (uint8_t x = 0; x < 128; x++) {
        float normal_x = (float) (((float)x - (float)x_offset)/128);
        uint8_t y = (uint8_t) (y_offset - ((2*amplitude / PI) * asinf(sinf(2*PI*cycles*normal_x))));
        LCD_DrawPixel(x, y, true, frameBuffer);
    }
}

void LCD_DrawSawToothWave(uint8_t amplitude, uint8_t cycles, 
    uint8_t x_offset, uint8_t y_offset, uint8_t *frameBuffer) {
    int8_t previousY = -1;
    for (uint8_t x = 0; x < 128; x++) {
        uint8_t y = (uint8_t) (y_offset - (2*amplitude * ((((float)cycles * (float)(x - x_offset))/128) - floorf((((float)cycles * (float)(x - x_offset))/128) + 0.5f))));
        LCD_DrawPixel(x, y, true, frameBuffer);
        // Check if the y coordinate drops vertically and draw a line if so
        if (y > previousY && previousY != -1) {
            LCD_DrawLine(x, previousY, x, y, frameBuffer);
        }
        previousY = y;
    }
}

static uint8_t Get_Glyph_Index(char c) {
    if (c >= '0' && c <= ':') return (c - '0' + 1);
    if (c >= 'A' && c <= 'Z') return (c - 'A' + 12);
    if (c >= 'a' && c <= 'z') return (c - 'a' + 12); // Auto-capitalize
    return 0;
}

void LCD_DrawChar(uint8_t x, uint8_t y, char c, uint8_t *frameBuffer) {
    uint8_t idx = Get_Glyph_Index(c);
    for (uint8_t col = 0; col < 5; col++) {
        uint8_t line = font5x7[idx][col];
        for (uint8_t row = 0; row < 7; row++) {
            LCD_DrawPixel(x + col, y + row, (line >> row) & 0x01, frameBuffer);
        }
    }

    // 1-pixel blank spacer column between characters
    for (uint8_t row = 0; row < 7; row++) {
        LCD_DrawPixel(x + 5, y + row, 0, frameBuffer);
    }
}

void LCD_DrawString(uint8_t x, uint8_t y, const char* str, uint8_t *frameBuffer) {
    while(*str) {
        if (x + 5 >= 128) break; // Stop safely at right edge
        LCD_DrawChar(x, y, *str++, frameBuffer);
        x += 6; // 5px glyph + 1px spacing
    }
}

