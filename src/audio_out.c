#include "audio_out.h"

#include "arm_math.h"

int16_t i2s_tx_buffer[I2S_BUFFER_SIZE] = {0};
uint32_t midi_fixed_phase_inc_lut[MAX_MIDI_NOTES];
int16_t sine_wave_lut[SINE_LUT_SIZE];

void I2S_DMA_Init(void) {
    // Enable GPIO Port B, I2S2, and DMA1 clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    
    // Disable PLLI2S and wait until it unlocks
    RCC->CR &= ~RCC_CR_PLLI2SON;
    while (RCC->CR & RCC_CR_PLLI2SRDY);

    // Select PLLI2S_R (00) as the clock source for SPI2/I2S2 (I2S1SRC)
    RCC->DCKCFGR &= ~RCC_DCKCFGR_I2S1SRC;

    // Configure PLLI2SCFGR for 192 MHz output
    // Use PLLI2SM = 8 for 16 MHz HSI (change 8U to 4U if using 8 MHz HSE)
    RCC->PLLI2SCFGR = (8U   << RCC_PLLI2SCFGR_PLLI2SM_Pos) |
                    (192U << RCC_PLLI2SCFGR_PLLI2SN_Pos) |
                    (2U   << RCC_PLLI2SCFGR_PLLI2SR_Pos);

    // Enable PLLI2S and wait for lock
    RCC->CR |= RCC_CR_PLLI2SON;
    while (!(RCC->CR & RCC_CR_PLLI2SRDY));


    // =========================================================================
    // REST OF YOUR CODE (PB9, PB10, PB15 PIN INITIALIZATIONS)
    // =========================================================================

    // 2. ISOLATED MODER MASKING: Modify ONLY pins 9, 10, and 15
    GPIOB->MODER &= ~( (3U << GPIO_MODER_MODER9_Pos)  | 
                       (3U << GPIO_MODER_MODER10_Pos) | 
                       (3U << GPIO_MODER_MODER15_Pos) );
    GPIOB->MODER |=  ( (2U << GPIO_MODER_MODER9_Pos)  | // Alternate Function
                       (2U << GPIO_MODER_MODER10_Pos) | 
                       (2U << GPIO_MODER_MODER15_Pos) );

    // 3. ISOLATED OSPEEDR MASKING: High Speed for audio lines
    GPIOB->OSPEEDR &= ~( (3U << GPIO_OSPEEDR_OSPEED9_Pos)  | 
                         (3U << GPIO_OSPEEDR_OSPEED10_Pos) | 
                         (3U << GPIO_OSPEEDR_OSPEED15_Pos) );
    GPIOB->OSPEEDR |=  ( (3U << GPIO_OSPEEDR_OSPEED9_Pos)  | 
                         (3U << GPIO_OSPEEDR_OSPEED10_Pos) | 
                         (3U << GPIO_OSPEEDR_OSPEED15_Pos) );

    // 4. FIXED: Clean, isolated mask clearing inside the AFR[1] High Register array slot
    // This removes ONLY the 4-bit fields for pins 9, 10, and 15. PB13 stays perfectly untouched!
    GPIOB->AFR[1] &= ~( (0xFU << ((9 - 8) * 4))  | 
                        (0xFU << ((10 - 8) * 4)) | 
                        (0xFU << ((15 - 8) * 4)) );

    // 5. FIXED: Assign exactly AF5 (I2S2) using literal bit shifts into AFR[1]
    GPIOB->AFR[1] |=  ( (5U << ((9 - 8) * 4))  | // PB9  -> I2S2_WS
                        (5U << ((10 - 8) * 4)) | // PB10 -> I2S2_CK
                        (5U << ((15 - 8) * 4)) );// PB15 -> I2S2_SD

    /* ===== I2S Configuration ===== */
    SPI2->I2SCFGR = 0;
    SPI2->I2SCFGR |= (0x1 << SPI_I2SCFGR_I2SMOD_Pos); // I2S Mode
    SPI2->I2SCFGR |= (0x2 << SPI_I2SCFGR_I2SCFG_Pos); // Master Transmit Mode

    // Set data  and channel length to 32-bit
    SPI2->I2SCFGR |= (0x2 << SPI_I2SCFGR_DATLEN_Pos);
    SPI2->I2SCFGR |= (0x1 << SPI_I2SCFGR_CHLEN_Pos);

    // Prescaler values set for fs = 48kHz
    SPI2->I2SPR = 0;
    SPI2->I2SPR |= (0x1 << SPI_I2SPR_ODD_Pos);
    SPI2->I2SPR |= (0x3E << SPI_I2SPR_I2SDIV_Pos);

    /* ===== DMA Configuration ===== */
    DMA1_Stream4->CR &= ~DMA_SxCR_EN;
    while (DMA1_Stream4->CR & DMA_SxCR_EN);

    DMA1_Stream4->PAR = (uint32_t)&(SPI2->DR);
    DMA1_Stream4->M0AR = (uint32_t)i2s_tx_buffer;
    DMA1_Stream4->NDTR = I2S_BUFFER_SIZE;

    DMA1_Stream4->CR = 0; 
    DMA1_Stream4->CR |= (0U << DMA_SxCR_CHSEL_Pos);   // Channel 0
    DMA1_Stream4->CR |= DMA_SxCR_MINC;                // Memory increment
    DMA1_Stream4->CR &= ~(DMA_SxCR_PINC);             // No peripheral increment
    DMA1_Stream4->CR |= (0x01 << DMA_SxCR_MSIZE_Pos); // 16-bit memory tracking
    DMA1_Stream4->CR |= (0x01 << DMA_SxCR_PSIZE_Pos); // 16-bit peripheral tracking
    DMA1_Stream4->CR |= DMA_SxCR_CIRC;                // Circular mode
    DMA1_Stream4->CR |= (0x01 << DMA_SxCR_DIR_Pos);   // Memory-to-peripheral

    DMA1_Stream4->CR |= DMA_SxCR_TCIE | DMA_SxCR_HTIE; // Enable interrupts

    NVIC_SetPriority(DMA1_Stream4_IRQn, 4); 
    NVIC_EnableIRQ(DMA1_Stream4_IRQn);

    // Enable TX DMA Request Generation inside the SPI peripheral
    SPI2->CR2 |= SPI_CR2_TXDMAEN;

    // Enable DMA Stream
    DMA1_Stream4->CR |= DMA_SxCR_EN;

    // Enable I2S peripheral
    SPI2->I2SCFGR |= (0x1 << SPI_I2SCFGR_I2SE_Pos);
}

// Global processing request flags
volatile bool audio_buffer_request = false;
volatile uint16_t audio_target_index = 0;

void DMA1_Stream4_IRQHandler(void) {
    // Half-transfer complete, second half of buffer is safe to write to
    if (DMA1->HISR & DMA_HISR_HTIF4) {
        DMA1->HIFCR = DMA_HIFCR_CHTIF4;
        audio_target_index = I2S_BUFFER_SIZE / 2; // Target the second half
        audio_buffer_request = true;              // Flag the main loop to process
    }

    // Full-transfer complete, first half of buffer is safe to write to
    if (DMA1->HISR & DMA_HISR_TCIF4) {
        DMA1->HIFCR = DMA_HIFCR_CTCIF4;
        audio_target_index = 0;                   // Target the first half
        audio_buffer_request = true;              // Flag the main loop to process
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
            voices[i].phase = 0;
            voices[i].phaseInc = midi_fixed_phase_inc_lut[noteNum];
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
            voices[i].phase = 0;
            break;
        }
    }
}

void Process_Audio(int16_t* i2s_buffer, uint16_t bufferLen, uint16_t startIndex) {
    for (int i = startIndex; i < bufferLen + startIndex; i += 2) {
        int32_t mixedSample = 0;
        for (int v = 0; v < MAX_VOICES; v++) {
            if (voices[v].active) {
                // Extract the active array index from the high 16 bits of the accumulator
                uint16_t lutIndex = (voices[v].phase >> 16) & (SINE_LUT_SIZE - 1);

                // Fetch the pre-computed sample value directly from your integer table
                int16_t rawTableSample = sine_wave_lut[lutIndex];

                // Apply velocity attenuation volume scaling cleanly
                int32_t voiceSample = (rawTableSample * voices[v].velocity) / 127;
                mixedSample += voiceSample;

                // FIXED-POINT INCREMENTATION: Add the pre-calculated step size 
                // The fraction automatically rolls over into the high bits cleanly
                voices[v].phase += voices[v].phaseInc;
            }
        }

        // Master Attenuation: Scale down the mixed result to prevent clipping 
        // down the road if many voices play at once.
        mixedSample = mixedSample / (MAX_VOICES / 2); 

        // Hard-clipping guard layer
        if (mixedSample > 32300)  mixedSample = 32300;
        if (mixedSample < -32300) mixedSample = -32300;

        int16_t final_output = (int16_t)mixedSample;

        // Output to I2S Buffer slots
        i2s_buffer[i]     = final_output; // Left
        i2s_buffer[i + 1] = final_output; // Right
    }
}

void Audio_Tables_Generate_On_Boot(void) {
    // 1. Automatically generate the 256-sample Sine Wave LUT
    for (int i = 0; i < SINE_LUT_SIZE; i++) {
        // Calculate the radian angle for this specific slot position
        float32_t radians = (2.0f * PI * i) / (float32_t)SINE_LUT_SIZE;
        
        // Scale a standard float sine wave up to safe 16-bit volume levels (~26000)
        sine_wave_lut[i] = (int16_t)(sinf(radians) * 26000.0f);
    }

    // 2. Automatically generate the 128-note Pitch Phase Increment LUT
    for (int note = 0; note < MAX_MIDI_NOTES; note++) {
        // Calculate the physical note frequency (Equal Temperament tuning formula)
        float32_t frequency = 440.0f * powf(2.0f, (note - 69) / 12.0f);
        
        // Convert the frequency step size directly into a Q16.16 fixed-point value
        float32_t continuousStep = (frequency * (float32_t)SINE_LUT_SIZE) / SAMPLING_FREQ_HZ;
        midi_fixed_phase_inc_lut[note] = (uint32_t)(continuousStep * 65536.0f);
    }
}

