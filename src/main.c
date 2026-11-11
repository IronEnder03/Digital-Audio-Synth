#include "stm32f4xx.h"

#include "lcd.h"
#include "util.h"
#include "audio_out.h"
#include "midi_input.h"

int main(void) {
    // Configure SysTick to trigger an interrupt every 1ms (1000 Hz)
    SysTick_Config(SystemCoreClock / 1000); 

    uint8_t frameBuffer[1024] = {0};

    Audio_Tables_Generate_On_Boot();

    SPI1_Init();
    LCD_Init();
    Hardware_Serial_Dual_Init();
    I2S_DMA_Init();

    LCD_ClearGraphics(frameBuffer);

    LCD_DrawSquareWave(16, 4, 0, 16, frameBuffer);

    LCD_UpdateScreen(frameBuffer);

    uint8_t x_offset = 0;
    uint32_t start = msTicks;

    // Main Execution Loop
    while (1) {
        // 1. Drain your MIDI controller circular buffer
        Proccess_MIDI_Buffer();

        // 2. Process audio ONLY when the hardware requests a half-buffer swap
        if (audio_buffer_request) {
            audio_buffer_request = false; // Reset the flag
            Process_Audio((int16_t*)i2s_tx_buffer, I2S_BUFFER_SIZE / 2, audio_target_index);
        }

        // Update LCD every 400 milliseconds
        if (msTicks - start > 400) {
            LCD_ClearGraphics(frameBuffer);

            LCD_DrawSquareWave(16, 4, x_offset, 16, frameBuffer);

            LCD_UpdateScreen(frameBuffer);

            x_offset += 10;
            if (x_offset >= 128) x_offset = 0;
        }
    }
}

