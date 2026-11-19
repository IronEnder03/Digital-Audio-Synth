#include "stm32f4xx.h"

#include "lcd.h"
#include "util.h"
#include "audio_out.h"
#include "midi_input.h"
#include "controls.h"

#include <stdlib.h>

int main(void) {
    // Configure system clock for peripherals
    SystemClock_Boost_180MHz();

    // Allows the FPU to be utilized
    SCB->CPACR |= ((3UL << 10*2) | (3UL << 11*2)); // Enable CP10 & CP11 Full Access

    // Configure SysTick to trigger an interrupt every 1ms (1000 Hz)
    SysTick_Config(SystemCoreClock / 1000); 

    uint8_t frameBuffer[1024] = {0};

    Audio_Tables_Generate_On_Boot();

    GPIO_Init();
    SPI1_Init();
    LCD_Init();
    Hardware_Serial_Dual_Init();
    I2S_DMA_Init();

    LCD_ClearGraphics(frameBuffer);

    LCD_DrawSquareWave(16, 4, 0, 16, frameBuffer);

    LCD_UpdateScreen(frameBuffer);

    uint8_t x_offset = 0;
    uint32_t start = msTicks;

    // Start Watchdog Timer
    IWDG->KR = 0xCCCC;

    // Unlock IWDG_PR and IWDG_RLR registers
    IWDG->KR = 0X5555;

    // Set prescalar to Watchdog Timer (divider /8)
    IWDG->PR = 1;

    // Set counter reload value
    IWDG->RLR = 3999;

    // Wait for updates
    while (IWDG->SR != 0U) {

    }

    IWDG->KR = 0xAAAA;

    // Main Execution Loop
    while (1) {
        // Drain MIDI controller circular buffer
        Proccess_MIDI_Buffer();

        // Update LCD based on the current page the user is on
        /*
        Page 1: Display the currently selected base waveform input.
        Page 2: Displays info of the current effects settings.
        Page 3: Displays more info of the current effects settings.
        Page 4: Displays a live waveform of the output based on the digital audio calculated.
        */
        if (msTicks - start > 50) {
            LCD_ClearGraphics(frameBuffer);

            if (currentPage == 0) {
                switch(currentWaveForm) {
                    case SINE:
                        LCD_DrawSineWave(16, 4, x_offset, 16, frameBuffer);
                        break;
                    case SQUARE:
                        LCD_DrawSquareWave(16, 4, x_offset, 16, frameBuffer);
                        break;
                    case TRIANGLE:
                        LCD_DrawTriangleWave(16, 4, x_offset, 16, frameBuffer);
                        break;
                    case SAWTOOTH:
                        LCD_DrawSawToothWave(16, 4, x_offset, 16, frameBuffer);
                        break;
                    default:
                        break;
                }
            } else if (currentPage == 1) {
                char lineBuffer[26] = "Attack Time: ";
                __itoa((int)attackInput, &lineBuffer[13], 10);
                LCD_DrawString(0, 0, lineBuffer, frameBuffer);
                
                strcpy(lineBuffer, "Decay Time: ");
                __itoa((int)decayInput, &lineBuffer[12], 10);
                LCD_DrawString(0, 8, lineBuffer, frameBuffer);

                strcpy(lineBuffer, "Sustain Level: ");
                __itoa((int)sustainInput, &lineBuffer[15], 10);
                LCD_DrawString(0, 16, lineBuffer, frameBuffer);

                strcpy(lineBuffer, "Release Time: ");
                __itoa((int)releaseInput, &lineBuffer[14], 10);
                LCD_DrawString(0, 24, lineBuffer, frameBuffer);

                strcpy(lineBuffer, "Filter Frequency: ");
                __itoa((int)filterFreqInput, &lineBuffer[18], 10);
                LCD_DrawString(0, 32, lineBuffer, frameBuffer);

                strcpy(lineBuffer, "Filter Resonance: ");
                __itoa((int)filterResonanceInput, &lineBuffer[18], 10);
                LCD_DrawString(0, 40, lineBuffer, frameBuffer);

                strcpy(lineBuffer, "Filter Type: ");
                if (currentFilterMode == FILTER_LOWPASS) {
                    LCD_DrawString(0, 48, "Filter Type: LowPass", frameBuffer);
                } else if (currentFilterMode == FILTER_BANDPASS) {
                    LCD_DrawString(0, 48, "Filter Type: BandPass", frameBuffer);
                } else if (currentFilterMode == FILTER_HIGHPASS) {
                    LCD_DrawString(0, 48, "Filter Type: HighPass", frameBuffer);
                }
            } else if (currentPage == 2) {
                char lineBuffer[26] = "Chorus: ";
                __itoa((int)chorusInput, &lineBuffer[8], 10);
                LCD_DrawString(0, 0, lineBuffer, frameBuffer);
                
                strcpy(lineBuffer, "Delay: ");
                __itoa((int)delayInput, &lineBuffer[7], 10);
                LCD_DrawString(0, 8, lineBuffer, frameBuffer);

                strcpy(lineBuffer, "Reverb: ");
                __itoa((int)reverbInput, &lineBuffer[8], 10);
                LCD_DrawString(0, 16, lineBuffer, frameBuffer);

                strcpy(lineBuffer, "Oscill Detune: ");
                __itoa((int)oscDetuneInput, &lineBuffer[15], 10);
                LCD_DrawString(0, 24, lineBuffer, frameBuffer);

                strcpy(lineBuffer, "LFO Speed: ");
                __itoa((int)lfoSpeedInput, &lineBuffer[11], 10);
                LCD_DrawString(0, 32, lineBuffer, frameBuffer);

                strcpy(lineBuffer, "Auto Wah Depth: ");
                __itoa((int)autoWahDepthInput, &lineBuffer[16], 10);
                LCD_DrawString(0, 40, lineBuffer, frameBuffer);
            } else if (currentPage == 3) {
                uint8_t scopeIndex = 0;
                for (size_t i = 0; i < 128; i++) {
                    if (scopeBuffer[i - 1] < 0 && scopeBuffer[i] >= 0) {
                        scopeIndex = i;
                    }
                }

                for (size_t i = 0; i < 128; i++) {
                    LCD_DrawPixel(i, 40 - (scopeBuffer[scopeIndex+i] >> 10), true, frameBuffer);
                }
            }

            LCD_UpdateScreen(frameBuffer);

            x_offset += 2;
            if (x_offset >= 128) x_offset = 0;

            scopeReady = false;

            start = msTicks;
        }

        // Update Watchdog key to prevent resetting
        IWDG->KR = 0xAAAA;
    }
}

