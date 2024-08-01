/*===================================================================*/
/*                                                                   */
/*  K6502_RW.h : 6502 Reading/Writing Operation for NES              */
/*               This file is included in K6502.cpp                  */
/*                                                                   */
/*  1999/11/03  Racoon  New preparation                              */
/*  2000/02/27  Brice   Some correction. See below                   */
/*  2000/04/21  Racoon  Add Bank Table Setting to K6502_Write()      */
/*===================================================================*/

#ifndef K6502_RW_H_INCLUDED
#define K6502_RW_H_INCLUDED

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/

#include "pNesX.h"
#include "pNesX_System.h"
#include "K6502.h"

extern uint16 PC;
extern unsigned char* pPC;
extern unsigned char* pPC_Offset;
extern unsigned char* BankTable[8];
extern uint16 BankMask[8];

#define REALPC  pPC_Offset = BankTable[ PC >> 13 ] - ( PC & BankMask[ PC >> 13 ] ); pPC = pPC_Offset + PC;
#define VIRPC   PC = pPC - pPC_Offset;

// I/O Operation (User definition)
unsigned char K6502_Read( uint16 wAddr);
uint16 K6502_ReadW( uint16 wAddr );
unsigned char K6502_ReadZp( unsigned char byAddr );
uint16 K6502_ReadZpW( unsigned char byAddr );
unsigned char K6502_ReadAbsX();
unsigned char K6502_ReadAbsY();
unsigned char K6502_ReadIY();

void K6502_Write( uint16 wAddr, unsigned char byData );
void K6502_WriteW( uint16 wAddr, uint16 wData );

extern uint16 PC;
extern unsigned char* pPC;
extern unsigned char* pPC_Offset;
extern unsigned char* BankTable[8];
extern uint16 BankMask[8];

#define REALPC  pPC_Offset = BankTable[ PC >> 13 ] - ( PC & BankMask[ PC >> 13 ] ); pPC = pPC_Offset + PC;
#define VIRPC   PC = pPC - pPC_Offset;

#endif /* !K6502_RW_H_INCLUDED */

/* REVISION
02/27/2000
In K6502_Write, correction: 0x2002 is not writable ( confirmed by Marat's
doc 2.2 and Yoshi's doc 2.0 ).
In K6502_Read, correction: when a register is not readable the upper half
address is returned ( tip from Marat ). This seems to apply to sound
registers too ( the ones < 0x4016 ). This will solve your Dragon Quest
problem because it will return 0x40 = 01000000b, where bit number 6 is set
if bits are 76543210. In Yoshi's doc 0x4015 is readable, in Marat's doc it
is not.
In K6502_Read, correction: added mirroring for values in 0x2000-0x3FFF.
 */
