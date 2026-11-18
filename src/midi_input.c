#include "midi_input.h"
#include "audio_out.h"

#include <stdlib.h>

uint8_t midi_rx_buffer[MIDI_BUF_SIZE] = {0};
Voice volatile voices[MAX_VOICES] = {0};

volatile uint8_t attackInput = 0;
volatile uint8_t decayInput = 0;
volatile uint8_t sustainInput = 0;
volatile uint8_t releaseInput = 0;
volatile uint8_t filterFreqInput = 0;
volatile uint8_t filterResonanceInput = 0;
volatile uint8_t chorusInput = 0;
volatile uint8_t delayInput = 0;
volatile uint8_t reverbInput = 0;
volatile uint8_t oscDetuneInput = 0;
volatile uint8_t lfoSpeedInput = 0;
volatile uint8_t autoWahDepthInput = 0;
volatile uint8_t pitchVibDepthInput = 0;

void Hardware_Serial_Dual_Init(void) {
    // Turn on all required system clocks
    RCC->AHB1ENR |= (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN);  // Enable GPIOA & GPIOC
    RCC->APB1ENR |= (RCC_APB1ENR_USART2EN | RCC_APB1ENR_USART3EN); // Enable USART2 & USART3
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;                            // Enable DMA1 
    
    // Configure the PLLI2S multipliers inside the clock configuration register
    RCC->PLLI2SCFGR = (192U << RCC_PLLI2SCFGR_PLLI2SN_Pos) | (2U << RCC_PLLI2SCFGR_PLLI2SR_Pos);

    // Safely route PLLI2S_R as the dedicated audio source clock layout for I2S2
    RCC->DCKCFGR &= ~(3U << 22); 

    // Turn on the physical PLLI2S hardware engine block
    RCC->CR |= RCC_CR_PLLI2SON;

    // This will now exit instantly because the multipliers above give it a real signal to stabilize!
    while((RCC->CR & RCC_CR_PLLI2SRDY) == 0);

    // GPIO CONFIGURATION: PA2 (USART2 TX) & PC11 (USART3 RX)
    // Configure PA2 for Alternate Function mode
    GPIOA->MODER &= ~GPIO_MODER_MODER2;
    GPIOA->MODER |= (0x2U << GPIO_MODER_MODER2_Pos);
    GPIOA->OSPEEDR |= (0x2U << GPIO_OSPEEDR_OSPEED2_Pos); // High Speed
    GPIOA->AFR[0] &= ~(0xFU << (2 * 4)); 
    GPIOA->AFR[0] |=  (0x7U << (2 * 4));                 // Map PA2 to AF7 (USART2 TX)

    // Configure PC11 for Alternate Function mode
    GPIOC->MODER &= ~GPIO_MODER_MODER11;
    GPIOC->MODER |= (0x2U << GPIO_MODER_MODER11_Pos);
    GPIOC->OSPEEDR |= (0x2U << GPIO_OSPEEDR_OSPEED11_Pos); // High Speed
    GPIOC->AFR[1] &= ~(0xFU << ((11 - 8) * 4));
    GPIOC->AFR[1] |=  (0x7U << ((11 - 8) * 4));           // Map PC11 to AF7 (USART3 RX)

    // PERIPHERAL CONFIGURATION: USART2 (Debug TX) & USART3 (MIDI RX)
    // USART2: 115,200 Baud (Debug / Serial Output)
    // 45,000,000 / 115,200 = 390.625 -> rounds to 391 (0x0187)
    USART2->BRR = 0x0187; // 391

    // USART3: 31,250 Baud (MIDI Input)
    // 45,000,000 / 31,250 = 1440 (0x05A0) exact
    USART3->BRR = 0x05A0; // 1440

    // Enable USART2 Transmitter only (for debugging console logs)
    USART2->CR1 |= (USART_CR1_TE | USART_CR1_UE);

    // Enable USART3 Receiver and activate its DMA Request path
    USART3->CR3 |= USART_CR3_DMAR;
    USART3->CR1 |= (USART_CR1_RE | USART_CR1_UE);

    // DMA CONFIGURATION: DMA1 Stream 1 Channel 4 (USART3_RX Hardware Map) 
    // Ensure Stream 1 is fully disabled before changing settings
    DMA1_Stream1->CR &= ~DMA_SxCR_EN;
    while (DMA1_Stream1->CR & DMA_SxCR_EN);

    // Bind hardware registers and user memory array variables
    DMA1_Stream1->PAR = (uint32_t)&(USART3->DR);
    DMA1_Stream1->M0AR = (uint32_t)midi_rx_buffer;
    DMA1_Stream1->NDTR = MIDI_BUF_SIZE;

    // Build the register configuration properties directly
    DMA1_Stream1->CR = 0; 
    DMA1_Stream1->CR |= (4U << DMA_SxCR_CHSEL_Pos);  // Channel 4 maps cleanly to USART3_RX
    DMA1_Stream1->CR |= DMA_SxCR_MINC;               // Increment memory layout
    DMA1_Stream1->CR &= ~DMA_SxCR_PINC;              // Fixed peripheral destination
    DMA1_Stream1->CR &= ~DMA_SxCR_MSIZE;             // 8-bit memory tracking
    DMA1_Stream1->CR &= ~DMA_SxCR_PSIZE;             // 8-bit peripheral tracking
    DMA1_Stream1->CR |= DMA_SxCR_CIRC;               // Circular mode active
    DMA1_Stream1->CR &= ~DMA_SxCR_DIR;               // Peripheral-to-memory destination

    // Clear legacy hardware interrupt state flags before startup
    DMA1->LIFCR = DMA_LIFCR_CTCIF1 | DMA_LIFCR_CHTIF1 | DMA_LIFCR_CTEIF1 | DMA_LIFCR_CDMEIF1 | DMA_LIFCR_CFEIF1;

    // Turn Stream 1 on to begin monitoring the input pipe in the background
    DMA1_Stream1->CR |= DMA_SxCR_EN;
}

void UART2_SendChar(char c) {
    // Wait until the Transmit Data Register is completely empty and ready (TXE)
    while (!(USART2->SR & USART_SR_TXE));
    // Write character byte straight to data register
    USART2->DR = c;
}

void UART2_SendString(const char* str) {
    while (*str) {
        UART2_SendChar(*str++);
    }
}

void Proccess_MIDI_Buffer(void) {
    static volatile uint16_t readIndex = 0;

    // Snapshot current write tail outside condition check loop to secure pointer data
    uint16_t dmaWriteIndex = MIDI_BUF_SIZE - (DMA1_Stream1->NDTR);

    while (readIndex != dmaWriteIndex) {
        uint8_t incomingByte = midi_rx_buffer[readIndex];
        
        // Inline incremental wrapping bounds calculation
        readIndex++;
        if (readIndex >= MIDI_BUF_SIZE) {
            readIndex = 0;
        }
        
        Parse_MIDI_Byte(incomingByte);
    }
}

void Parse_MIDI_Byte(uint8_t byte) {
    static midi_state_t currentState = MIDI_STATE_IDLE;
    static uint8_t currentStatus = 0;
    static uint8_t runningStatus = 0;
    static uint8_t noteNumber = 0;
    static uint8_t cc = 0;

    // Drop real-time messages
    if (byte >= 0xF8) return;

    // Status byte
    if (byte & 0x80) {
        uint8_t msgType = byte & 0xF0;

        // Only except Note On, Note Off and CC status messages
        if (msgType == 0x90 || msgType == 0x80 || msgType == 0xB0) {
            currentStatus = byte;
            runningStatus = byte;
            currentState = MIDI_STATE_WAIT_DATA1;
        } else {
            // Drop any other status
            currentState = MIDI_STATE_IDLE;
            runningStatus = 0;
        }
        return;
    }

    // Data byte
    if (currentState == MIDI_STATE_IDLE && runningStatus != 0) {
        currentStatus = runningStatus;
        currentState = MIDI_STATE_WAIT_DATA1;
    }

    switch (currentState) {
        case MIDI_STATE_WAIT_DATA1:
            if (((currentStatus & 0xF0) == 0x90) || ((currentStatus & 0xF0) == 0x80)) {
                noteNumber = byte;
            } else {
                cc = byte;
            }
            currentState = MIDI_STATE_WAIT_DATA2;
            break;
        case MIDI_STATE_WAIT_DATA2:
            uint8_t velocity = byte;
            uint8_t cc_data = byte;
            uint8_t msgType = currentStatus & 0xF0;
            uint8_t channel = (currentStatus & 0x0F) + 1;

            if (msgType == 0x90 && velocity > 0) {
                // Key Pressed
                Synth_Play_Note(channel, noteNumber, velocity);
            } 
            else if (msgType == 0x80 || (msgType == 0x90 && velocity == 0)){
                // Key Released (Handles both 0x80 and 0x90 with Vel 0)
                Synth_Stop_Note(channel, noteNumber);
            } else if (msgType == 0xB0) {
                float norm = (float)cc_data / 127.0f;
                float cutoffHz = 40.0f + (norm * norm) * 15960.0f;
                switch (cc)
                {
                case MIDI_KNOB1:
                    attackTime = (norm * norm * 2.0f) + 0.005f;
                    attackInput = cc_data;
                    break;
                case MIDI_KNOB2:
                    decayTime = (norm * norm * 2.0f) + 0.005f;
                    decayInput = cc_data;
                    break;
                case MIDI_KNOB3:
                    sustainLevel = ((float)cc_data / 127.0f);
                    sustainInput = cc_data;
                    break;
                case MIDI_KNOB4:
                    releaseTime = (norm * norm * 2.0f) + 0.005f;
                    releaseInput = cc_data;
                    break;
                case MIDI_KNOB5:
                    svf_f = 2.0f * sinf((PI * cutoffHz) / (2.0f * SAMPLING_FREQ_HZ));
                    filterFreqInput = cc_data;
                    break;
                case MIDI_KNOB6:
                    svf_q = 1.0f - ((float)cc_data / 127.0f) * 0.96f;
                    filterResonanceInput = cc_data;
                    break;
                case MIDI_KNOB7:
                    chorusMix = norm * 0.45f;
                    chorusDepth = 30.0f + (norm * 140.0f);
                    chorusInput = cc_data;
                    break;
                case MIDI_KNOB8:
                    delayMix = norm * 0.50f;
                    delayFeedback = norm * 0.65f;
                    delayInput = cc_data;
                    break;
                case MIDI_FADER1:
                    reverbMix = norm * 0.45f;
                    reverbDecay = 0.65f + (norm * 0.21f);
                    reverbInput = cc_data;
                    break;
                case MIDI_FADER2:
                    detuneRatio = (norm * norm) * 0.015f;
                    oscDetuneInput = cc_data;
                    break;
                case MIDI_FADER3:
                    lfoRateHz = 0.2f + (norm * norm) * 11.8f;
                    lfoSpeedInput = cc_data;
                    break;
                case MIDI_FADER4:
                    autoWahDepth = (norm * norm) * 0.35f;
                    autoWahDepthInput = cc_data;
                    break;
                case MIDI_MOD_WHEEL:
                    vibratoDepth = norm * 0.02f;
                    pitchVibDepthInput = cc_data;
                    break;
                case MIDI_KNOB_BTN:
                    if (cc_data > 0) currentFilterMode++;
                    if (currentFilterMode > 2) currentFilterMode = FILTER_LOWPASS;
                    break;
                default:
                    break;
                }
            }

            currentState = MIDI_STATE_IDLE; // Ready for next pair of bytes via running status
            break;

        default:
            currentState = MIDI_STATE_IDLE;
    }
}


