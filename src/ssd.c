// *****************************************************************
// * Author: Brian Johnson
// * CPEG222 Project 3, 9/29/26
// * seven segment display (SSD) file
// *****************************************************************

//ssd.c does the following:
// 1. configures the functions to run the 7 segment display (SSD)
// 2. converts numbers into the 4 digits to be displayed on the SSD
// 3. multiplexes the SSD to display the digits

#include "ssd.h"

typedef struct { GPIO_TypeDef *port; uint8_t pin; } PinDef;

// Segment pins in order A, B, C, D, E, F, G, DP
static const PinDef segPins[8] = {
    {GPIOG, 9},   // A
    {GPIOF, 12},  // B
    {GPIOF, 13},  // C
    {GPIOG, 14},  // D
    {GPIOE, 8},   // E
    {GPIOF, 15},  // F
    {GPIOB, 4},   // G
    {GPIOF, 14}   // DP
};

// Digit-enable pins in order: tens, ones, tenths, hundredths
static const PinDef digitPins[4] = {
    {GPIOE, 10},  // CA1
    {GPIOE, 7},   // CA2
    {GPIOB, 5},   // CA3
    {GPIOB, 3}    // CA4
};

#define SSD_BLANK 10

// Logical segment patterns for 0-9 and blank; bit i = 1 means "segment i is ON"
static const uint8_t segmentTable[11] = {
    0b0111111, // 0
    0b0000110, // 1
    0b1011011, // 2
    0b1001111, // 3
    0b1100110, // 4
    0b1101101, // 5
    0b1111101, // 6
    0b0000111, // 7
    0b1111111, // 8
    0b1101111, // 9
    0b0000000  // blank
};

// Holds the digit currently being shown and which digit multiplexing is on
static volatile uint8_t digitValues[4] = { SSD_BLANK, 0, 0, 0 };
static volatile uint8_t currentDigit = 0;

// Drives one pin HIGH or LOW using BSRR for an atomic write
static inline void setPin(GPIO_TypeDef *port, uint8_t pin, uint8_t level) {
    if (level) {
        port->BSRR = (1 << pin);
    } else {
        port->BSRR = (1 << (pin + 16));
    }
}

// Turns every segment and every digit off, used between multiplex steps to prevent ghosting
static void SSD_AllOff(void) {
    for (int i = 0; i < 8; i++) {
        setPin(segPins[i].port, segPins[i].pin, 1);
    }
    for (int i = 0; i < 4; i++) {
        setPin(digitPins[i].port, digitPins[i].pin, 1);
    }
}

// Enables clocks, sets all segment/digit pins as outputs, and blanks the display
void SSD_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOEEN  //turn on the clocks for GPIOB, GPIOE, GPIOF, GPIOG  
                  | RCC_AHB1ENR_GPIOFEN | RCC_AHB1ENR_GPIOGEN;

    for (int i = 0; i < 8; i++) {  // set all segment pins as outputs
        GPIO_TypeDef *p = segPins[i].port;
        uint8_t pin = segPins[i].pin;
        p->MODER &= ~(0x3 << (pin * 2));
        p->MODER |=  (0x1 << (pin * 2));
    }
    for (int i = 0; i < 4; i++) {  // set all digit pins as outputs
        GPIO_TypeDef *p = digitPins[i].port;
        uint8_t pin = digitPins[i].pin;
        p->MODER &= ~(0x3 << (pin * 2));
        p->MODER |=  (0x1 << (pin * 2));
    }

    SSD_AllOff();
}

// takes in the hundreths value and converts it into the 4 digits to be displayed on the SSD
void SSD_SetValue(uint16_t hundredths) {
    if (hundredths > 9999) hundredths = 9999;

    uint8_t tens    = hundredths / 1000;
    uint8_t ones    = (hundredths / 100) % 10;
    uint8_t tenths  = (hundredths / 10) % 10;
    uint8_t hund    = hundredths % 10;

    digitValues[0] = (tens == 0) ? SSD_BLANK : tens;
    digitValues[1] = ones;
    digitValues[2] = tenths;
    digitValues[3] = hund;
}

// Multiplex step: blanks everything, lights the current digit's segments, then moves to the next digit
void SSD_Refresh(void) {
    SSD_AllOff();

    uint8_t value   = digitValues[currentDigit];  //figure out which digit to displau
    uint8_t pattern = segmentTable[value];

    for (int i = 0; i < 7; i++) { // light the segments for this digit
        uint8_t on = (pattern >> i) & 0x1;
        setPin(segPins[i].port, segPins[i].pin, on ? 0 : 1); // active-low segment
    }

    uint8_t dpOn = (currentDigit == 1); // decimal point sits after the ones digit
    setPin(segPins[7].port, segPins[7].pin, dpOn ? 0 : 1);

    setPin(digitPins[currentDigit].port, digitPins[currentDigit].pin, 0); // enable this digit

    currentDigit = (currentDigit + 1) % 4;  //updates current digit value
}