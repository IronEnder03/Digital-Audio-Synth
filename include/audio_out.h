#ifndef AUDIO_OUT_H
#define AUDIO_OUT_H

#include "stm32f4xx.h"

#include "midi_input.h"

#define SAMPLING_FREQ_HZ 48000.0f

#define I2S_BUFFER_SIZE 2048
extern int16_t i2s_tx_buffer[I2S_BUFFER_SIZE];
extern volatile bool audio_buffer_request;
extern volatile uint16_t audio_target_index;

typedef enum WaveForm {
    SINE,
    SQUARE,
    TRIANGLE,
    SAWTOOTH,
} WaveForm;

typedef enum FilterMode {
    FILTER_LOWPASS,
    FILTER_BANDPASS,
    FILTER_HIGHPASS,
} FilterMode;

#define MAX_MIDI_NOTES 128
#define WAVE_LUT_SIZE 2048
extern volatile WaveForm currentWaveForm;
extern uint32_t midi_fixed_phase_inc_lut[MAX_MIDI_NOTES];
extern int16_t sine_wave_lut[WAVE_LUT_SIZE];
extern int16_t square_wave_lut[WAVE_LUT_SIZE];
extern int16_t triangle_wave_lut[WAVE_LUT_SIZE];
extern int16_t sawtooth_wave_lut[WAVE_LUT_SIZE];

extern volatile float attackTime;
extern volatile float decayTime;
extern volatile float releaseTime;
extern volatile float sustainLevel;
extern volatile float svf_f;
extern volatile float svf_q;
extern volatile FilterMode currentFilterMode;
extern volatile float chorusMix;
extern volatile float chorusDepth;
extern volatile float delayMix;
extern volatile float delayFeedback;
#define DELAY_BUF_SIZE 16384
extern volatile float reverbMix;
extern volatile float reverbDecay;
#define COMB1_LEN 1427
#define COMB2_LEN 1781
#define COMB3_LEN 1973
#define COMB4_LEN 2099
#define AP_SHARED_LEN 83
#define AP_LEFT_LEN 241
#define AP_RIGHT_LEN 263
extern volatile float detuneRatio;
extern volatile float lfoRateHz;
extern volatile float vibratoDepth;
extern volatile float autoWahDepth;

#define SCOPE_BUF_SIZE 256
extern int16_t scopeBuffer[SCOPE_BUF_SIZE];
extern volatile bool scopeReady;

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
void Process_Audio(int16_t* i2s_buffer, uint16_t bufferLen,uint16_t startIndex);

/**
 * @brief Create audio waveform tables on boot-up as reference during audio process.
 * 
 */
void Audio_Tables_Generate_On_Boot(void);

#endif