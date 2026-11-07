#ifndef MIDI_INPUT_H
#define MIDI_INPUT_H

#include <stdbool.h>

#include "stm32f4xx.h"

#define MIDI_BUF_SIZE 64
extern uint8_t midi_rx_buffer[MIDI_BUF_SIZE];

#define MAX_VOICES 8
typedef struct Voice {
    bool active;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    float32_t phase;
} Voice;
Voice voices[MAX_VOICES];

typedef enum midi_state_t {
    MIDI_STATE_IDLE,
    MIDI_STATE_WAIT_DATA1,
    MIDI_STATE_WAIT_DATA2
} midi_state_t;

/**
 * @brief Initialize USART1 and DMA peripherals.
 * 
 */
void USART1_DMA_MIDI_Init(void);

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