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

struct OpcodeTable_tag
{
	void (*pFPtr)();	
};

#ifdef DEBUG
void UploadDisassembly();
#endif

extern struct OpcodeTable_tag OpcodeTable[256];

void Op_00();
void Op_01();
void Op_05();
void Op_06();
void Op_08();
void Op_09();
void Op_0A();
void Op_0D();
void Op_0E();
void Op_10();
void Op_11();
void Op_15();
void Op_16();
void Op_18();
void Op_19();
void Op_1D();
void Op_1E();
void Op_20();
void Op_21();
void Op_24();
void Op_25();
void Op_26();
void Op_28();
void Op_29();
void Op_2A();
void Op_2C();
void Op_2D(); 
void Op_2E();
void Op_30(); 
void Op_31();
void Op_35();
void Op_36 ();
void Op_38 ();
void Op_39 ();
void Op_3D ();
void Op_3E ();
void Op_40 ();
void Op_41 ();
void Op_45 ();
void Op_46 ();
void Op_48 ();
void Op_49 ();
void Op_4A ();
void Op_4C ();
void Op_4D ();
void Op_4E ();
void Op_50 ();
void Op_51 ();
void Op_55 ();
void Op_56 ();
void Op_58 ();
void Op_59 ();
void Op_5D ();
void Op_5E ();
void Op_60 ();
void Op_61 ();
void Op_65 ();
void Op_66 ();
void Op_68 ();
void Op_69 ();
void Op_6A ();
void Op_6C ();
void Op_6D ();
void Op_6E ();
void Op_70 ();
void Op_71 ();
void Op_75 ();
void Op_76 ();
void Op_78 ();
void Op_79 ();
void Op_7D ();
void Op_7E ();
void Op_81 ();
void Op_84 ();
void Op_85 ();
void Op_86 ();
void Op_88 ();
void Op_89 ();
void Op_8A ();
void Op_8C ();
void Op_8D ();
void Op_8E ();
void Op_90 ();
void Op_91 ();
void Op_94 ();
void Op_95 ();
void Op_96 ();
void Op_98 ();
void Op_99 ();
void Op_9A ();
void Op_9D ();
void Op_A0 ();
void Op_A1 ();
void Op_A2 ();
void Op_A4 ();
void Op_A5 ();
void Op_A6 ();
void Op_A8 ();
void Op_A9 ();
void Op_AA ();
void Op_AC ();
void Op_AD ();
void Op_AE ();
void Op_B0 ();
void Op_B1 ();
void Op_B4 ();
void Op_B5 ();
void Op_B6 ();
void Op_B8 ();
void Op_B9 ();
void Op_BA ();
void Op_BC ();
void Op_BD ();
void Op_BE ();
void Op_C0 ();
void Op_C1 ();
void Op_C4 ();
void Op_C5 ();
void Op_C6 ();
void Op_C8 ();
void Op_C9 ();
void Op_CA ();
void Op_CC ();
void Op_CD ();
void Op_CE ();
void Op_D0();
void Op_D1();
void Op_D5();
void Op_D6();
void Op_D8();
void Op_D9();
void Op_DD();
void Op_DE();
void Op_E0();
void Op_E1();
void Op_E4();
void Op_E5();
void Op_E6();
void Op_E8();
void Op_E9();
void Op_EA();
void Op_EC();
void Op_ED();
void Op_EE();
void Op_F0();
void Op_F1();
void Op_F5();
void Op_F6();
void Op_F8();
void Op_F9();
void Op_FD();
void Op_FE();
void Op_XX();

#endif /* !K6502_H_INCLUDED */