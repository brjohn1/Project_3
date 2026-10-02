// *****************************************************************
// * Author: Brian Johnson
// * CPEG222 Project 3, 9/29/26
// * NucleoF446ZE + CPEG222 Shield - CMSIS bare metal
// *****************************************************************

// main.c does the following:
// 1. runs the stopwatch
// 2. handles button presses/interrupts
// 3. handles systick timer for stopwatch timing
// 4. handles TIM5 timer for SSD multiplexing


#include "stm32f4xx.h"
#include "ssd.h"

// Button pin/port definitions
#define BTN_LEFT_PORT   GPIOF
#define BTN_LEFT_PIN    9   // EXTI9
#define BTN_CENTER_PORT GPIOF
#define BTN_CENTER_PIN  8   // EXTI8
#define BTN_UP_PORT     GPIOF
#define BTN_UP_PIN      7   // EXTI7
#define BTN_RIGHT_PORT  GPIOE
#define BTN_RIGHT_PIN   6   // EXTI6
#define BTN_DOWN_PORT   GPIOE
#define BTN_DOWN_PIN    5   // EXTI5

// Program state and the live stopwatch value
typedef enum { PAUSED, COUNT_UP, COUNT_DOWN } state_t;
volatile state_t state = PAUSED;
volatile uint16_t hundredths = 0; // 0-9999 => 0.00-99.99

// Blocking delay, used only for button debounce
void delay(volatile uint32_t count) {
    while (count--) { __NOP(); }
}
void delay_ms(uint32_t ms) {
    delay(ms * 4000);
}

// Sets all 5 button pins as inputs; external 10k pull-ups already exist on the shield
void Buttons_Init(void) {  // defines which pins are button inputs
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN | RCC_AHB1ENR_GPIOFEN;

    BTN_LEFT_PORT->MODER   &= ~(0x3 << (BTN_LEFT_PIN * 2));
    BTN_CENTER_PORT->MODER &= ~(0x3 << (BTN_CENTER_PIN * 2));
    BTN_UP_PORT->MODER     &= ~(0x3 << (BTN_UP_PIN * 2));
    BTN_RIGHT_PORT->MODER  &= ~(0x3 << (BTN_RIGHT_PIN * 2));
    BTN_DOWN_PORT->MODER   &= ~(0x3 << (BTN_DOWN_PIN * 2));
}

// Routes EXTI lines 5-9 to the correct ports and enables their shared interrupt
void EXTI_Init(void) {   //connects buttons to external interrupts
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    SYSCFG->EXTICR[2] &= ~(0xF << 4);
    SYSCFG->EXTICR[2] |=  (0x5 << 4);   // EXTI9 -> GPIOF (LEFT)
    SYSCFG->EXTICR[2] &= ~(0xF << 0);
    SYSCFG->EXTICR[2] |=  (0x5 << 0);   // EXTI8 -> GPIOF (CENTER)
    SYSCFG->EXTICR[1] &= ~(0xF << 12);
    SYSCFG->EXTICR[1] |=  (0x5 << 12);  // EXTI7 -> GPIOF (UP)
    SYSCFG->EXTICR[1] &= ~(0xF << 8);
    SYSCFG->EXTICR[1] |=  (0x4 << 8);   // EXTI6 -> GPIOE (RIGHT)
    SYSCFG->EXTICR[1] &= ~(0xF << 4);
    SYSCFG->EXTICR[1] |=  (0x4 << 4);   // EXTI5 -> GPIOE (DOWN)

    uint32_t lines = (1<<9)|(1<<8)|(1<<7)|(1<<6)|(1<<5);
    EXTI->IMR  |= lines;
    EXTI->FTSR |= lines; // falling edge = button press
    EXTI->RTSR &= ~lines;

    NVIC_SetPriority(EXTI9_5_IRQn, 1);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
}

// Configures SysTick to interrupt every 10ms
void SysTick_Init(void) {
    SysTick->LOAD = (SystemCoreClock / 100) - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk
                  | SysTick_CTRL_ENABLE_Msk;
    NVIC_SetPriority(SysTick_IRQn, 0);
}

// Every 10ms: advances or reverses the stopwatch depending on state, then updates the digits
void SysTick_Handler(void) {
    if (state == COUNT_UP) {
        if (hundredths < 9999) hundredths++;
    } else if (state == COUNT_DOWN) {
        if (hundredths > 0) hundredths--;
    }
    SSD_SetValue(hundredths);
}

// Configures TIM5 to interrupt every 10ms, used to drive display multiplexing
void TIM5_Init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;

    TIM5->PSC = 1599; // 16MHz / 1600 = 10kHz -> 0.1ms per tick
    TIM5->ARR = 24;      // 25 ticks = 2.5ms per interrupt

    // clock divider math again: 16MHz / 1600 = 10kHz -> 0.1ms per tick, 25 ticks = 2.5ms per interrupt

    TIM5->EGR = TIM_EGR_UG;
    TIM5->SR  = ~TIM_SR_UIF;
    TIM5->DIER |= TIM_DIER_UIE;

    NVIC_SetPriority(TIM5_IRQn, 2);
    NVIC_EnableIRQ(TIM5_IRQn);

    TIM5->CR1 = TIM_CR1_CEN;
}

// Every 10ms: refreshes one SSD digit, driving the multiplexing
void TIM5_IRQHandler(void) {
    if (TIM5->SR & TIM_SR_UIF) {
        TIM5->SR = ~TIM_SR_UIF;
        SSD_Refresh();
    }
}

// Shared handler for all 5 buttons (EXTI lines 5-9); checks each pending flag to find who fired
void EXTI9_5_IRQHandler(void) {
    if (EXTI->PR & (1 << 9)) {        // LEFT: jump to 0.00 and pause (bonus)
        EXTI->PR |= (1 << 9);
        hundredths = 0;
        SSD_SetValue(hundredths);
        state = PAUSED;
        delay_ms(50);
    }
    if (EXTI->PR & (1 << 8)) {        // CENTER: pause
        EXTI->PR |= (1 << 8);
        state = PAUSED;
        delay_ms(50);
    }
    if (EXTI->PR & (1 << 7)) {        // UP: count up
        EXTI->PR |= (1 << 7);
        state = COUNT_UP;
        delay_ms(50);
    }
    if (EXTI->PR & (1 << 6)) {        // RIGHT: jump to 99.99 and pause (bonus)
        EXTI->PR |= (1 << 6);
        hundredths = 9999;
        SSD_SetValue(hundredths);
        state = PAUSED;
        delay_ms(50);
    }
    if (EXTI->PR & (1 << 5)) {        // DOWN: count down
        EXTI->PR |= (1 << 5);
        state = COUNT_DOWN;
        delay_ms(50);
    }
}

int main(void) {
    Buttons_Init();
    SSD_Init();
    EXTI_Init();
    SysTick_Init();
    TIM5_Init();

    SSD_SetValue(0); // startup: shows 0.00 with the first digit blank

    while (1) {
        // everything runs from interrupts; nothing needed here
    }
    return 0;
}
