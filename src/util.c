#include "util.h"
#include <stdlib.h>

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

void SystemClock_Boost_180MHz(void) {
    // Enable Power Control clock and set Voltage Regulator to Scale 1
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS;

    // Ensure 16 MHz HSI is ON and ready
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));

    // Configure Main PLL from 16 MHz HSI -> 180 MHz SYSCLK
    //    PLLM = 8 (2 MHz input), PLLN = 180 (360 MHz VCO), PLLP = 2 (180 MHz out)
    RCC->PLLCFGR = (8U   << RCC_PLLCFGR_PLLM_Pos) |
                   (180U << RCC_PLLCFGR_PLLN_Pos) |
                   (0U   << RCC_PLLCFGR_PLLP_Pos) |
                   (RCC_PLLCFGR_PLLSRC_HSI)       |
                   (7U   << RCC_PLLCFGR_PLLQ_Pos);

    // Enable Main PLL and wait for lock
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    // Enable Over-Drive mode (required above 168 MHz on STM32F446)
    PWR->CR |= PWR_CR_ODEN;
    while (!(PWR->CSR & PWR_CSR_ODRDY));
    PWR->CR |= PWR_CR_ODSWEN;
    while (!(PWR->CSR & PWR_CSR_ODSWRDY));

    // Enable Flash caches and 5 Wait States for 180 MHz
    FLASH->ACR = FLASH_ACR_PRFTEN |
                 FLASH_ACR_ICEN   |
                 FLASH_ACR_DCEN   |
                 FLASH_ACR_LATENCY_5WS;

    // Set bus prescalers: AHB = 180 MHz, APB1 = 45 MHz, APB2 = 90 MHz
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1  |
                 RCC_CFGR_PPRE1_DIV4 |
                 RCC_CFGR_PPRE2_DIV2;

    // Switch SYSCLK to PLL
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

    SystemCoreClockUpdate();
}
