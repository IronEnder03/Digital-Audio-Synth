#include "midi_input.h"
#include "audio_out.h"

uint8_t midi_rx_buffer[MIDI_BUF_SIZE] = {0};
Voice voices[MAX_VOICES] = {0};

void USART1_DMA_MIDI_Init(void) {
    // Turn on system clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;  
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

    // Configure PA9 (TX) and PA10 (RX) for Alternate Function
    GPIOA->MODER &= ~(GPIO_MODER_MODER9 | GPIO_MODER_MODER10);
    GPIOA->MODER |= (0x2U << GPIO_MODER_MODER9_Pos) | (0x2U << GPIO_MODER_MODER10_Pos);

    // Set PA9 and PA10 to High Speed
    GPIOA->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED9 | GPIO_OSPEEDR_OSPEED10);
    GPIOA->OSPEEDR |= (0x2U << (9 * 2)) | (0x2U << (10 * 2));

    // Map pins to Alternate Function 7 (AF7 = USART1) safely
    GPIOA->AFR[1] &= ~(0xFU << ((9 - 8) * 4)); // Clear PA9 mapping
    GPIOA->AFR[1] |=  (0x7U << ((9 - 8) * 4));  // Set PA9 to AF7

    GPIOA->AFR[1] &= ~(0xFU << ((10 - 8) * 4)); // Clear PA9 mapping
    GPIOA->AFR[1] |=  (0x7U << ((10 - 8) * 4));  // Set PA9 to AF7

    // Set Baud Rate (31,250 at default 16MHz clock) (Mantissa = 32, Fraction = 0)
    USART2->BRR = 0x0100U;

    // Enable DMA for reception
    USART2->CR3 |= (USART_CR3_DMAR);
    
    // Enable receiver and global UART block
    USART2->CR1 |= (USART_CR1_RE | USART_CR1_UE);

    // Ensure the stream is disabled before configuring
    DMA2_Stream2->CR &= ~DMA_SxCR_EN;
    while (DMA2_Stream2->CR & DMA_SxCR_EN);

    // Set Addresses and Length
    DMA2_Stream2->PAR = (uint32_t)&(USART1->DR);
    DMA2_Stream2->M0AR = (uint32_t)midi_rx_buffer;
    DMA2_Stream2->NDTR = MIDI_BUF_SIZE;

    // Configure DMA Stream Control Register
    DMA2_Stream2->CR = 0; 
    DMA2_Stream2->CR |= (4U << DMA_SxCR_CHSEL_Pos);  // Channel 4
    DMA2_Stream2->CR |= DMA_SxCR_MINC;               // Memory increment (on)
    DMA2_Stream2->CR &= ~(DMA_SxCR_PINC);            // No peripheral increment
    DMA2_Stream2->CR &= ~DMA_SxCR_MSIZE;             // Memory size: 8-bit (00)
    DMA2_Stream2->CR &= ~DMA_SxCR_PSIZE;             // Peripheral size: 8-bit (00)

    // Circular mode configuration
    DMA2_Stream2->CR |= DMA_SxCR_CIRC;    // Circular mode (auto-wraps around buffer)
    DMA2_Stream2->CR &= ~DMA_SxCR_DIR;    // Direction: Peripheral-to-memory (00)

    // Enable DMA
    DMA2_Stream2->CR |= DMA_SxCR_EN;
}

void Proccess_MIDI_Buffer(void) {
    static uint16_t readIndex = 0;

    // NDTR counts down
    uint16_t dmaWriteIndex = MIDI_BUF_SIZE - (DMA2_Stream2->NDTR);

    // Process all unread bytes until we catch up to the DMA write head
    while (readIndex != dmaWriteIndex) {
        uint8_t incomingByte = midi_rx_buffer[readIndex];
        
        // Advance read pointer with wrapping
        readIndex = (readIndex + 1) % MIDI_BUF_SIZE;
        
        // Pass the byte to our MIDI state machine parser
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


