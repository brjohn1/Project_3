// *****************************************************************
// * Author: Brian Johnson
// * CPEG222 Project 3, 9/29/26
// * seven segment display (SSD) file
// *****************************************************************

//ssd.h does the following:
// tells main.c what ssd.c provides

#ifndef SSD_H
#define SSD_H

#include "stm32f4xx.h"

// Sets up all GPIO pins used by the segments and digit enables, and blanks the display
void SSD_Init(void);

// Converts a 0-9999 hundredths-of-a-second value into the 4 digits to be displayed
void SSD_SetValue(uint16_t hundredths);

// Lights exactly one digit and advances to the next; call this every 10ms from TIM5's ISR
void SSD_Refresh(void);

#endif