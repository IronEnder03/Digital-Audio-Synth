#include "stm32f4xx.h"

#include "lcd.h"
#include "util.h"
#include "audio_out.h"
#include "midi_input.h"

int main(void) {
    // Configure SysTick to trigger an interrupt every 1ms (1000 Hz)
    SysTick_Config(SystemCoreClock / 1000); 

    uint8_t frameBuffer[1024] = {0};

    SPI1_Init();
    LCD_Init();
    USART1_DMA_MIDI_Init();
    I2S_DMA_Init();

    LCD_ClearGraphics(frameBuffer);

    LCD_DrawSquareWave(16, 4, 0, 16, frameBuffer);

    LCD_UpdateScreen(frameBuffer);

    uint8_t x_offset = 0;
    uint32_t start = msTicks;

    // Main Execution Loop
    while (1) {
        // Update LCD every 400 milliseconds
        if (msTicks - start > 400) {
            LCD_ClearGraphics(frameBuffer);

            LCD_DrawSquareWave(16, 4, x_offset, 16, frameBuffer);

            LCD_UpdateScreen(frameBuffer);

            x_offset += 10;
            if (x_offset >= 128) x_offset = 0;
        }

        // Read from MIDI input and send digital audio data based on input
        Proccess_MIDI_Buffer();
    }
}

