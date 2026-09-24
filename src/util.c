#include "util.h"

void delay(volatile uint32_t count) {
    count = count * 3000;
    while (count--) {
        __NOP(); 
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
