#ifndef MIDI_INPUT_H
#define MIDI_INPUT_H

#include <stdbool.h>

#include "stm32f4xx.h"
#include "arm_math.h"

#define MIDI_BUF_SIZE 64
extern uint8_t midi_rx_buffer[MIDI_BUF_SIZE];

#define MAX_VOICES 8
typedef struct Voice {
    volatile bool active;
    volatile uint8_t channel;
    volatile uint8_t note;
    volatile uint8_t velocity;
    volatile uint32_t phase;
    volatile uint32_t phaseInc;
} Voice;
extern Voice volatile voices[MAX_VOICES];

typedef enum midi_state_t {
    MIDI_STATE_IDLE,
    MIDI_STATE_WAIT_DATA1,
    MIDI_STATE_WAIT_DATA2
} midi_state_t;


/**
 * @brief Initialize USART and DMA peripherals.
 * 
 */
void Hardware_Serial_Dual_Init(void);

void UART2_SendChar(char c);

void UART2_SendString(const char* str);

/**
 * @brief Process the data currently in the RX MIDI buffer. Should be in main loop.
 * 
 */
void Proccess_MIDI_Buffer(void);

/**
 * @brief Parse the given MIDI byte data and performing various 
 * actions on the synthesizer based on the data.
 * 
 * @param byte The byte data from the MIDI input being parsed.
 */
void Parse_MIDI_Byte(uint8_t byte);

#endif