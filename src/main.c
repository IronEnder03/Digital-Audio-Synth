#include "stm32f4xx.h"

#include "lcd.h"
#include "util.h"

// USART2
void USART2_Init(void);
void USART2_Write(int ch);
void USART2_SendString(char*);

int main(void) {
    uint8_t frameBuffer[1024] = {0};

    SPI1_Init();
    LCD_Init();

    LCD_ClearGraphics(frameBuffer);

    LCD_DrawSineWave(16, 4, 0, 16, frameBuffer);

    LCD_UpdateScreen(frameBuffer);

    // Enable the clock for GPIO Port A
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    // Configure PA5 safely as an Output Pin for your LED
    GPIOA->MODER &= ~(0b11U << (5 * 2));
    GPIOA->MODER |= (0b1U << (5 * 2)); 

    uint8_t x_offset = 0;
    // Main Execution Loop
    while (1) {
        // Toggle the physical green user LED pin
        GPIOA->ODR ^= GPIO_ODR_OD5;

        //LCD_ClearGraphics(frameBuffer);

        //LCD_DrawSineWave(16, 4, x_offset, 16, frameBuffer);

        //LCD_UpdateScreen(frameBuffer);

        x_offset += 10;
        if (x_offset >= 128) x_offset = 0;

        delay(1000);
    }
}

void USART2_Init(void) {
    // 1. Turn on system clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;  
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN; 

    // 2. Configure PA2 (TX) and PA3 (RX) for Alternate Function
    GPIOA->MODER &= ~(GPIO_MODER_MODER2 | GPIO_MODER_MODER3);
    GPIOA->MODER |= (0x2U << GPIO_MODER_MODER2_Pos) | (0x2U << GPIO_MODER_MODER3_Pos);

    // 3. Set PA2 and PA3 to High Speed
    GPIOA->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED2 | GPIO_OSPEEDR_OSPEED3);
    GPIOA->OSPEEDR |= (0x2U << (2 * 2)) | (0x2U << (3 * 2));

    // 4. Map pins to Alternate Function 7 (AF7 = USART2) safely
    GPIOA->AFR[0] &= ~(0xFU << (2 * 4)); // Clear PA2 mapping
    GPIOA->AFR[0] |=  (0x7U << (2 * 4));  // Set PA2 to AF7

    GPIOA->AFR[0] &= ~(0xFU << (3 * 4)); // Clear PA3 mapping
    GPIOA->AFR[0] |=  (0x7U << (3 * 4));  // Set PA3 to AF7

    // 5. Set Baud Rate (115200 at default 16MHz clock)
    USART2->BRR = 0x008BU;
    
    // 6. Enable transmitter, receiver, and global UART block
    USART2->CR1 |= (USART_CR1_TE | USART_CR1_RE | USART_CR1_UE);
}

void USART2_Write(int ch) {
    // Wait ONLY until Transmit Data Register is empty (TXE)
    while (!(USART2->SR & USART_SR_TXE));
    USART2->DR = (ch & 0xFF);
}

void USART2_SendString(char *str) {
    while (*str) {
        USART2_Write(*str++);
    }
}

