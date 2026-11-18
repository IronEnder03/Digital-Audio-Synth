#include "controls.h"
#include "audio_out.h"
#include "util.h"
#include "lcd.h"

void GPIO_Init(void) {
    // Intialize peripherial clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    // Set PB0 and PB3 to Input Mode
    GPIOB->MODER &= ~GPIO_MODER_MODER0;
    GPIOB->MODER &= ~GPIO_MODER_MODER3;

    // Configure internal Pull-Up on PB0 and PB3
    GPIOB->PUPDR &= ~GPIO_PUPDR_PUPDR0;
    GPIOB->PUPDR |= (1U << GPIO_PUPDR_PUPD0_Pos);

    GPIOB->PUPDR &= ~GPIO_PUPDR_PUPDR3;
    GPIOB->PUPDR |= (1U << GPIO_PUPDR_PUPD3_Pos);

    // Map EXTI0 line and EXTI3 to Port B in SYSCFG_EXTICR1
    // EXTICR[0] controls EXTI0..3; bits [3:0] select the port for EXTI0 (0001 = PB)
    SYSCFG->EXTICR[0] &= ~(SYSCFG_EXTICR1_EXTI0 | SYSCFG_EXTICR1_EXTI3);
    SYSCFG->EXTICR[0] |= (SYSCFG_EXTICR1_EXTI0_PB | SYSCFG_EXTICR1_EXTI3_PB);

    // Configure EXTI Line 0 and EXTI Line 3 trigger and mask
    EXTI->IMR  |= EXTI_IMR_MR0;   // Unmask (enable) interrupt on EXTI0
    EXTI->RTSR &= ~EXTI_RTSR_TR0;  // Disable Rising Edge trigger
    EXTI->FTSR |= EXTI_FTSR_TR0; // Enable Falling Edge trigger

    EXTI->IMR  |= EXTI_IMR_MR3;   // Unmask (enable) interrupt on EXTI3
    EXTI->RTSR &= ~EXTI_RTSR_TR3;  // Disable Rising Edge trigger
    EXTI->FTSR |= EXTI_FTSR_TR3; // Enable Falling Edge trigger

    // Configure NVIC for EXTI0 and EXTI0
    NVIC_SetPriority(EXTI0_IRQn, 3);
    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_SetPriority(EXTI3_IRQn, 3);
    NVIC_EnableIRQ(EXTI3_IRQn);
}

// Interrupt Service Routine (ISR) for EXTI Line 0
void EXTI0_IRQHandler(void) {
    // Check if EXTI0 pending flag is set
    if (EXTI->PR & EXTI_PR_PR0) {
        // Clear the pending flag by writing a '1' to it
        EXTI->PR = EXTI_PR_PR0;
        
        static uint32_t lastPressTime = 0;
        // Increment waveform index
        uint32_t now = msTicks;
        if ((now - lastPressTime) > 150) { // 150 ms lockout window
            lastPressTime = now;
            currentWaveForm++;
            if (currentWaveForm > 3) currentWaveForm = SINE;
        }
    }
}

// Interrupt Service Routine (ISR) for EXTI Line 3
void EXTI3_IRQHandler(void) {
    // Check if EXTI0 pending flag is set
    if (EXTI->PR & EXTI_PR_PR3) {
        // Clear the pending flag by writing a '1' to it
        EXTI->PR = EXTI_PR_PR3;
        
        static uint32_t lastPressTime = 0;
        // Increment waveform index
        uint32_t now = msTicks;
        if ((now - lastPressTime) > 150) { // 150 ms lockout window
            lastPressTime = now;
            currentPage++;
            if (currentPage > 3) currentPage = 0;
        }
    }
}
