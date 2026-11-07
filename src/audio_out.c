#include "audio_out.h"

#include "arm_math.h"

uint16_t i2s_tx_buffer[I2S_BUFFER_SIZE] = {0};

void I2S_DMA_Init() {
    // Enable GPIO Port B, I2S2, and DMA1 clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

    // Set GPIO pins PB9, PB10, and PB15 to alternate function mode for I2S
    GPIOA->MODER &= ~(GPIO_MODER_MODER9 | GPIO_MODER_MODER10 | GPIO_MODER_MODER15);
    GPIOA->MODER |= (0x2U << GPIO_MODER_MODER9_Pos) | 
    (0x2U << GPIO_MODER_MODER10_Pos) | (0x2U << GPIO_MODER_MODER15_Pos);

    // Set GPIO pins to high speed
    GPIOB->OSPEEDR |= (0x3U << GPIO_OSPEEDR_OSPEED9_Pos) | 
    (0x3U << GPIO_OSPEEDR_OSPEED10_Pos) | (0x3U << GPIO_OSPEEDR_OSPEED15_Pos);

    // Map pins to Alternate Function
    GPIOA->AFR[1] &= ~(0xF0F00000); // Clear setting
    GPIOA->AFR[1] |=  (5 << GPIO_AFRH_AFSEL9_Pos) | 
    (5 << GPIO_AFRH_AFSEL10_Pos) | (5 << GPIO_AFRH_AFSEL15_Pos);  // Set setting

    /* ===== I2S Configuration ===== */
    // Reset I2S Config
    SPI2->I2SCFGR = 0;

    // Set peripheral to I2S mode
    SPI2->I2SCFGR |= (0x1 << SPI_I2SCFGR_I2SMOD_Pos);

    // Set to Master - transmit mode
    SPI2->I2SCFGR |= (0x2 << SPI_I2SCFGR_I2SCFG_Pos);

    // Reset I2S prescalar register
    SPI2->I2SPR = 0;

    // Prescalar values set for fs = 48kHz
    // Set odd factor prescalar
    SPI2->I2SPR |= (0X1 << SPI_I2SPR_ODD_Pos);

    // Set linear prescalar value
    SPI2->I2SPR |= (0XF << SPI_I2SPR_I2SDIV_Pos);

    /* ===== DMA Configuration ===== */
    // Ensure the stream is disabled before configuring
    DMA1_Stream4->CR &= ~DMA_SxCR_EN;
    while (DMA1_Stream4->CR & DMA_SxCR_EN);

    // Set Addresses and Length
    DMA1_Stream4->PAR = (uint32_t)&(SPI2->DR);
    DMA1_Stream4->M0AR = (uint32_t)i2s_tx_buffer;
    DMA1_Stream4->NDTR = I2S_BUFFER_SIZE;

    // Configure DMA Stream Control Register
    DMA1_Stream4->CR = 0; 
    DMA1_Stream4->CR |= (0U << DMA_SxCR_CHSEL_Pos);          // Channel 0
    DMA1_Stream4->CR |= DMA_SxCR_MINC;                       // Memory increment (on)
    DMA1_Stream4->CR &= ~(DMA_SxCR_PINC);                    // No peripheral increment
    DMA1_Stream4->CR |= (0x01 << DMA_SxCR_MSIZE_Pos);        // Memory size: 16-bit (01)
    DMA1_Stream4->CR |= (0x01 << DMA_SxCR_PSIZE_Pos);        // Peripheral size: 16-bit (01)

    // Circular mode configuration
    DMA1_Stream4->CR |= DMA_SxCR_CIRC;    // Circular mode (auto-wraps around buffer)
    DMA1_Stream4->CR |= (0x01 << DMA_SxCR_DIR_Pos);    // Direction: Memory-to-peripheral (01)

    // Enable transfer tracking interrupts
    DMA1_Stream4->CR |= DMA_SxCR_TCIE | DMA_SxCR_HTIE;

    NVIC_SetPriority(DMA1_Stream4_IRQn, 4); // Keep audio processing priority tight
    NVIC_EnableIRQ(DMA1_Stream4_IRQn);

    // Enable DMA
    DMA1_Stream4->CR |= DMA_SxCR_EN;

    // Enable I2S peripheral
    SPI2->I2SCFGR |= (0x1 << SPI_I2SCFGR_I2SE_Pos);
}

void DMA1_Stream4_IRQHandler(void) {
    // Half-transfer complete, second half of buffer is safe to write to
    if (DMA1->HISR & DMA_HISR_HTIF4) {
        DMA1->HIFCR = DMA_HIFCR_CHTIF4;
        Process_Audio(i2s_tx_buffer, I2S_BUFFER_SIZE/2, I2S_BUFFER_SIZE/2);
    }

    // Full-transfer complete, first half of buffer is safe to write to
    if (DMA1->HISR & DMA_HISR_TCIF4) {
        DMA1->HIFCR = DMA_HIFCR_CTCIF4;
        Process_Audio(i2s_tx_buffer, I2S_BUFFER_SIZE/2, 0);
    }
}

void Synth_Play_Note(uint8_t channel, uint8_t noteNum, uint8_t velocity) {
    // Find open voice and set values
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active == true) {
            continue;
        } else {
            voices[i].active = true;
            voices[i].channel = channel;
            voices[i].note = noteNum;
            voices[i].velocity = velocity;
            break;
        }
    }
}

void Synth_Stop_Note(uint8_t channel, uint8_t noteNum) {
    // Find voice that has matching channel and note, and deactivate it
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].channel == channel && voices[i].note == noteNum) {
            voices[i].active = false;
            voices[i].channel = 0;
            voices[i].note = 0;
            voices[i].velocity = 0;
            break;
        }
    }
}

void Process_Audio(uint16_t* i2s_buffer, uint16_t bufferLen, uint16_t startIndex) {
    for (int i = startIndex; i < bufferLen + startIndex; i++) {
        float32_t mixedSample = 0.0f;
        for (int v = 0; v < MAX_VOICES; v++) {
            if (voices[v].active) {
                // Calculate this voice's unique current sample value
                float32_t voiceSample = arm_sin_f32(voices[v].phase) * ((voices[v].velocity / 127) * 20000.0f);
                mixedSample += voiceSample;

                // Advance this specific voice's phase independently
                voices[v].phase += 440 * pow(2.0f, (voices[v].note - 69)/12);
                if (voices[v].phase >= (2.0f * PI))
                {
                    voices[v].phase -= (2.0f * PI);
                }
            }
        }

        // Master Attenuation: Scale down the mixed result to prevent clipping 
        // down the road if many voices play at once.
        mixedSample = mixedSample / (MAX_VOICES / 2.0f); 

        // Hard-clipping guard layer
        if (mixedSample > 32767.0f)  mixedSample = 32767.0f;
        if (mixedSample < -32768.0f) mixedSample = -32768.0f;

        int16_t final_output = (int16_t)mixedSample;

        // Output to I2S Buffer slots
        i2s_buffer[i]     = final_output; // Left
        i2s_buffer[i + 1] = final_output; // Right
    }
}
