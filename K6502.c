/*===================================================================*/
/*                                                                   */
/*  K6502.cpp : 6502 Emulator                                        */
/*                                                                   */
/*  1999/10/19  Racoon  New preparation                              */
/*  2001/12/22  ReGex   FrNES 0.60 Final Release - Not much changed  */
/*                      Now uses a jumptable                         */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/

#include "K6502.h"
#include "pNesX.h"

/*-------------------------------------------------------------------*/
/*  Operation Macros                                                 */
/*-------------------------------------------------------------------*/

// Clock Op.
#define CLK(a)   g_wPassedClocks += (a);

// Addressing Op.
// Address
// (Indirect,X)
//#define AA_IX    K6502_ReadZpW( K6502_Read( PC++ ) + X )
//#define AA_IX    K6502_ReadZpW( *(pPC++) + X )
#define AA_IX    ( RAM[ (unsigned char)(*pPC + X) ] | RAM[ (unsigned char)(*pPC + X + 1) ] << 8 )
//#define AA_IX    *(uint16 *)&RAM[ (unsigned char)(*pPC + X) ]
// (Indirect),Y
//#define AA_IY    K6502_ReadZpW( K6502_Read( PC++ ) ) + Y
//#define AA_IY    K6502_ReadZpW( *(pPC++) ) + Y
#define AA_IY    ( RAM[ *pPC ] | RAM[ (unsigned char)(*pPC + 1) ] << 8 ) + Y
//#define AA_IY    *(uint16 *)&RAM[ *pPC ] + Y
// Zero Page
//#define AA_ZP    K6502_Read( PC++ )
#define AA_ZP    (*(pPC++))
// Zero Page,X
//#define AA_ZPX   (unsigned char)( K6502_Read( PC++ ) + X )
#define AA_ZPX   (unsigned char)( *(pPC++) + X )
// Zero Page,Y
//#define AA_ZPY   (unsigned char)( K6502_Read( PC++ ) + Y )
#define AA_ZPY   (unsigned char)( *(pPC++) + Y )
// Absolute
//#define AA_ABS   ( K6502_Read( PC++ ) | (uint16)K6502_Read( PC++ ) << 8 )
#define AA_ABS   ( *pPC | *(pPC + 1) << 8 )
//#define AA_ABS   *(uint16 *)pPC
// Absolute2 ( PC-- )
//#define AA_ABS2  ( K6502_Read( PC++ ) | (uint16)K6502_Read( PC ) << 8 )
#define AA_ABS2   ( *pPC | *(pPC + 1) << 8 )
//#define AA_ABS2  *(uint16 *)pPC
// Absolute,X
//#define AA_ABSX  AA_ABS + X
#define AA_ABSX  AA_ABS + X
// Absolute,Y
//#define AA_ABSY  AA_ABS + Y
#define AA_ABSY  AA_ABS + Y

// Data
// (Indirect,X)
#define A_IX    K6502_Read( AA_IX )
// (Indirect),Y
#define A_IY    K6502_ReadIY()
// Zero Page
#define A_ZP    RAM[ AA_ZP ]
// Zero Page,X
#define A_ZPX   RAM[ AA_ZPX ]
// Zero Page,Y
#define A_ZPY   RAM[ AA_ZPY ]
// Absolute
#define A_ABS   K6502_Read( AA_ABS )
// Absolute,X
#define A_ABSX  K6502_ReadAbsX()
// Absolute,Y
#define A_ABSY  K6502_ReadAbsY()
// Immediate
#define A_IMM   (*(pPC++))

// Flag Op.
#define SETF(a)  F |= (a)
#define RSTF(a)  F &= ~(a)
#define TEST(a)  RSTF( FLAG_N | FLAG_Z ); SETF( g_byTestTable[ a ] )

// Load & Store Op.
#define STA(a)    K6502_Write( (a), A );
#define STX(a)    K6502_Write( (a), X );
#define STY(a)    K6502_Write( (a), Y );
#define LDA(a)    A = (a); TEST( A );
#define LDX(a)    X = (a); TEST( X );
#define LDY(a)    Y = (a); TEST( Y );

// Stack Op.
//#define PUSH(a)   K6502_Write( BASE_STACK + SP--, (a) )
#define PUSH(a)   RAM[ BASE_STACK + SP-- ] = (a)
#define PUSHW(a)  PUSH( (a) >> 8 ); PUSH( a )
//#define PUSHW(a)  *(uint16 *)&RAM[ BASE_STACK + SP - 1 ] = (a); SP -= 2;
//#define POP(a)    a = K6502_Read( BASE_STACK + ++SP )
#define POP(a)    a = RAM[ BASE_STACK + ++SP ]
//#define POPW(a)   POP(a); a |= ( K6502_Read( BASE_STACK + ++SP ) << 8 )
#define POPW(a)   POP(a); a |= ( RAM[ BASE_STACK + ++SP ] << 8 )
//#define POPW(a)   SP += 2; a = *(uint16 *)&RAM[ BASE_STACK + SP - 1 ]

// Logical Op.
#define ORA(a)  A |= (a); TEST( A )
#define AND(a)  A &= (a); TEST( A )
#define EOR(a)  A ^= (a); TEST( A )
#define BIT(a)  byD0 = (a); RSTF( FLAG_N | FLAG_V | FLAG_Z ); SETF( ( byD0 & ( FLAG_N | FLAG_V ) ) | ( ( byD0 & A ) ? 0 : FLAG_Z ) );
#define CMP(a)  wD0 = (uint16)A - (a); RSTF( FLAG_N | FLAG_Z | FLAG_C ); SETF( g_byTestTable[ wD0 & 0xff ] | ( wD0 < 0x100 ? FLAG_C : 0 ) );
#define CPX(a)  wD0 = (uint16)X - (a); RSTF( FLAG_N | FLAG_Z | FLAG_C ); SETF( g_byTestTable[ wD0 & 0xff ] | ( wD0 < 0x100 ? FLAG_C : 0 ) );
#define CPY(a)  wD0 = (uint16)Y - (a); RSTF( FLAG_N | FLAG_Z | FLAG_C ); SETF( g_byTestTable[ wD0 & 0xff ] | ( wD0 < 0x100 ? FLAG_C : 0 ) );
  
// Math Op. (A D flag isn't being supported.)
#define ADC(a)  byD0 = (a); wD0 = A + byD0 + ( F & FLAG_C ); byD1 = (unsigned char)wD0; RSTF( FLAG_N | FLAG_V | FLAG_Z | FLAG_C ); SETF( g_byTestTable[ byD1 ] | ( ( ~( A ^ byD0 ) & ( A ^ byD1 ) & 0x80 ) ? FLAG_V : 0 ) | ( wD0 > 0xff ) ); A = byD1;

#define SBC(a)  byD0 = (a); wD0 = A - byD0 - ( ~F & FLAG_C ); byD1 = (unsigned char)wD0; RSTF( FLAG_N | FLAG_V | FLAG_Z | FLAG_C ); SETF( g_byTestTable[ byD1 ] | ( ( ( A ^ byD0 ) & ( A ^ byD1 ) & 0x80 ) ? FLAG_V : 0 ) | ( wD0 < 0x100 ) ); A = byD1;

#define DEC(a)  wA0 = a; byD0 = K6502_Read( wA0 ); --byD0; K6502_Write( wA0, byD0 ); TEST( byD0 )
#define INC(a)  wA0 = a; byD0 = K6502_Read( wA0 ); ++byD0; K6502_Write( wA0, byD0 ); TEST( byD0 )

// Shift Op.
#define ASLA    RSTF( FLAG_N | FLAG_Z | FLAG_C ); SETF( g_ASLTable[ A ].byFlag ); A = g_ASLTable[ A ].byValue 
#define ASL(a)  RSTF( FLAG_N | FLAG_Z | FLAG_C ); wA0 = a; byD0 = K6502_Read( wA0 ); SETF( g_ASLTable[ byD0 ].byFlag ); K6502_Write( wA0, g_ASLTable[ byD0 ].byValue )
#define LSRA    RSTF( FLAG_N | FLAG_Z | FLAG_C ); SETF( g_LSRTable[ A ].byFlag ); A = g_LSRTable[ A ].byValue 
#define LSR(a)  RSTF( FLAG_N | FLAG_Z | FLAG_C ); wA0 = a; byD0 = K6502_Read( wA0 ); SETF( g_LSRTable[ byD0 ].byFlag ); K6502_Write( wA0, g_LSRTable[ byD0 ].byValue ) 
#define ROLA    byD0 = F & FLAG_C; RSTF( FLAG_N | FLAG_Z | FLAG_C ); SETF( g_ROLTable[ byD0 ][ A ].byFlag ); A = g_ROLTable[ byD0 ][ A ].byValue 
#define ROL(a)  byD1 = F & FLAG_C; RSTF( FLAG_N | FLAG_Z | FLAG_C ); wA0 = a; byD0 = K6502_Read( wA0 ); SETF( g_ROLTable[ byD1 ][ byD0 ].byFlag ); K6502_Write( wA0, g_ROLTable[ byD1 ][ byD0 ].byValue )
#define RORA    byD0 = F & FLAG_C; RSTF( FLAG_N | FLAG_Z | FLAG_C ); SETF( g_RORTable[ byD0 ][ A ].byFlag ); A = g_RORTable[ byD0 ][ A ].byValue 
#define ROR(a)  byD1 = F & FLAG_C; RSTF( FLAG_N | FLAG_Z | FLAG_C ); wA0 = a; byD0 = K6502_Read( wA0 ); SETF( g_RORTable[ byD1 ][ byD0 ].byFlag ); K6502_Write( wA0, g_RORTable[ byD1 ][ byD0 ].byValue )

// Jump Op.
#define JSR     wA0 = AA_ABS2; ++pPC; VIRPC; PUSHW( PC ); PC = wA0; REALPC;
#define BRA(a)  if ( a ) { VIRPC; wA0 = PC; PC += (char)(*pPC); CLK( 3 + ( ( wA0 & 0x0100 ) != ( PC & 0x0100 ) ) ); ++PC; REALPC; } else { ++pPC; CLK( 2 ); }
#define JMP(a)  PC = a; REALPC;

#if 0
#define REALPC  if ( PC != PredPC ) { PredPC = PC; switch ( PC >> 13 ) { case 0x0: pPC_Offset = RAM - ( PC & 0xf800 ); break; case 0x3: pPC_Offset = SRAM - ( PC & 0xe000 ); break; case 0x4: pPC_Offset = ROMBANK0 - ( PC & 0xe000 ); break; case 0x5: pPC_Offset = ROMBANK1 - ( PC & 0xe000 ); break; case 0x6: pPC_Offset = ROMBANK2 - ( PC & 0xe000 ); break; case 0x7: pPC_Offset = ROMBANK3 - ( PC & 0xe000 ); break; } pPC = pPC_Offset + PC; } else pPC = pPC_Offset + PC;
#else
#define REALPC  pPC_Offset = BankTable[ PC >> 13 ] - ( PC & BankMask[ PC >> 13 ] ); pPC = pPC_Offset + PC;
#endif

#define VIRPC   PC = pPC - pPC_Offset;

/*-------------------------------------------------------------------*/
/*  Global valiables                                                 */
/*-------------------------------------------------------------------*/

// 6502 Register
uint16 PC;
unsigned char SP;
unsigned char F;
unsigned char A;
unsigned char X;
unsigned char Y;

unsigned char *pPC;
unsigned char *pPC_Offset;
uint16 PredPC;
unsigned char *pPredPC;

unsigned char *BankTable[ 8 ];
uint16 BankMask[ 8 ] = { 0xf800, 0xf800, 0xf800, 0xe000, 0xe000, 0xe000, 0xe000, 0xe000 };

// The state of the IRQ pin
unsigned char IRQ_State;

// Wiring of the IRQ pin
unsigned char IRQ_Wiring;

// The state of the NMI pin
unsigned char NMI_State;

// Wiring of the NMI pin
unsigned char NMI_Wiring;

// The number of the clocks that it passed
uint16 g_wPassedClocks;

// A table for the test
unsigned char g_byTestTable[ 256 ];

// Value and Flag Data
struct value_table_tag
{
  unsigned char byValue;
  unsigned char byFlag;
};

// A table for ASL
struct value_table_tag g_ASLTable[ 256 ];

// A table for LSR
struct value_table_tag g_LSRTable[ 256 ];

// A table for ROL
struct value_table_tag g_ROLTable[ 2 ][ 256 ];

// A table for ROR
struct value_table_tag g_RORTable[ 2 ][ 256 ];

struct OpcodeTable_tag OpcodeTable[256] =
{
	{Op_00}, {Op_01}, {Op_XX}, {Op_XX}, {Op_XX}, {Op_05}, {Op_06}, {Op_XX}, {Op_08}, {Op_09}, {Op_0A}, {Op_XX}, {Op_XX}, {Op_0D}, {Op_0E}, {Op_XX},
	{Op_10}, {Op_11}, {Op_XX}, {Op_XX}, {Op_XX}, {Op_15}, {Op_16}, {Op_XX}, {Op_18}, {Op_19}, {Op_XX}, {Op_XX}, {Op_XX}, {Op_1D}, {Op_1E}, {Op_XX},
	{Op_20}, {Op_21}, {Op_XX}, {Op_XX}, {Op_24}, {Op_25}, {Op_26}, {Op_XX}, {Op_28}, {Op_29}, {Op_2A}, {Op_XX}, {Op_2C}, {Op_2D}, {Op_2E}, {Op_XX},
	{Op_30}, {Op_31}, {Op_XX}, {Op_XX}, {Op_XX}, {Op_35}, {Op_36}, {Op_XX}, {Op_38}, {Op_39}, {Op_XX}, {Op_XX}, {Op_XX}, {Op_3D}, {Op_3E}, {Op_XX},
	{Op_40}, {Op_41}, {Op_XX}, {Op_XX}, {Op_XX}, {Op_45}, {Op_46}, {Op_XX}, {Op_48}, {Op_49}, {Op_4A}, {Op_XX}, {Op_4C}, {Op_4D}, {Op_4E}, {Op_XX},
	{Op_50}, {Op_51}, {Op_XX}, {Op_XX}, {Op_XX}, {Op_55}, {Op_56}, {Op_XX}, {Op_58}, {Op_59}, {Op_XX}, {Op_XX}, {Op_XX}, {Op_5D}, {Op_5E}, {Op_XX},
	{Op_60}, {Op_61}, {Op_XX}, {Op_XX}, {Op_XX}, {Op_65}, {Op_66}, {Op_XX}, {Op_68}, {Op_69}, {Op_6A}, {Op_XX}, {Op_6C}, {Op_6D}, {Op_6E}, {Op_XX},
	{Op_70}, {Op_71}, {Op_XX}, {Op_XX}, {Op_XX}, {Op_75}, {Op_76}, {Op_XX}, {Op_78}, {Op_79}, {Op_XX}, {Op_XX}, {Op_XX}, {Op_7D}, {Op_7E}, {Op_XX},
	{Op_XX}, {Op_81}, {Op_XX}, {Op_XX}, {Op_84}, {Op_85}, {Op_86}, {Op_XX}, {Op_88}, {Op_XX}, {Op_8A}, {Op_XX}, {Op_8C}, {Op_8D}, {Op_8E}, {Op_XX},
	{Op_90}, {Op_91}, {Op_XX}, {Op_XX}, {Op_94}, {Op_95}, {Op_96}, {Op_XX}, {Op_98}, {Op_99}, {Op_9A}, {Op_XX}, {Op_XX}, {Op_9D}, {Op_XX}, {Op_XX},
	{Op_A0}, {Op_A1}, {Op_A2}, {Op_XX}, {Op_A4}, {Op_A5}, {Op_A6}, {Op_XX}, {Op_A8}, {Op_A9}, {Op_AA}, {Op_XX}, {Op_AC}, {Op_AD}, {Op_AE}, {Op_XX},
	{Op_B0}, {Op_B1}, {Op_XX}, {Op_XX}, {Op_B4}, {Op_B5}, {Op_B6}, {Op_XX}, {Op_B8}, {Op_B9}, {Op_BA}, {Op_XX}, {Op_BC}, {Op_BD}, {Op_BE}, {Op_XX},
	{Op_C0}, {Op_C1}, {Op_XX}, {Op_XX}, {Op_C4}, {Op_C5}, {Op_C6}, {Op_XX}, {Op_C8}, {Op_C9}, {Op_CA}, {Op_XX}, {Op_CC}, {Op_CD}, {Op_CE}, {Op_XX},
	{Op_D0}, {Op_D1}, {Op_XX}, {Op_XX}, {Op_XX}, {Op_D5}, {Op_D6}, {Op_XX}, {Op_D8}, {Op_D9}, {Op_XX}, {Op_XX}, {Op_XX}, {Op_DD}, {Op_DE}, {Op_XX},
	{Op_E0}, {Op_E1}, {Op_XX}, {Op_XX}, {Op_E4}, {Op_E5}, {Op_E6}, {Op_XX}, {Op_E8}, {Op_E9}, {Op_EA}, {Op_XX}, {Op_EC}, {Op_ED}, {Op_EE}, {Op_XX},
	{Op_F0}, {Op_F1}, {Op_XX}, {Op_XX}, {Op_XX}, {Op_F5}, {Op_F6}, {Op_XX}, {Op_F8}, {Op_F9}, {Op_XX}, {Op_XX}, {Op_XX}, {Op_FD}, {Op_FE}, {Op_XX}
};

uint16 wA0;
unsigned char byD0;
unsigned char byD1;
uint16 wD0;

void Op_00()
{
	VIRPC; ++PC; PUSHW( PC ); SETF( FLAG_B ); PUSH( F ); SETF( FLAG_I ); RSTF( FLAG_D ); PC = K6502_ReadW( VECTOR_IRQ ); REALPC; CLK( 7 );
}

void Op_01()  // ORA (Zpg,X)
{
	ORA( A_IX ); ++pPC; CLK( 6 );
}

void Op_05()  // ORA Zpg
{
	ORA( A_ZP ); CLK( 3 );
}

void Op_06()  // ASL Zpg
{
	ASL( AA_ZP ); CLK( 5 );
}

void Op_08()  // PHP
{
	PUSH( F ); CLK( 3 );
}

void Op_09()  // ORA #Oper
{
	ORA( A_IMM ); CLK( 2 );
}

void Op_0A()  // ASL A
{
	ASLA; CLK( 2 );
}

void Op_0D  () // ORA Abs
{
	ORA( A_ABS ); pPC += 2; CLK( 4 );
}

void Op_0E  () // ASL Abs
{
	ASL( AA_ABS ); pPC += 2; CLK( 6 );
}

void Op_10 () // BPL Oper
{
	BRA( !( F & FLAG_N ) );
}

void Op_11 () // ORA (Zpg),Y
{
	ORA( A_IY ); CLK( 5 );
}

void Op_15 () // ORA Zpg,X
{
	ORA( A_ZPX ); CLK( 4 );
}

void Op_16 () // ASL Zpg,X
{
	ASL( AA_ZPX ); CLK( 6 );
}

void Op_18 () // CLC
{
	RSTF( FLAG_C ); CLK( 2 );
}

void Op_19 () // ORA Abs,Y
{
	ORA( A_ABSY ); CLK( 4 );
}

void Op_1D () // ORA Abs,X
{
	ORA( A_ABSX ); CLK( 4 );
}

void Op_1E () // ASL Abs,X
{
	ASL( AA_ABSX ); pPC += 2; CLK( 7 );
}

void Op_20 () // JSR Abs
{
	JSR; CLK( 6 );
}

void Op_21 () // AND (Zpg,X)
{
	AND( A_IX ); ++pPC; CLK( 6 );
}

void Op_24 () // BIT Zpg
{
	BIT( A_ZP ); CLK( 3 );
}

void Op_25 () // AND Zpg
{
	AND( A_ZP ); CLK( 3 );
}

void Op_26 () // ROL Zpg
{
	ROL( AA_ZP ); CLK( 5 );
}

void Op_28 () // PLP
{
	POP( F ); SETF( FLAG_R ); CLK( 4 );
}

void Op_29 () // AND #Oper
{
	AND( A_IMM ); CLK( 2 );
}

void Op_2A () // ROL A
{
	ROLA; CLK( 2 );
}

void Op_2C () // BIT Abs
{
	BIT( A_ABS ); pPC += 2; CLK( 4 );
}

void Op_2D () // AND Abs 
{
	AND( A_ABS ); pPC += 2; CLK( 4 );
}

void Op_2E () // ROL Abs
{
	ROL( AA_ABS ); pPC += 2; CLK( 6 );
}

void Op_30 () // BMI Oper 
{
	BRA( F & FLAG_N );
}

void Op_31 () // AND (Zpg),Y
{
	AND( A_IY ); CLK( 5 );
}

void Op_35 () // AND Zpg,X
{
	AND( A_ZPX ); CLK( 4 );
}

void Op_36 () // ROL Zpg,X
{
	ROL( AA_ZPX ); CLK( 6 );
}

void Op_38 () // SEC
{
	SETF( FLAG_C ); CLK( 2 );
}

void Op_39 () // AND Abs,Y
{
	AND( A_ABSY ); CLK( 4 );
}

void Op_3D () // AND Abs,X
{
	AND( A_ABSX ); CLK( 4 );
}

void Op_3E () // ROL Abs,X
{
	ROL( AA_ABSX ); pPC += 2; CLK( 7 );
}

void Op_40 () // RTI
{
	POP( F ); SETF( FLAG_R ); POPW( PC ); REALPC; CLK( 6 );
}

void Op_41 () // EOR (Zpg,X)
{
	EOR( A_IX ); ++pPC; CLK( 6 );
}

void Op_45 () // EOR Zpg
{
	EOR( A_ZP ); CLK( 3 );
}

void Op_46 () // LSR Zpg
{
	LSR( AA_ZP ); CLK( 5 );
}

void Op_48 () // PHA
{
	PUSH( A ); CLK( 3 );
}

void Op_49 () // EOR #Oper
{
	EOR( A_IMM ); CLK( 2 );
}

void Op_4A () // LSR A
{
	LSRA; CLK( 2 );
}

void Op_4C () // JMP Abs
{
	JMP( AA_ABS ); CLK( 3 );
}

void Op_4D () // EOR Abs
{
	EOR( A_ABS ); pPC += 2; CLK( 4 );
}

void Op_4E () // LSR Abs
{
	LSR( AA_ABS ); pPC += 2; CLK( 6 );
}

void Op_50 () // BVC
{
	BRA( !( F & FLAG_V ) );
}

void Op_51 () // EOR (Zpg),Y
{
	EOR( A_IY ); CLK( 5 );
}

void Op_55 () // EOR Zpg,X
{
	EOR( A_ZPX ); CLK( 4 );
}

void Op_56 () // LSR Zpg,X
{
	LSR( AA_ZPX ); CLK( 6 );
}

void Op_58 () // CLI
{
	byD0 = F;
    RSTF( FLAG_I ); CLK( 2 );
    if ( ( byD0 & FLAG_I ) && IRQ_State == 0 )
    {
      // IRQ Interrupt
      // Execute IRQ if an I flag isn't being set
      if ( !( F & FLAG_I ) )
      {
        CLK( 7 );

        VIRPC;
        PUSHW( PC );
        PUSH( F & ~FLAG_B );

        RSTF( FLAG_D );
        SETF( FLAG_I );

        PC = K6502_ReadW( VECTOR_IRQ );
        REALPC;
      }
    }
}

void Op_59 () // EOR Abs,Y
{
	EOR( A_ABSY ); CLK( 4 );
}

void Op_5D () // EOR Abs,X
{
	EOR( A_ABSX ); CLK( 4 );
}

void Op_5E () // LSR Abs,X
{
	LSR( AA_ABSX ); pPC += 2; CLK( 7 );
}

void Op_60 () // RTS
{
	POPW( PC ); ++PC; REALPC; CLK( 6 );
}

void Op_61 () // ADC (Zpg,X)
{
	ADC( A_IX ); ++pPC; CLK( 6 );
}

void Op_65 () // ADC Zpg
{
	ADC( A_ZP ); CLK( 3 );
}

void Op_66 () // ROR Zpg
{
	ROR( AA_ZP ); CLK( 5 );
}

void Op_68 () // PLA
{
	POP( A ); TEST( A ); CLK( 4 );
}

void Op_69 () // ADC #Oper
{
	ADC( A_IMM ); CLK( 2 );
}

void Op_6A () // ROR A
{
	RORA; CLK( 2 );
}

void Op_6C () // JMP (Abs)
{
	JMP( K6502_ReadW( AA_ABS ) ); CLK( 5 );
}

void Op_6D () // ADC Abs
{
	ADC( A_ABS ); pPC += 2; CLK( 4 );
}

void Op_6E () // ROR Abs
{
	ROR( AA_ABS ); pPC += 2; CLK( 6 );
}

void Op_70 () // BVS
{
	BRA( F & FLAG_V );
}
void Op_71 () // ADC (Zpg),Y
{
	ADC( A_IY ); CLK( 5 );
}

void Op_75 () // ADC Zpg,X
{
	ADC( A_ZPX ); CLK( 4 );
}

void Op_76 () // ROR Zpg,X
{
	ROR( AA_ZPX ); CLK( 6 );
}

void Op_78 () // SEI
{
	SETF( FLAG_I ); CLK( 2 );
}

void Op_79 () // ADC Abs,Y
{
	ADC( A_ABSY ); CLK( 4 );
}

void Op_7D () // ADC Abs,X
{
    ADC( A_ABSX ); CLK( 4 );
}

void Op_7E () // ROR Abs,X
{
	ROR( AA_ABSX ); pPC += 2; CLK( 7 );
}

void Op_81 () // STA (Zpg,X)
{
	STA( AA_IX ); ++pPC; CLK( 6 );
}
      
void Op_84 () // STY Zpg
{
	STY( AA_ZP ); CLK( 3 );
}

void Op_85 () // STA Zpg
{
	STA( AA_ZP ); CLK( 3 );
}

void Op_86 () // STX Zpg
{
	STX( AA_ZP ); CLK( 3 );
}

void Op_88 () // DEY
{
	--Y; TEST( Y ); CLK( 2 );
}

void Op_8A () // TXA
{
	A = X; TEST( A ); CLK( 2 );
}

void Op_8C () // STY Abs
{
	STY( AA_ABS ); pPC += 2; CLK( 4 );
}

void Op_8D () // STA Abs
{
	STA( AA_ABS ); pPC += 2; CLK( 4 );
}

void Op_8E () // STX Abs
{
	STX( AA_ABS ); pPC += 2; CLK( 4 );
}

void Op_90 () // BCC
{
	BRA( !( F & FLAG_C ) );
}

void Op_91 () // STA (Zpg),Y
{
	STA( AA_IY ); ++pPC; CLK( 6 );
}

void Op_94 () // STY Zpg,X
{
	STY( AA_ZPX ); CLK( 4 );
}

void Op_95 () // STA Zpg,X
{
	STA( AA_ZPX ); CLK( 4 );
}

void Op_96 () // STX Zpg,Y
{
	STX( AA_ZPY ); CLK( 4 );
}

void Op_98 () // TYA
{
	A = Y; TEST( A ); CLK( 2 );
}

void Op_99 () // STA Abs,Y
{
	STA( AA_ABSY ); pPC += 2; CLK( 5 );
}

void Op_9A () // TXS
{
	SP = X; CLK( 2 );
}

void Op_9D () // STA Abs,X
{
	STA( AA_ABSX ); pPC += 2; CLK( 5 );
}

void Op_A0 () // LDY #Oper
{
	LDY( A_IMM ); CLK( 2 );
}

void Op_A1 () // LDA (Zpg,X)
{
	LDA( A_IX ); ++pPC; CLK( 6 );
}

void Op_A2 () // LDX #Oper
{
	LDX( A_IMM ); CLK( 2 );
}

void Op_A4 () // LDY Zpg
{
	LDY( A_ZP ); CLK( 3 );
}

void Op_A5 () // LDA Zpg
{
	LDA( A_ZP ); CLK( 3 );
}

void Op_A6 () // LDX Zpg
{
	LDX( A_ZP ); CLK( 3 );
}

void Op_A8 () // TAY
{
	Y = A; TEST( A ); CLK( 2 );
}

void Op_A9 () // LDA #Oper
{
	LDA( A_IMM ); CLK( 2 );
}

void Op_AA () // TAX
{
	X = A; TEST( A ); CLK( 2 );
}

void Op_AC () // LDY Abs
{
	LDY( A_ABS ); pPC += 2; CLK( 4 );
}

void Op_AD () // LDA Abs
{
	LDA( A_ABS ); pPC += 2; CLK( 4 );
}

void Op_AE () // LDX Abs
{
	LDX( A_ABS ); pPC += 2; CLK( 4 );
}

void Op_B0 () // BCS
{
	BRA( F & FLAG_C );
}

void Op_B1 () // LDA (Zpg),Y
{
	LDA( A_IY ); CLK( 5 );
}

void Op_B4 () // LDY Zpg,X
{
	LDY( A_ZPX ); CLK( 4 );
}

void Op_B5 () // LDA Zpg,X
{
	LDA( A_ZPX ); CLK( 4 );
}

void Op_B6 () // LDX Zpg,Y
{
	LDX( A_ZPY ); CLK( 4 );
}
void Op_B8 () // CLV
{
	RSTF( FLAG_V ); CLK( 2 );
}

void Op_B9 () // LDA Abs,Y
{
	LDA( A_ABSY ); CLK( 4 );
}

void Op_BA () // TSX
{
	X = SP; TEST( X ); CLK( 2 );
}

void Op_BC () // LDY Abs,X
{
	LDY( A_ABSX ); CLK( 4 );
}

void Op_BD () // LDA Abs,X
{
	LDA( A_ABSX ); CLK( 4 );
}

void Op_BE () // LDX Abs,Y
{
	LDX( A_ABSY ); CLK( 4 );
}

void Op_C0 () // CPY #Oper
{
	CPY( A_IMM ); CLK( 2 );
}

void Op_C1 () // CMP (Zpg,X)
{
	CMP( A_IX ); ++pPC; CLK( 6 );
}

void Op_C4 () // CPY Zpg
{
	CPY( A_ZP ); CLK( 3 );
}

void Op_C5 () // CMP Zpg
{
	CMP( A_ZP ); CLK( 3 );
}

void Op_C6 () // DEC Zpg
{
	DEC( AA_ZP ); CLK( 5 );
}

void Op_C8 () // INY
{
	++Y; TEST( Y ); CLK( 2 );
}

void Op_C9 () // CMP #Oper
{
	CMP( A_IMM ); CLK( 2 );
}

void Op_CA () // DEX
{
	--X; TEST( X ); CLK( 2 );
}

void Op_CC () // CPY Abs
{
	CPY( A_ABS ); pPC += 2; CLK( 4 );
}
void Op_CD () // CMP Abs
{
	CMP( A_ABS ); pPC += 2; CLK( 4 );
}

void Op_CE () // DEC Abs
{
	DEC( AA_ABS ); pPC += 2;CLK( 6 );
}

void Op_D0 () // BNE
{
	BRA( !( F & FLAG_Z ) );
}

void Op_D1 () // CMP (Zpg),Y
{
	CMP( A_IY ); CLK( 5 );
}

void Op_D5 () // CMP Zpg,X
{
	CMP( A_ZPX ); CLK( 4 );
}
        
void Op_D6 () // DEC Zpg,X
{
	DEC( AA_ZPX ); CLK( 6 );
}

void Op_D8 () // CLD
{
	RSTF( FLAG_D ); CLK( 2 );
}

void Op_D9 () // CMP Abs,Y
{
	CMP( A_ABSY ); CLK( 4 );
}

void Op_DD () // CMP Abs,X
{
	CMP( A_ABSX ); CLK( 4 );
}

void Op_DE () // DEC Abs,X
{
	DEC( AA_ABSX ); pPC += 2; CLK( 7 );
}

void Op_E0 () // CPX #Oper
{
	CPX( A_IMM ); CLK( 2 );
}

void Op_E1 () // SBC (Zpg,X)
{
	SBC( A_IX ); ++pPC; CLK( 6 );
}

void Op_E4 () // CPX Zpg
{
	CPX( A_ZP ); CLK( 3 );
}

void Op_E5 () // SBC Zpg
{
	SBC( A_ZP ); CLK( 3 );
}

void Op_E6 () // INC Zpg
{
	INC( AA_ZP ); CLK( 5 );
}

void Op_E8 () // INX
{
	++X; TEST( X ); CLK( 2 );
}
void Op_E9 () // SBC #Oper
{
	SBC( A_IMM ); CLK( 2 );
}

void Op_EA () // NOP
{
	CLK( 2 );
}

void Op_EC () // CPX Abs
{
	CPX( A_ABS ); pPC += 2; CLK( 4 );
}

void Op_ED () // SBC Abs
{
	SBC( A_ABS ); pPC += 2; CLK( 4 );
}

void Op_EE () // INC Abs
{
	INC( AA_ABS ); pPC += 2; CLK( 6 );
}

void Op_F0 () // BEQ
{
	BRA( F & FLAG_Z );
}

void Op_F1 () // SBC (Zpg),Y
{
	SBC( A_IY ); CLK( 5 );
}

void Op_F5 () // SBC Zpg,X
{
	SBC( A_ZPX ); CLK( 4 );
}

void Op_F6 () // INC Zpg,X
{
	INC( AA_ZPX ); CLK( 6 );
}

void Op_F8 () // SED
{
	SETF( FLAG_D ); CLK( 2 );
}

void Op_F9 () // SBC Abs,Y
{
	SBC( A_ABSY ); CLK( 4 );
}

void Op_FD () // SBC Abs,X
{
	SBC( A_ABSX ); CLK( 4 );
}

void Op_FE () // INC Abs,X
{
	INC( AA_ABSX ); pPC += 2; CLK( 7 );
}

void Op_XX () // Unknown Instruction
{
	CLK( 2 );
}

/*===================================================================*/
/*                                                                   */
/*                K6502_Init() : Initialize K6502                    */
/*                                                                   */
/*===================================================================*/
void K6502_Init()
{
/*
 *  Initialize K6502
 *
 *  You must call this function only once at first.
 */
  unsigned char idx;
  unsigned char idx2;

  // The establishment of the IRQ pin
  NMI_Wiring = NMI_State = 1;
  IRQ_Wiring = IRQ_State = 1;

  // Make a table for the test
  idx = 0;
  do
  {
    if ( idx == 0 )
      g_byTestTable[ 0 ] = FLAG_Z;
    else
    if ( idx > 127 )
      g_byTestTable[ idx ] = FLAG_N;
    else
      g_byTestTable[ idx ] = 0;

    ++idx;
  } while ( idx != 0 );

  // Make a table ASL
  idx = 0;
  do
  {
    g_ASLTable[ idx ].byValue = idx << 1;
    g_ASLTable[ idx ].byFlag = 0;

    if ( idx > 127 )
      g_ASLTable[ idx ].byFlag = FLAG_C;

    if ( g_ASLTable[ idx ].byValue == 0 )
      g_ASLTable[ idx ].byFlag |= FLAG_Z;
    else
    if ( g_ASLTable[ idx ].byValue & 0x80 )
      g_ASLTable[ idx ].byFlag |= FLAG_N;

    ++idx;
  } while ( idx != 0 );

  // Make a table LSR
  idx = 0;
  do
  {
    g_LSRTable[ idx ].byValue = idx >> 1;
    g_LSRTable[ idx ].byFlag = 0;

    if ( idx & 1 )
      g_LSRTable[ idx ].byFlag = FLAG_C;

    if ( g_LSRTable[ idx ].byValue == 0 )
      g_LSRTable[ idx ].byFlag |= FLAG_Z;

    ++idx;
  } while ( idx != 0 );

  // Make a table ROL
  for ( idx2 = 0; idx2 < 2; ++idx2 )
  {
    idx = 0;
    do
    {
      g_ROLTable[ idx2 ][ idx ].byValue = ( idx << 1 ) | idx2;
      g_ROLTable[ idx2 ][ idx ].byFlag = 0;

      if ( idx > 127 )
        g_ROLTable[ idx2 ][ idx ].byFlag = FLAG_C;

      if ( g_ROLTable[ idx2 ][ idx ].byValue == 0 )
        g_ROLTable[ idx2 ][ idx ].byFlag |= FLAG_Z;
      else
      if ( g_ROLTable[ idx2 ][ idx ].byValue & 0x80 )
        g_ROLTable[ idx2 ][ idx ].byFlag |= FLAG_N;

      ++idx;
    } while ( idx != 0 );
  }

  // Make a table ROR
  for ( idx2 = 0; idx2 < 2; ++idx2 )
  {
    idx = 0;
    do
    {
      g_RORTable[ idx2 ][ idx ].byValue = ( idx >> 1 ) | ( idx2 << 7 );
      g_RORTable[ idx2 ][ idx ].byFlag = 0;

      if ( idx & 1 )
        g_RORTable[ idx2 ][ idx ].byFlag = FLAG_C;

      if ( g_RORTable[ idx2 ][ idx ].byValue == 0 )
        g_RORTable[ idx2 ][ idx ].byFlag |= FLAG_Z;
      else
      if ( g_RORTable[ idx2 ][ idx ].byValue & 0x80 )
        g_RORTable[ idx2 ][ idx ].byFlag |= FLAG_N;

      ++idx;
    } while ( idx != 0 );
  }
}

/*===================================================================*/
/*                                                                   */
/*                K6502_Reset() : Reset a CPU                        */
/*                                                                   */
/*===================================================================*/
void K6502_Reset()
{
/*
 *  Reset a CPU
 *
 */
  // Reset Bank Table
  BankTable[ 0 ] = RAM;
  BankTable[ 1 ] = RAM;
  BankTable[ 2 ] = RAM;
  BankTable[ 3 ] = SRAM;
  BankTable[ 4 ] = ROMBANK0;
  BankTable[ 5 ] = ROMBANK1;
  BankTable[ 6 ] = ROMBANK2;
  BankTable[ 7 ] = ROMBANK3;

  // Reset Registers
  PC = K6502_ReadW( VECTOR_RESET );
  PredPC = 0;
  REALPC;
  SP = 0xFF;
  A = X = Y = 0;
  F = FLAG_Z | FLAG_R;

  // Set up the state of the Interrupt pin.
  NMI_State = NMI_Wiring;
  IRQ_State = IRQ_Wiring;

  // Reset Passed Clocks
  g_wPassedClocks = 0;
}

/*===================================================================*/
/*                                                                   */
/*    K6502_Set_Int_Wiring() : Set up wiring of the interrupt pin    */
/*                                                                   */
/*===================================================================*/
void K6502_Set_Int_Wiring( unsigned char byNMI_Wiring, unsigned char byIRQ_Wiring )
{
/*
 * Set up wiring of the interrupt pin
 *
 */

  NMI_Wiring = byNMI_Wiring;
  IRQ_Wiring = byIRQ_Wiring;
}

void K6502_DoNMI()
{
	if (NMI_State != NMI_Wiring)
	{
		// NMI Interrupt
		CLK( 7 );

		VIRPC;
		PUSHW( PC );
		PUSH( F & ~FLAG_B );

		RSTF( FLAG_D );

		PC = K6502_ReadW( VECTOR_NMI );
		REALPC;

		NMI_State = NMI_Wiring;
	}
}

/*===================================================================*/
/*                                                                   */
/*  K6502_Step() :                                                   */
/*          Only the specified number of the clocks execute Op.      */
/*                                                                   */
/*===================================================================*/
void K6502_Step( uint16 wClocks )
{
/*
 *  Only the specified number of the clocks execute Op.
 *
 *  Parameters
 *    uint16 wClocks              (Read)
 *      The number of the clocks
 */

  wA0 = 0;
  byD0 = 0;
  byD1 = 0;
  wD0 = 0;
  
  // Moved to it's own separate function for the purpose of more accurate emulation
  // ex-  Dispose of it if there is an interrupt requirement
/*  if ( NMI_State != NMI_Wiring )
  {
    // NMI Interrupt
    CLK( 7 );

    VIRPC;
    PUSHW( PC );
    PUSH( F & ~FLAG_B );

    RSTF( FLAG_D );

    PC = K6502_ReadW( VECTOR_NMI );
    REALPC;
  }
  else*/
  if ( IRQ_State != IRQ_Wiring )
  {
    // IRQ Interrupt
    // Execute IRQ if an I flag isn't being set
    if ( !( F & FLAG_I ) )
    {
      CLK( 7 );

      VIRPC;
      PUSHW( PC );
      PUSH( F & ~FLAG_B );

      RSTF( FLAG_D );
      SETF( FLAG_I );
    
      PC = K6502_ReadW( VECTOR_IRQ );
      REALPC;
    }
  }

  //Reset the state
//  NMI_State = NMI_Wiring;
  IRQ_State = IRQ_Wiring;

  // It has a loop until a constant clock passes
  while ( g_wPassedClocks < wClocks )
  {
    // Read an instruction
    //byCode = K6502_Read( PC++ );
    //byCode = *(pPC++);
    
    // Execute an instruction, run the instruction from the jump table.
	((OpcodeTable[*(pPC++)]).pFPtr)();
  }  /* end of while ... */

  // Correct the number of the clocks
  g_wPassedClocks -= wClocks;
}

// Addressing Op.
// Data
// Absolute,X
inline unsigned char K6502_ReadAbsX(){ uint16 wA0, wA1; wA0 = AA_ABS; pPC += 2; wA1 = wA0 + X; CLK( ( wA0 & 0x0100 ) != ( wA1 & 0x0100 ) ); return K6502_Read( wA1 ); };
// Absolute,Y
inline unsigned char K6502_ReadAbsY(){ uint16 wA0, wA1; wA0 = AA_ABS; pPC += 2; wA1 = wA0 + Y; CLK( ( wA0 & 0x0100 ) != ( wA1 & 0x0100 ) ); return K6502_Read( wA1 ); };
// (Indirect),Y
//static inline unsigned char K6502_ReadIY(){ uint16 wA0, wA1; wA0 = K6502_ReadZpW( K6502_Read( PC++ ) ); wA1 = wA0 + Y; CLK( ( wA0 & 0x0100 ) != ( wA1 & 0x0100 ) ); return K6502_Read( wA1 ); };
inline unsigned char K6502_ReadIY(){ uint16 wA0, wA1; wA0 = RAM[ *pPC ] | RAM[ (unsigned char)(*pPC + 1) ] << 8; ++pPC; wA1 = wA0 + Y; CLK( ( wA0 & 0x0100 ) != ( wA1 & 0x0100 ) ); return K6502_Read( wA1 ); };
//static inline unsigned char K6502_ReadIY(){ uint16 wA0, wA1; wA0 = *(uint16 *)&RAM[ *pPC ]; ++pPC; wA1 = wA0 + Y; CLK( ( wA0 & 0x0100 ) != ( wA1 & 0x0100 ) ); return K6502_Read( wA1 ); };