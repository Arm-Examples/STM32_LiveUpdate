#ifndef _PRESERVEDVARIABLES
#define _PRESERVEDVARIABLES

#include "stm32l4xx.h"                  // Device header

extern volatile uint32_t Milliseconds, Seconds ;
extern volatile uint32_t Tick ;
extern uint32_t LastMilliseconds, LongestMilliseconds ;
extern uint32_t Switched ;

#endif  // _PRESERVEDVARIABLES
