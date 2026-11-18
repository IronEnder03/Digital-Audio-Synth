#include "audio_out.h"

#include "arm_math.h"

int16_t i2s_tx_buffer[I2S_BUFFER_SIZE] = {0};
uint32_t midi_fixed_phase_inc_lut[MAX_MIDI_NOTES];
int16_t sine_wave_lut[WAVE_LUT_SIZE];
int16_t square_wave_lut[WAVE_LUT_SIZE];
int16_t triangle_wave_lut[WAVE_LUT_SIZE];
int16_t sawtooth_wave_lut[WAVE_LUT_SIZE];

volatile WaveForm currentWaveForm = SINE;
volatile FilterMode currentFilterMode = FILTER_LOWPASS;

volatile float attackTime = 0.05f;
volatile float decayTime = 0.05f;
volatile float releaseTime = 0.05f;
volatile float sustainLevel = 0.70f;
volatile float svf_f = 0.25f;
volatile float svf_q = 0.70f;
volatile float chorusMix = 0.0f;
volatile float chorusDepth = 0.0f;
volatile float delayMix = 0.0f;
volatile float delayFeedback = 0.0f;
volatile float reverbMix = 0.0f;
volatile float reverbDecay = 0.65f;
volatile float detuneRatio = 0.015f;
volatile float lfoRateHz = 3.0f;
volatile float vibratoDepth = 0.0f;
volatile float autoWahDepth = 0.0f; 

int16_t scopeBuffer[256] = {0};
volatile bool scopeReady = false;
static uint16_t scopeIdx = 0;

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

    // Configure PLLI2SCFGR
    RCC->PLLI2SCFGR = (8U   << RCC_PLLI2SCFGR_PLLI2SM_Pos) |
                      (192U << RCC_PLLI2SCFGR_PLLI2SN_Pos) |
                      (2U   << RCC_PLLI2SCFGR_PLLI2SR_Pos);

    // Enable PLLI2S and wait for lock
    RCC->CR |= RCC_CR_PLLI2SON;
    while (!(RCC->CR & RCC_CR_PLLI2SRDY));

    // Modify ONLY pins 9, 10, and 15
    GPIOB->MODER &= ~( (3U << GPIO_MODER_MODER9_Pos)  | 
                       (3U << GPIO_MODER_MODER10_Pos) | 
                       (3U << GPIO_MODER_MODER15_Pos) );
    GPIOB->MODER |=  ( (2U << GPIO_MODER_MODER9_Pos)  | // Alternate Function
                       (2U << GPIO_MODER_MODER10_Pos) | 
                       (2U << GPIO_MODER_MODER15_Pos) );

    // High Speed for audio lines
    GPIOB->OSPEEDR &= ~( (3U << GPIO_OSPEEDR_OSPEED9_Pos)  | 
                         (3U << GPIO_OSPEEDR_OSPEED10_Pos) | 
                         (3U << GPIO_OSPEEDR_OSPEED15_Pos) );
    GPIOB->OSPEEDR |=  ( (3U << GPIO_OSPEEDR_OSPEED9_Pos)  | 
                         (3U << GPIO_OSPEEDR_OSPEED10_Pos) | 
                         (3U << GPIO_OSPEEDR_OSPEED15_Pos) );

    // This removes the 4-bit fields for pins 9, 10, and 15
    GPIOB->AFR[1] &= ~( (0xFU << ((9 - 8) * 4))  | 
                        (0xFU << ((10 - 8) * 4)) | 
                        (0xFU << ((15 - 8) * 4)) );

    // Assign exactly AF5 (I2S2) using literal bit shifts into AFR[1]
    GPIOB->AFR[1] |=  ( (5U << ((9 - 8) * 4))  | // PB9  -> I2S2_WS
                        (5U << ((10 - 8) * 4)) | // PB10 -> I2S2_CK
                        (5U << ((15 - 8) * 4)) );// PB15 -> I2S2_SD

    /* ===== I2S Configuration ===== */
    SPI2->I2SCFGR = 0;
    SPI2->I2SCFGR |= (0x1 << SPI_I2SCFGR_I2SMOD_Pos); // I2S Mode
    SPI2->I2SCFGR |= (0x2 << SPI_I2SCFGR_I2SCFG_Pos); // Master Transmit Mode

    // Prescaler for exact fs = 48.000 kHz:
    SPI2->I2SPR = 0;
    SPI2->I2SPR |= (0x1U  << SPI_I2SPR_ODD_Pos);       // ODD = 1
    SPI2->I2SPR |= (0x3EU << SPI_I2SPR_I2SDIV_Pos);    // I2SDIV = 62 (0x3E)

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

void DMA1_Stream4_IRQHandler(void) {
    // Half-transfer complete, second half of buffer is safe to write to
    if (DMA1->HISR & DMA_HISR_HTIF4) {
        DMA1->HIFCR = DMA_HIFCR_CHTIF4;
        Process_Audio(i2s_tx_buffer, I2S_BUFFER_SIZE / 2, 0);
    }

    // Full-transfer complete, first half of buffer is safe to write to
    if (DMA1->HISR & DMA_HISR_TCIF4) {
        DMA1->HIFCR = DMA_HIFCR_CTCIF4;
        Process_Audio(i2s_tx_buffer, I2S_BUFFER_SIZE / 2, I2S_BUFFER_SIZE / 2);
    }
}

void Synth_Play_Note(uint8_t channel, uint8_t noteNum, uint8_t velocity) {
    // Find if a current voice is already playing the given note
    bool notePlaying = false;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].note == noteNum && voices[i].active == true) {
            notePlaying = true;
            if (voices[i].envStatus == ENV_RELEASE) {
                voices[i].envStatus = ENV_ATTACK;
                voices[i].velocity = velocity;
            }
            break;
        }
    }

    // If the given note is not already playing, assign it to the next available voice.
    bool voiceAvailable = false;
    if (!notePlaying) {
        for (size_t i = 0; i < MAX_VOICES; i++) {
            if (voices[i].active == false) {
                voices[i].active = true;
                voices[i].envStatus = ENV_ATTACK;
                voices[i].envLevel = 0.0f;
                voices[i].note = noteNum;
                voices[i].channel = channel;
                voices[i].velocity = velocity;
                voices[i].phase = 0;
                voices[i].phase2 = 0x80000000;
                voices[i].phaseInc = midi_fixed_phase_inc_lut[noteNum];
                for (size_t j = 0; j < MAX_VOICES; j++) {
                    if (voices[j].active) voices[j].age++;
                }
                voiceAvailable = true;
                break;
            }
        }

        if (!voiceAvailable) {
            for (size_t i = 0; i < MAX_VOICES; i++) {
                if (voices[i].age == 8) {
                    voices[i].active = true;
                    voices[i].envStatus = ENV_STEAL;
                    voices[i].channel = channel;
                    voices[i].age = 1;
                    voices[i].pendingNote = noteNum;
                    voices[i].pendingVelocity = velocity;
                    break;
                }
            }
        }
    }
}

void Synth_Stop_Note(uint8_t channel, uint8_t noteNum) {
    // Find voice that has matching channel and note, and set it to envelope release status
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].channel == channel && voices[i].note == noteNum && voices[i].active == true) {
            voices[i].envStatus = ENV_RELEASE;
            break;
        }
    }
}

void Process_Audio(int16_t* i2s_buffer, uint16_t bufferLen, uint16_t startIndex) {
    const int16_t* activeLut = sine_wave_lut;
    if (currentWaveForm == SQUARE)        activeLut = square_wave_lut;
    else if (currentWaveForm == TRIANGLE) activeLut = triangle_wave_lut;
    else if (currentWaveForm == SAWTOOTH) activeLut = sawtooth_wave_lut;

    
    float attackStep = (1.0f/(attackTime*SAMPLING_FREQ_HZ));
    float decayStep = ((1.0f-sustainLevel)/(decayTime*SAMPLING_FREQ_HZ));
    float releaseStep = (1.0f/(releaseTime*SAMPLING_FREQ_HZ));
    float stealStep = (1.0f/(0.05f*SAMPLING_FREQ_HZ));

    static int16_t chorusBuffer[2048] = {0};
    static uint16_t chorusWriteIdx = 0;
    static float chorusLfoPhase = 0.0f;
    const float chorusLfoInc = (2.0f * PI * 0.8f) / SAMPLING_FREQ_HZ;

    static int16_t delayBuffer[DELAY_BUF_SIZE] = {0};
    static uint16_t delayWriteIdx = 0;
    static float delayFbFilter = 0.0f;
    const uint16_t TAP_LEFT  = 10560;   // 220 ms
    const uint16_t TAP_RIGHT = 15840;   // 330 ms

    static int16_t comb1Buffer[COMB1_LEN] = {0};
    static int16_t comb2Buffer[COMB2_LEN] = {0};
    static int16_t comb3Buffer[COMB3_LEN] = {0};
    static int16_t comb4Buffer[COMB4_LEN] = {0};
    static uint16_t comb1WriteIdx = 0;
    static uint16_t comb2WriteIdx = 0;
    static uint16_t comb3WriteIdx = 0;
    static uint16_t comb4WriteIdx = 0;
    static float combLp1 = 0.0f;
    static float combLp2 = 0.0f;
    static float combLp3 = 0.0f;
    static float combLp4 = 0.0f;
    static int16_t apSharedBuffer[AP_SHARED_LEN] = {0};
    static int16_t apLeftBuffer[AP_LEFT_LEN] = {0};
    static int16_t apRightBuffer[AP_RIGHT_LEN] = {0};
    static uint16_t apSharedWriteIdx = 0;
    static uint16_t apLeftWriteIdx = 0;
    static uint16_t apRightWriteIdx = 0;

    uint32_t inc1[MAX_VOICES];
    uint32_t inc2[MAX_VOICES];

    for (int i = startIndex; i < bufferLen + startIndex; i += 2) {
        int32_t mixedSample = 0;
        
        int16_t s0_1 = 0;
        int16_t s1_1 = 0;
        int16_t s0_2 = 0;
        int16_t s1_2 = 0;
        
        static float low = 0.0f;
        static float band = 0.0f;
        float high = 0.0f;
        float filteredOut = 0.0f;
        
        static float perfLfoPhase = 0.0f;
        float perfLfoInc = (2.0f * PI * lfoRateHz) / SAMPLING_FREQ_HZ;
        float perfLfoVal = arm_sin_f32(perfLfoPhase);
        perfLfoPhase += perfLfoInc;
        if (perfLfoPhase >= 2.0f * PI) perfLfoPhase -= 2.0f * PI;
        float pitchMod = 1.0f + (perfLfoVal * vibratoDepth);

        for (int v = 0; v < MAX_VOICES; v++) {
            if (voices[v].active) {
                inc1[v] = (uint32_t)(((float)voices[v].phaseInc * (1.0f - detuneRatio)) * pitchMod);
                inc2[v] = (uint32_t)(((float)voices[v].phaseInc * (1.0f + detuneRatio)) * pitchMod);
                // ADSR logic for active note
                if (voices[v].envStatus == ENV_ATTACK) {
                    voices[v].envLevel += attackStep;
                    if (voices[v].envLevel >= 1.0f) {
                        voices[v].envLevel = 1.0f;
                        voices[v].envStatus = ENV_DECAY;
                    }
                } else if (voices[v].envStatus == ENV_DECAY) {
                    voices[v].envLevel -= decayStep;
                    if (voices[v].envLevel <= sustainLevel) {
                        voices[v].envLevel = sustainLevel;
                        voices[v].envStatus = ENV_SUSTAIN;
                    }
                } else if (voices[v].envStatus == ENV_RELEASE) {
                    voices[v].envLevel -= releaseStep;
                    if (voices[v].envLevel <= 0.0f) {
                        voices[v].envLevel = 0.0f;
                        voices[v].envStatus = ENV_IDLE;
                        voices[v].active = false;
                        for (size_t j = 0; j < MAX_VOICES; j++) {
                            if (voices[j].active == true && voices[v].age > voices[j].age) {
                                voices[j].age--;
                            }
                        }
                        voices[v].age = 0;
                    }
                } else if (voices[v].envStatus == ENV_STEAL) {
                    voices[v].envLevel -= stealStep;
                    if (voices[v].envLevel <= 0.0f) {
                        voices[v].envLevel = 0.0f;
                        voices[v].note = voices[v].pendingNote;
                        voices[v].velocity = voices[v].pendingVelocity;
                        voices[v].phase = 0;
                        voices[v].phase2 = 0x80000000;
                        voices[v].phaseInc = midi_fixed_phase_inc_lut[voices[v].pendingNote];
                        voices[v].envStatus = ENV_ATTACK;
                    }
                }

                // Extract the active array index from the high 16 bits of the accumulator
                uint16_t lutIndex1 = (voices[v].phase >> 16) & (WAVE_LUT_SIZE - 1);
                uint16_t lutIndex2 = (voices[v].phase2 >> 16) & (WAVE_LUT_SIZE - 1);

                // Fetch the pre-computed sample value directly from integer table
                s0_1 = activeLut[lutIndex1];
                if (lutIndex1 != (WAVE_LUT_SIZE-1)) {
                    s1_1 = activeLut[lutIndex1+1];
                } else {
                    s1_1 = activeLut[0];
                }

                s0_2 = activeLut[lutIndex2];
                if (lutIndex2 != (WAVE_LUT_SIZE-1)) {
                    s1_2 = activeLut[lutIndex2+1];
                } else {
                    s1_2 = activeLut[0];
                }

                int32_t blendedSample1 = s0_1 + (((int32_t)(s1_1 - s0_1) * (int32_t)(voices[v].phase & 0xFFFF)) >> 16);
                int32_t blendedSample2 = s0_2 + (((int32_t)(s1_2 - s0_2) * (int32_t)(voices[v].phase2 & 0xFFFF)) >> 16);
                int32_t averageBlendedSample = (blendedSample1 + blendedSample2) >> 1;

                // Apply velocity attenuation volume scaling cleanly
                int32_t voiceSample = ((averageBlendedSample * voices[v].velocity) >> 7) * voices[v].envLevel;
                mixedSample += voiceSample;

                // Add the pre-calculated step size 
                // The fraction automatically rolls over into the high bits cleanly
                voices[v].phase += inc1[v];
                voices[v].phase2 += inc2[v];
            }
        }

        float rawInput = (float)mixedSample / 65536.0f;

        // Resonant Filter
        float modF = svf_f + (perfLfoVal * autoWahDepth);
        if (modF < 0.005f) modF = 0.005f; 
        if (modF > 0.82f) modF = 0.82f;
        for (int os = 0; os < 2; os++) {
            high = rawInput - low - (svf_q * band);
            band = band + (modF * high);
            low  = low  + (modF * band);
        }

        switch (currentFilterMode) {
            case FILTER_LOWPASS:  filteredOut = low;  break;
            case FILTER_BANDPASS: filteredOut = band; break;
            case FILTER_HIGHPASS: filteredOut = high; break;
        }

        if (filteredOut > 1.0f) filteredOut = 1.0f;
        if (filteredOut < -1.0f) filteredOut = -1.0f;

        // Chorus
        chorusBuffer[chorusWriteIdx] = (int16_t)(filteredOut * 32767.0f);
        float lfoVal = arm_sin_f32(chorusLfoPhase);
        chorusLfoPhase += chorusLfoInc;
        if (chorusLfoPhase >= (2.0f * PI)) chorusLfoPhase -= (2.0f * PI);

        float delayL = 864.0f - (lfoVal * chorusDepth);
        float delayR = 864.0f + (lfoVal * chorusDepth);

        int intL = (int)delayL;
        float fracL = delayL - (float)intL;
        uint16_t idxL0 = (chorusWriteIdx - intL) & 2047;
        uint16_t idxL1 = (chorusWriteIdx - intL - 1) & 2047;
        float sL0 = (float)chorusBuffer[idxL0] / 32768.0f;
        float sL1 = (float)chorusBuffer[idxL1] / 32768.0f;
        float wetL = sL0 + fracL * (sL1 - sL0);

        int intR = (int)delayR;
        float fracR = delayR - (float)intR;
        uint16_t idxR0 = (chorusWriteIdx - intR) & 2047;
        uint16_t idxR1 = (chorusWriteIdx - intR - 1) & 2047;
        float sR0 = (float)chorusBuffer[idxR0] / 32768.0f;
        float sR1 = (float)chorusBuffer[idxR1] / 32768.0f;
        float wetR = sR0 + fracR * (sR1 - sR0);

        chorusWriteIdx = (chorusWriteIdx + 1) & 2047;

        float outL = filteredOut + (wetL * chorusMix);
        float outR = filteredOut + (wetR * chorusMix);

        // Delay
        // Read past Left & Right echoes
        uint16_t readIdxL = (delayWriteIdx - TAP_LEFT)  & 16383;
        uint16_t readIdxR = (delayWriteIdx - TAP_RIGHT) & 16383;
        float echoL = (float)delayBuffer[readIdxL] / 32768.0f;
        float echoR = (float)delayBuffer[readIdxR] / 32768.0f;

        // Warm up the feedback signal (1-pole low-pass filter makes repeats darker & warmer)
        float avgEcho = 0.5f * (echoL + echoR);
        delayFbFilter = delayFbFilter + 0.35f * (avgEcho - delayFbFilter);

        // Combine incoming mono chorus sum + damped feedback and write into buffer:
        float monoIn  = 0.5f * (outL + outR);
        float toWrite = monoIn + (delayFbFilter * delayFeedback);
        if (toWrite > 1.0f) toWrite = 1.0f;
        if (toWrite < -1.0f) toWrite = -1.0f;
        delayBuffer[delayWriteIdx] = (int16_t)(toWrite * 32767.0f);

        // Advance write pointer:
        delayWriteIdx = (delayWriteIdx + 1) & 16383;

        // Mix Dry Chorus Output + Wet Stereo Echoes:
        float delayOutL = outL + (echoL * delayMix);
        float delayOutR = outR + (echoR * delayMix);

        // Reverb
        // Sum stereo delay output to mono reverb input (attenuated slightly so 4 combs don't overload)
        float revIn = 0.25f * (delayOutL + delayOutR);

        //  Run 4 Parallel Damped Comb Filters
        // Read oldest sample at current write index
        float delayed1 = (float)comb1Buffer[comb1WriteIdx] / 32768.0f;
        float delayed2 = (float)comb2Buffer[comb2WriteIdx] / 32768.0f;
        float delayed3 = (float)comb3Buffer[comb3WriteIdx] / 32768.0f;
        float delayed4 = (float)comb4Buffer[comb4WriteIdx] / 32768.0f;

        // Warm wall absorption (1-pole low-pass filter on the feedback)
        combLp1 = combLp1 + 0.30f * (delayed1 - combLp1);
        combLp2 = combLp2 + 0.30f * (delayed2 - combLp2);
        combLp3 = combLp3 + 0.30f * (delayed3 - combLp3);
        combLp4 = combLp4 + 0.30f * (delayed4 - combLp4);

        // Write input + damped feedback back into buffer
        toWrite = revIn + (combLp1 * reverbDecay);
        if (toWrite < -1.0f) toWrite = -1.0f;
        if (toWrite > 1.0f) toWrite = 1.0f;
        comb1Buffer[comb1WriteIdx] = (int16_t)(toWrite * 32767.0f);
        
        toWrite = revIn + (combLp2 * reverbDecay);
        if (toWrite < -1.0f) toWrite = -1.0f;
        if (toWrite > 1.0f) toWrite = 1.0f;
        comb2Buffer[comb2WriteIdx] = (int16_t)(toWrite * 32767.0f);
        
        toWrite = revIn + (combLp3 * reverbDecay);
        if (toWrite < -1.0f) toWrite = -1.0f;
        if (toWrite > 1.0f) toWrite = 1.0f;
        comb3Buffer[comb3WriteIdx] = (int16_t)(toWrite * 32767.0f);
        
        toWrite = revIn + (combLp4 * reverbDecay);
        if (toWrite < -1.0f) toWrite = -1.0f;
        if (toWrite > 1.0f) toWrite = 1.0f;
        comb4Buffer[comb4WriteIdx] = (int16_t)(toWrite * 32767.0f);

        // Advance index and wrap cleanly without slow '%' modulo:
        comb1WriteIdx++;
        comb2WriteIdx++;
        comb3WriteIdx++;
        comb4WriteIdx++;
        if (comb1WriteIdx >= COMB1_LEN) comb1WriteIdx = 0;
        if (comb2WriteIdx >= COMB2_LEN) comb2WriteIdx = 0;
        if (comb3WriteIdx >= COMB3_LEN) comb3WriteIdx = 0;
        if (comb4WriteIdx >= COMB4_LEN) comb4WriteIdx = 0;

        // Sum all 4 comb outputs together
        float combSum = delayed1 + delayed2 + delayed3 + delayed4;

        // Pass through Shared Series All-Pass Filter (gain g = 0.5f):
        float apBufOut = (float)apSharedBuffer[apSharedWriteIdx] / 32768.0f;
        float apV = combSum + (0.5f * apBufOut);
        if (apV < -1.0f) apV = -1.0f;
        if (apV > 1.0f) apV = 1.0f;
        apSharedBuffer[apSharedWriteIdx] = (int16_t)(apV * 32767.0f);
        float diffused = apBufOut - (0.5f * apV);
        apSharedWriteIdx++;
        if (apSharedWriteIdx >= AP_SHARED_LEN) apSharedWriteIdx = 0;

        // Split into Stereo Left & Right All-Pass Diffusers
        // Left Channel
        float apLOut = (float)apLeftBuffer[apLeftWriteIdx] / 32768.0f;
        float apLV = diffused + (0.5f * apLOut);
        if (apLV < -1.0f) apLV = -1.0f;
        if (apLV > 1.0f) apLV = 1.0f;
        apLeftBuffer[apLeftWriteIdx] = (int16_t)(apLV * 32767.0f);
        float wetRevL = apLOut - (0.5f * apLV);
        apLeftWriteIdx++;
        if (apLeftWriteIdx >= AP_LEFT_LEN) apLeftWriteIdx = 0;

        // Right Channel
        float apROut = (float)apRightBuffer[apRightWriteIdx] / 32768.0f;
        float apRV = diffused + (0.5f * apROut);
        if (apRV < -1.0f) apRV = -1.0f;
        if (apRV > 1.0f) apRV = 1.0f;
        apRightBuffer[apRightWriteIdx] = (int16_t)(apRV * 32767.0f);
        float wetRevR = apROut - (0.5f * apRV);
        apRightWriteIdx++;
        if (apRightWriteIdx >= AP_RIGHT_LEN) apRightWriteIdx = 0;

        // Mix Dry Signal + Wet Stereo Reverb:
        float revOutL = delayOutL + (wetRevL * reverbMix);
        float revOutR = delayOutR + (wetRevR * reverbMix);

        // Soft clipping
        if (revOutL > 1.0f) revOutL = 1.0f;
        else if (revOutL < -1.0f) revOutL = -1.0f;

        float yL = (1.5f * (revOutL - ((revOutL*revOutL*revOutL)/3.0f))) * 32767.0f;
        int16_t finalOutputL = (int16_t)yL;

        if (revOutR > 1.0f) revOutR = 1.0f;
        else if (revOutR < -1.0f) revOutR = -1.0f;

        float yR = (1.5f * (revOutR - ((revOutR*revOutR*revOutR)/3.0f))) * 32767.0f;
        int16_t finalOutputR = (int16_t)yR;

        // Output to I2S Buffer slots
        i2s_buffer[i]     = finalOutputL; // Left
        i2s_buffer[i + 1] = finalOutputR; // Right

        // If main is not ready, copy Left channel into scope buffer
        if (!scopeReady) {
            scopeBuffer[scopeIdx++] = finalOutputL;
            if (scopeIdx >= SCOPE_BUF_SIZE) {
                scopeIdx = 0;
                scopeReady = true;
            }
        }
    }
}

void Audio_Tables_Generate_On_Boot(void) {
    // Automatically generate the Sine Wave LUT
    for (int i = 0; i < WAVE_LUT_SIZE; i++) {
        // Calculate the radian angle for this specific slot position
        float32_t radians = (2.0f * PI * i) / (float32_t)WAVE_LUT_SIZE;
        
        // Scale a standard float sine wave up to safe 16-bit volume levels (~21000-26000)
        sine_wave_lut[i] = (int16_t)(sinf(radians) * 26000.0f);

        // Anti-aliasing for computing square, triangle, and sawtooth waves
        // Performing by summing sine waves
        float squareSum = 0.0f;
        float triangleSum = 0.0f;
        float sawToothSum = 0.0f;
        for (float k = 1.0f; k <= 12.0f; k += 1.0f) {
            sawToothSum += ((2.0f / PI) * (sinf(k*radians)/k) * 20000.0f);
        }

        for (float k = 1.0f; k <= 13.0f; k += 2.0f) {
            squareSum += ((4.0f / PI) * (sinf(k*radians)/k) * 21000.0f);
            triangleSum += ((4.0f / PI) * (powf(-1.0f, (k-1.0f)/2.0f) * (sinf(k*radians) / (k*k))) * 20000.0f);
        }

        square_wave_lut[i] = (int16_t)squareSum;
        triangle_wave_lut[i] = (int16_t)triangleSum;
        sawtooth_wave_lut[i] = (int16_t)sawToothSum;
    }

    // Automatically generate the 128-note Pitch Phase Increment LUT
    for (int note = 0; note < MAX_MIDI_NOTES; note++) {
        // Calculate the physical note frequency (Equal Temperament tuning formula)
        float32_t frequency = 440.0f * powf(2.0f, (note - 69) / 12.0f);
        
        // Convert the frequency step size directly into a Q16.16 fixed-point value
        float32_t continuousStep = (frequency * (float32_t)WAVE_LUT_SIZE) / SAMPLING_FREQ_HZ;
        midi_fixed_phase_inc_lut[note] = (uint32_t)(continuousStep * 65536.0f);
    }
}

