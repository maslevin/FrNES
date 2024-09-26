/*===================================================================*/
/*                                                                   */
/*  K6502.h : NES 6502 header                                        */
/*                                                                   */
/*  1999/11/03  Racoon  New preparation                              */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#ifndef K6502_H_INCLUDED
#define K6502_H_INCLUDED

#include <kos.h>
#include "K6502_rw.h"
// Type definition

/* 6502 Flags */
#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_R 0x20
#define FLAG_V 0x40
#define FLAG_N 0x80

/* Stack Address */
#define BASE_STACK   0x100

/* Interrupt Vectors */
#define VECTOR_NMI   0xfffa
#define VECTOR_RESET 0xfffc
#define VECTOR_IRQ   0xfffe

// NMI Request
#define NMI_REQ  NMI_State = 0;

// IRQ Request
#define IRQ_REQ  IRQ_State = 0;

// Emulator Operation
void K6502_Init();
void K6502_Reset();
void K6502_Set_Int_Wiring( unsigned char byNMI_Wiring, unsigned char byIRQ_Wiring );
void K6502_Step( register uint16 wClocks );
void K6502_Burn(uint16 wClocks);
uint32 K6502_GetCycles();

void K6502_DoNMI();
void K6502_DoIRQ();

// The state of the IRQ pin
extern unsigned char IRQ_State;

// The state of the NMI pin
extern unsigned char NMI_State;

extern unsigned char* BankTable[8];

extern unsigned char HALT;

#ifdef DEBUG
void EndTracing();
#endif

#endif /* !K6502_H_INCLUDED */