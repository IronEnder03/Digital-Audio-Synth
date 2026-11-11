#include "midi_input.h"
#include "audio_out.h"

uint8_t midi_rx_buffer[MIDI_BUF_SIZE] = {0};
Voice volatile voices[MAX_VOICES] = {0};

void Hardware_Serial_Dual_Init(void) {
    // 1. Turn on all required system clocks
    RCC->AHB1ENR |= (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN);  // Enable GPIOA & GPIOC
    RCC->APB1ENR |= (RCC_APB1ENR_USART2EN | RCC_APB1ENR_USART3EN); // Enable USART2 & USART3
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;                            // Enable DMA1 
    
    // Configure the PLLI2S multipliers inside the clock configuration register
    // To generate a precise 48kHz audio clock sample rate from a standard 16MHz system clock:
    // Set PLLI2SN (multiplication factor) = 192 (Bits 14:6)
    // Set PLLI2SR (division factor) = 2 (Bits 30:28)
    RCC->PLLI2SCFGR = (192U << RCC_PLLI2SCFGR_PLLI2SN_Pos) | (2U << RCC_PLLI2SCFGR_PLLI2SR_Pos);

    // Safely route PLLI2S_R as the dedicated audio source clock layout for I2S2
    // Clear old clock selection bits (bits 23:22) in the Dedicated Clock Configuration Register
    RCC->DCKCFGR &= ~(3U << 22); 

    // Turn on the physical PLLI2S hardware engine block
    RCC->CR |= RCC_CR_PLLI2SON;

    // This will now exit instantly because the multipliers above give it a real signal to stabilize!
    while((RCC->CR & RCC_CR_PLLI2SRDY) == 0);

    // =========================================================================
    // GPIO CONFIGURATION: PA2 (USART2 TX) & PC11 (USART3 RX)
    // =========================================================================
    
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

    // =========================================================================
    // PERIPHERAL CONFIGURATION: USART2 (Debug TX) & USART3 (MIDI RX)
    // =========================================================================

    // Set USART2 to 115200 Baud for ultra-fast, clean text logging
    #if defined(SystemCoreClock) && (SystemCoreClock == 180000000 || SystemCoreClock == 168000000)
        // 42,000,000 / (16 * 115200) = 22.78 -> Mantissa = 22 (0x16), Fraction = 0.78 * 16 = 12 (0xC)
        USART2->BRR = (0x16U << 4) | 0xCU;
    #else
        // Fallback for default internal HSI 16MHz clock speed
        // 16,000,000 / (16 * 115200) = 8.68 -> Mantissa = 8, Fraction = 0.68 * 16 = 11 (0xB)
        USART2->BRR = (8U << 4) | 0xBU;
    #endif

    // Set USART3 to 31250 Baud for standard MIDI incoming frames
    #if defined(SystemCoreClock) && (SystemCoreClock == 180000000 || SystemCoreClock == 168000000)
        // 42,000,000 / (16 * 31,250) = 84.00 -> Mantissa = 84 (0x54), Fraction = 0
        USART3->BRR = (84U << 4);
    #else
        // Fallback for default internal HSI 16MHz clock speed
        // 16,000,000 / (16 * 31,250) = 32.00 -> Mantissa = 32 (0x20), Fraction = 0
        USART3->BRR = (32U << 4);
    #endif

    // Enable USART2 Transmitter only (for debugging console logs)
    USART2->CR1 |= (USART_CR1_TE | USART_CR1_UE);

    // Enable USART3 Receiver and activate its DMA Request path
    USART3->CR3 |= USART_CR3_DMAR;
    USART3->CR1 |= (USART_CR1_RE | USART_CR1_UE);

    // =========================================================================
    // DMA CONFIGURATION: DMA1 Stream 1 Channel 4 (USART3_RX Hardware Map)
    // =========================================================================
    
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

    // Drop real-time messages
    if (byte >= 0xF8) return;

    // Status byte
    if (byte & 0x80) {
        uint8_t msgType = byte & 0xF0;

        // Only except Note On and Note Off status messages
        if (msgType == 0x90 || msgType == 0x80) {
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
            noteNumber = byte;
            currentState = MIDI_STATE_WAIT_DATA2;
            break;
        case MIDI_STATE_WAIT_DATA2:
            uint8_t velocity = byte;
            uint8_t msgType = currentStatus & 0xF0;
            uint8_t channel = (currentStatus & 0x0F) + 1;

            if (msgType == 0x90 && velocity > 0) {
                // Key Pressed
                Synth_Play_Note(channel, noteNumber, velocity);
            } 
            else {
                // Key Released (Handles both 0x80 and 0x90 with Vel 0)
                Synth_Stop_Note(channel, noteNumber);
            }

            currentState = MIDI_STATE_IDLE; // Ready for next pair of bytes via running status
            break;

        default:
            currentState = MIDI_STATE_IDLE;
    }
}


