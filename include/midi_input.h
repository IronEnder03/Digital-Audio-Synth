#ifndef MIDI_INPUT_H
#define MIDI_INPUT_H

#include <stdbool.h>

#include "stm32f4xx.h"
#include "arm_math.h"

// APB1 Peripheral Clock = 45 MHz (180 MHz SYSCLK / 4)
#define APB1_CLOCK_HZ 45000000UL

#define MIDI_BUF_SIZE 64
extern uint8_t midi_rx_buffer[MIDI_BUF_SIZE];

typedef enum EnvelStatus {
    ENV_IDLE,
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE,
    ENV_STEAL,
} EnvelStatus;

#define MAX_VOICES 8
typedef struct Voice {
    volatile bool active;
    volatile EnvelStatus envStatus;
    volatile float envLevel;
    volatile uint8_t channel;
    volatile uint8_t note;
    volatile uint8_t velocity;
    volatile uint32_t phase;
    volatile uint32_t phase2;
    volatile uint32_t phaseInc;
    volatile uint8_t age;
    volatile uint8_t pendingNote;
    volatile uint8_t pendingVelocity;
} Voice;
extern Voice volatile voices[MAX_VOICES];

typedef enum midi_state_t {
    MIDI_STATE_IDLE,
    MIDI_STATE_WAIT_DATA1,
    MIDI_STATE_WAIT_DATA2
} midi_state_t;

typedef enum midi_control_t {
    MIDI_KNOB1 = 74,
    MIDI_KNOB2 = 71,
    MIDI_KNOB3 = 76,
    MIDI_KNOB4 = 77,
    MIDI_KNOB5 = 93,
    MIDI_KNOB6 = 18,
    MIDI_KNOB7 = 19,
    MIDI_KNOB8 = 16,
    MIDI_FADER1 = 82,
    MIDI_FADER2 = 83,
    MIDI_FADER3 = 85,
    MIDI_FADER4 = 17,
    MIDI_MOD_WHEEL = 1,
    MIDI_KNOB_BTN = 115,
} midi_control_t;

extern volatile uint8_t attackInput;
extern volatile uint8_t decayInput;
extern volatile uint8_t sustainInput;
extern volatile uint8_t releaseInput;
extern volatile uint8_t filterFreqInput;
extern volatile uint8_t filterResonanceInput;
extern volatile uint8_t chorusInput;
extern volatile uint8_t delayInput;
extern volatile uint8_t reverbInput;
extern volatile uint8_t oscDetuneInput;
extern volatile uint8_t lfoSpeedInput;
extern volatile uint8_t autoWahDepthInput;
extern volatile uint8_t pitchVibDepthInput;


/**
 * @brief Initialize USART and DMA peripherals.
 * 
 */
void Hardware_Serial_Dual_Init(void);

/**
 * @brief Send character via UART2.
 * 
 * @param c The character to send via UART2.
 */
void UART2_SendChar(char c);

/**
 * @brief Send string via UART2.
 * 
 * @param str The string to send via UART2.
 */
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