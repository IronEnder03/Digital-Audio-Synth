#include "util.h"

volatile uint32_t msTicks = 0;

// SysTick Interrupt Handler - increments every 1 millisecond
void SysTick_Handler(void) {
    msTicks++;
}

// Precise Millisecond Delay
void delay_ms(uint32_t ms) {
    uint32_t start = msTicks;
    while ((msTicks - start) < ms);
}

// Precise Microsecond Delay (Uses the underlying SysTick value counter)
void delay_us(uint32_t us) {
    // Get the number of clock cycles per microsecond (180 MHz / 1,000,000 = 180)
    uint32_t ticks_per_us = SystemCoreClock / 1000000;
    uint32_t ticks = us * ticks_per_us;
    
    uint32_t start_val = SysTick->VAL;
    uint32_t current_val;
    uint32_t elapsed = 0;
    
    while (elapsed < ticks) {
        current_val = SysTick->VAL;
        if (current_val < start_val) {
            elapsed += (start_val - current_val);
        } else if (current_val > start_val) {
            // Handled timer reload/wrap around
            elapsed += (SysTick->LOAD - current_val + start_val);
        }
        start_val = current_val;
    }
}


int8_t signf(float num) {
    if (num > 0) {
        return 1;
    } else if (num < 0) {
        return -1;
    } else {
        return 0;
    }
}
