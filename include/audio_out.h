#ifndef AUDIO_OUT_H
#define AUDIO_OUT_H

#include "stm32f4xx.h"

#include "midi_input.h"

#define SAMPLING_FREQ_HZ 48000.0f

#define I2S_BUFFER_SIZE 512
extern uint16_t i2s_tx_buffer[I2S_BUFFER_SIZE];

/**
 * @brief Initialize I2S and DMA peripherals for outputting digital audio data.
 * 
 */
void I2S_DMA_Init();

/**
 * @brief Play note being inputted.
 * 
 * @param channel The voice channel to play on.
 * @param noteNum The numerical representation of the note being played.
 * @param velocity The velocity the note being played.
 */
void Synth_Play_Note(uint8_t channel, uint8_t noteNum, uint8_t velocity);

/**
 * @brief Stop note being played on the synthesizer.
 * 
 * @param channel The channel the note is playing on.
 * @param noteNum The note to stop.
 */
void Synth_Stop_Note(uint8_t channel, uint8_t noteNum);

/**
 * @brief Process the current I2C buffer and output data.
 * 
 * @param i2s_buffer The I2S buffer where the digital audio data will be stored in.
 * @param bufferLen The length of the I2S buffer.
 * @param startIndex The start index to start reading the buffer from.
 */
void Process_Audio(uint16_t* i2s_buffer, uint16_t bufferLen,uint16_t startIndex);

#endif