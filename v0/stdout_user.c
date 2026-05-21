/*-----------------------------------------------------------------------------
 * Name:    stdout_user.c
 * Purpose: STDOUT User Template
 * Rev.:    1.0.0
 *-----------------------------------------------------------------------------*/

/*
 * Copyright (C) 2023 ARM Limited or its affiliates. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "retarget_stdout.h"
#include "Driver_USART.h"

#define UART_BAUDRATE     115200

extern ARM_DRIVER_USART Driver_USART2 ;
ARM_DRIVER_USART *ptrUSART = &Driver_USART2 ;


int stdio_uninit (void) {
  if (ptrUSART->PowerControl(ARM_POWER_OFF) != ARM_DRIVER_OK) {
    return -1;
  }
  if (ptrUSART->Uninitialize( ) != ARM_DRIVER_OK) {
    return -1;
  }
  return( 0 ) ;
}


int stdio_init (void) {

  if (ptrUSART->Initialize(NULL) != ARM_DRIVER_OK) {
    return -1;
  }

  if (ptrUSART->PowerControl(ARM_POWER_FULL) != ARM_DRIVER_OK) {
    return -1;
  }

  if (ptrUSART->Control(ARM_USART_MODE_ASYNCHRONOUS |
                        ARM_USART_DATA_BITS_8       |
                        ARM_USART_PARITY_NONE       |
                        ARM_USART_STOP_BITS_1       |
                        ARM_USART_FLOW_CONTROL_NONE,
                        UART_BAUDRATE) != ARM_DRIVER_OK) {
    return -1;
  }

  if (ptrUSART->Control(ARM_USART_CONTROL_RX, 1U) != ARM_DRIVER_OK) {
    return -1;
  }

  if (ptrUSART->Control(ARM_USART_CONTROL_TX, 1U) != ARM_DRIVER_OK) {
    return -1;
  }

  return 0;
}



/**
  Put a character to the stdout

  \param[in]   ch  Character to output
  \return          The character written, or -1 on write error.
*/
int stdout_putchar (int ch) {
  uint8_t buf[1];

  buf[0] = (uint8_t)ch;

  if (ptrUSART->Send(buf, 1U) != ARM_DRIVER_OK) {
    return -1;
  }

  while (ptrUSART->GetStatus().tx_busy != 0U);

  return ch;
}
