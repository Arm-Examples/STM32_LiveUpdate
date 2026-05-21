#ifndef _PRESERVEDVARIABLES
#define _PRESERVEDVARIABLES

#include "stm32l4xx.h"                  // Device header
/* version 0 preserved variables */
extern volatile uint32_t Milliseconds, Seconds, Minutes ;
extern volatile uint32_t Tick ;
extern uint32_t LastMilliseconds, LongestMilliseconds ;
extern uint32_t Switched ;
/* version 1 preserved variables */
extern volatile uint32_t Minutes ;

#endif  // _PRESERVEDVARIABLES
