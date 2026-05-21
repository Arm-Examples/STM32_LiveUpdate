#include "stm32l4xx.h"                  // Device header
#include <string.h>


extern void __main( void ) ;
extern uint32_t Image$$RW_HANDLERS$$Base ;
extern uint32_t Load$$RW_HANDLERS$$Base ;
extern uint32_t Load$$RW_HANDLERS$$Length ;
extern uint32_t Image$$RW_HANDLERS_1$$Base ;
extern uint32_t Load$$RW_HANDLERS_1$$Base ;
extern uint32_t Load$$RW_HANDLERS_1$$Length ;
extern uint32_t Image$$ER_RESET$$Length ;


__attribute__( ( used ) ) uint32_t * const Signature[ ] =
{ 
  ( void * )0xABABABAB,
  &Image$$RW_HANDLERS$$Base,
  &Load$$RW_HANDLERS$$Base,
  &Load$$RW_HANDLERS$$Length,
  &Image$$RW_HANDLERS_1$$Base,
  &Load$$RW_HANDLERS_1$$Base,
  &Load$$RW_HANDLERS_1$$Length,
  &Image$$ER_RESET$$Length,
  ( uint32_t * const )__main
} ;


void SwitchEntry( void )
{
  uint32_t *SwitchSignature ;
  uint32_t SwitchVectors ;

  SwitchSignature = ( uint32_t * )( ( uint32_t )Signature | 0x00080000 ) ;
  if( 0xABABABAB == SwitchSignature[ 0 ] )
  {  /* other bank has valid firmware version */
    if( ( uint32_t )Signature[ 9 ] < SwitchSignature[ 9 ] )
    {  /* other bank has newer version */
      if( SYSCFG->MEMRMP & SYSCFG_MEMRMP_FB_MODE )
      {  /* bank 1 active, new vectors are in "bank 0" SRAM2 */
        memcpy( ( void * )SwitchSignature[ 1 ],  /* copy new handler functions */
                ( void * )( SwitchSignature[ 2 ] | 0x00080000 ),
                ( size_t )( SwitchSignature[ 3 ] ) ) ;
        SwitchVectors = 0x10000000 ;
        memcpy( ( void * )SwitchVectors,  /* copy vector table */
                ( void * )0x08080000UL,
                ( size_t )( SwitchSignature[ 7 ] ) ) ;
      }
      else
      {  /* bank 0 active, new vectors are in "bank 1" SRAM2 */
        memcpy( ( void * )SwitchSignature[ 4 ],  /* copy new handler functions */
                ( void * )( SwitchSignature[ 5 ] | 0x00080000 ),
                ( size_t )( SwitchSignature[ 6 ] ) ) ;
        SwitchVectors = 0x10004000 ;
        memcpy( ( void * )SwitchVectors,  /* copy vector table */
                ( void * )0x08080000UL,
                ( size_t )( SwitchSignature[ 7 ] ) ) ;
      }
      __disable_irq( ) ;
      SCB->VTOR = SwitchVectors ;
      SYSCFG->MEMRMP ^= SYSCFG_MEMRMP_FB_MODE ;
      __DSB( ) ;
      __ISB( ) ;
      __enable_irq( ) ;
      ( ( void ( * )( void ) )SwitchSignature[ 8 ] )( ) ;
    }
  }
}


void NMI_Handler( void )
{
  while( 1 )
  {
  }
}
