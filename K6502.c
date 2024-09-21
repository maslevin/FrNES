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
#include "macros.h"

#include "pNesX.h"
#include "profile.h"

/*-------------------------------------------------------------------*/
/*  Operation Macros                                                 */
/*-------------------------------------------------------------------*/

// Clock Op.
#define CLK(a)   g_wPassedClocks += (a); totalClocks += (a);

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
#define AA_ABS   ( *pPC | *(pPC + 1) << 8 )
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

/*-------------------------------------------------------------------*/
/*  Global valiables                                                 */
/*-------------------------------------------------------------------*/

unsigned char HALT;

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
uint32 totalClocks;

// A table for the test
uint8 g_byTestTable[ 256 ];

// Value and Flag Data
typedef struct __attribute__ ((packed, aligned(4))) {
	unsigned char byValue;
	unsigned char byFlag;
} value_table_tag;

// A table for ASL
value_table_tag g_ASLTable[ 256 ];

// A table for LSR
value_table_tag g_LSRTable[ 256 ];

// A table for ROL
value_table_tag g_ROLTable[ 2 ][ 256 ];

// A table for ROR
value_table_tag g_RORTable[ 2 ][ 256 ];

uint16 wA0;
unsigned char byD0;
unsigned char byD1;
uint16 wD0;

#ifdef DEBUG
#define MAX_DISASM_STEPS 131072
#define MAX_DISASM_STRING 20
char DisassemblyBuffer[MAX_DISASM_STEPS][MAX_DISASM_STRING];

uint16 DisassemblyBufferIndex = 0;

void DisassembleInstruction(char* Opcode, char* fmt, uint16 value) {
	VIRPC;
	char p[16];
	if (fmt) {
		snprintf(p, 16, fmt, value);
	} else {
		p[0] = '\0';
	}
	snprintf(DisassemblyBuffer[DisassemblyBufferIndex], MAX_DISASM_STRING, "$%04x: %s %s", PC - 1, Opcode, p); 
	DisassemblyBufferIndex++;
	DisassemblyBufferIndex %= MAX_DISASM_STEPS;
	REALPC;
}

void DisassembleInstruction2(char* Opcode, char* fmt, uint16 value, uint16 value2) {
	VIRPC;
	char p[16];
	if (fmt) {
		snprintf(p, 16, fmt, value, value2);
	} else {
		p[0] = '\0';
	}
	snprintf(DisassemblyBuffer[DisassemblyBufferIndex], MAX_DISASM_STRING, "$%04x: %s %s", PC - 1, Opcode, p); 
	DisassemblyBufferIndex++;
	DisassemblyBufferIndex %= MAX_DISASM_STEPS;
	REALPC;
}

void UploadDisassembly() {
	printf("Uploading Disassembly to PC Host\n");
	char PCPath[256];
	// TODO: use a log time or something like that in the filename here
	snprintf(PCPath, 256, "/pc/Users/maslevin/Documents/Projects/numechanix/frnes/disasm.txt");	
	file_t PCFile = fs_open(PCPath, O_WRONLY);
	if (PCFile != -1) {
		int bufferIndex = DisassemblyBufferIndex;
		while (bufferIndex != DisassemblyBufferIndex - 1) {
			if (strcmp(DisassemblyBuffer[bufferIndex], "") != 0) {
				fs_write(PCFile, DisassemblyBuffer[bufferIndex], strlen(DisassemblyBuffer[bufferIndex]));
				fs_write(PCFile, "\n", 1);
			}
			bufferIndex++;
			bufferIndex %= MAX_DISASM_STEPS;
		}
		fs_close(PCFile);
		printf("Closed file on PC\n");
	} else {
		printf("Error: Unable to Open File on PC Host\n");
	}
}

#else
//#define PrintRegisters() (0)
#define DisassembleInstruction(...) (0)
#define DisassembleInstruction2(...) (0)
#endif

void PrintRegisters() {
	printf("PC: [$%04x] A: [$%02x] X: [$%02x] Y: [$%02x] SP: [$%04x]\n", (pPC - pPC_Offset) - 1, A, X, Y, BASE_STACK + SP);
	printf("Stack: \n");
	for (int SP_Index = SP + 1; SP_Index <= 0xFF; SP_Index++) {
		printf("$%04x: %02x\n", BASE_STACK + SP_Index, RAM[BASE_STACK + SP_Index]);
	}
}

// Unknown Instruction
void Op_XX() {
	DisassembleInstruction("HACF", NULL, 0);
	printf("WARNING: RUNNING UNDOCUMENTED INSTRUCTION - HALTING\n");
	PrintRegisters();

	unsigned char* banktest = pPC - 1;
	printf("$%04x: ", pPC - pPC_Offset - 1);
	for (int i = 0; i < 16; i++) {
		printf("$%02x ", banktest[i]);
	}
	printf("\n");

	HALT = 1;
	CLK( 2 );
}

/*===================================================================*/
/*                                                                   */
/*                K6502_Init() : Initialize K6502                    */
/*                                                                   */
/*===================================================================*/
void K6502_Init() {
	unsigned char idx;
	unsigned char idx2;

	HALT = 0;

	// The establishment of the IRQ pin
	NMI_Wiring = NMI_State = 1;
	IRQ_Wiring = IRQ_State = 1;

	// Make a table for the test
	idx = 0;
	do {
		if ( idx == 0 )
			g_byTestTable[ 0 ] = FLAG_Z;
		else if ( idx > 127 )
			g_byTestTable[ idx ] = FLAG_N;
		else
			g_byTestTable[ idx ] = 0;
		++idx;
	} while ( idx != 0 );

	// Make a table ASL
	idx = 0;
	do {
		g_ASLTable[ idx ].byValue = idx << 1;
		g_ASLTable[ idx ].byFlag = 0;

		if ( idx > 127 )
			g_ASLTable[ idx ].byFlag = FLAG_C;

		if ( g_ASLTable[ idx ].byValue == 0 )
			g_ASLTable[ idx ].byFlag |= FLAG_Z;
		else if ( g_ASLTable[ idx ].byValue & 0x80 )
			g_ASLTable[ idx ].byFlag |= FLAG_N;

		++idx;
	} while ( idx != 0 );

	// Make a table LSR
	idx = 0;
	do {
		g_LSRTable[ idx ].byValue = idx >> 1;
		g_LSRTable[ idx ].byFlag = 0;

		if ( idx & 1 )
			g_LSRTable[ idx ].byFlag = FLAG_C;

		if ( g_LSRTable[ idx ].byValue == 0 )
			g_LSRTable[ idx ].byFlag |= FLAG_Z;

		++idx;
	} while ( idx != 0 );

	// Make a table ROL
	for ( idx2 = 0; idx2 < 2; ++idx2 ) {
		idx = 0;
		do {
			g_ROLTable[ idx2 ][ idx ].byValue = ( idx << 1 ) | idx2;
			g_ROLTable[ idx2 ][ idx ].byFlag = 0;

			if ( idx > 127 )
				g_ROLTable[ idx2 ][ idx ].byFlag = FLAG_C;

			if ( g_ROLTable[ idx2 ][ idx ].byValue == 0 )
				g_ROLTable[ idx2 ][ idx ].byFlag |= FLAG_Z;
			else if ( g_ROLTable[ idx2 ][ idx ].byValue & 0x80 )
				g_ROLTable[ idx2 ][ idx ].byFlag |= FLAG_N;

			++idx;
		} while ( idx != 0 );
	}

	// Make a table ROR
	for ( idx2 = 0; idx2 < 2; ++idx2 ) {
		idx = 0;
		do {
			g_RORTable[ idx2 ][ idx ].byValue = ( idx >> 1 ) | ( idx2 << 7 );
			g_RORTable[ idx2 ][ idx ].byFlag = 0;

			if ( idx & 1 )
				g_RORTable[ idx2 ][ idx ].byFlag = FLAG_C;

			if ( g_RORTable[ idx2 ][ idx ].byValue == 0 )
				g_RORTable[ idx2 ][ idx ].byFlag |= FLAG_Z;
			else if ( g_RORTable[ idx2 ][ idx ].byValue & 0x80 )
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
void K6502_Reset() {

	HALT = 0;

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
	printf("Setting PC to Reset Vector [$%04x]\n", PC);
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
	totalClocks = 0;

	#ifdef DEBUG
	memset(DisassemblyBuffer, 0, MAX_DISASM_STEPS * MAX_DISASM_STRING);
	#endif
}

/*===================================================================*/
/*                                                                   */
/*    K6502_Set_Int_Wiring() : Set up wiring of the interrupt pin    */
/*                                                                   */
/*===================================================================*/
void K6502_Set_Int_Wiring( unsigned char byNMI_Wiring, unsigned char byIRQ_Wiring ) {
	NMI_Wiring = byNMI_Wiring;
	IRQ_Wiring = byIRQ_Wiring;
}

void K6502_DoNMI() {
	if (NMI_State != NMI_Wiring) {
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

void K6502_DoIRQ() {
	if ( IRQ_State != IRQ_Wiring ) {
		// IRQ Interrupt
		// Execute IRQ if an I flag isn't being set
		if ( !( F & FLAG_I ) ) {
			CLK( 7 );

			VIRPC;
			PUSHW( PC );
			PUSH( F & ~FLAG_B );

			RSTF( FLAG_D );
			SETF( FLAG_I );

			PC = K6502_ReadW( VECTOR_IRQ );		
			REALPC;
		}

		IRQ_State = IRQ_Wiring;	
	}
}

bool writeOnce = true;

/*===================================================================*/
/*                                                                   */
/*  K6502_Step() :                                                   */
/*          Only the specified number of the clocks execute Op.      */
/*                                                                   */
/*===================================================================*/
void K6502_Step( uint16 wClocks ) {
	startProfiling(0);
	wA0 = 0;
	byD0 = 0;
	byD1 = 0;
	wD0 = 0;

	unsigned char opcode;

	// It has a loop until a constant clock passes
	while ( g_wPassedClocks < wClocks ) {
		K6502_DoIRQ();

		// Read an instruction
		opcode = *(pPC++);

		switch (opcode) {
			case 0x00:
				DisassembleInstruction("BRK", NULL, 0);
				VIRPC; PC++; PUSHW( PC ); SETF( FLAG_B ); PUSH( F ); SETF( FLAG_I ); PC = K6502_ReadW( VECTOR_IRQ ); REALPC; CLK( 7 );
				break;

			case 0x01:
				DisassembleInstruction("ORA", "($%04x,x)", AA_ABS);
				ORA( A_IX ); ++pPC; CLK( 6 );
				break;

			case 0x05:
				DisassembleInstruction("ORA", "$%02x", *pPC);
				ORA( A_ZP ); CLK( 3 );
				break;

			case 0x06:
				DisassembleInstruction("ASL", "$%02x", *pPC);
				ASL( AA_ZP ); CLK( 5 );
				break;

			case 0x08:
				DisassembleInstruction("PHP", NULL, 0);
				PUSH( F | FLAG_B | FLAG_R ); CLK( 3 );
				break;

			case 0x09:
				DisassembleInstruction("ORA", "#$%02x", *pPC);
				ORA( A_IMM ); CLK( 2 );
				break;

			case 0x0A:
				DisassembleInstruction("ASL", NULL, 0);
				ASLA; CLK( 2 );
				break;

			case 0x0D:
				DisassembleInstruction("ORA", "$%04x", AA_ABS);
				ORA( A_ABS ); pPC += 2; CLK( 4 );
				break;

			case 0x0E:
				DisassembleInstruction("ASL", "$%04x", AA_ABS);
				ASL( AA_ABS ); pPC += 2; CLK( 6 );
				break;

			case 0x10:
				DisassembleInstruction("BPL", "$%04x", AA_ABS);
				BRA( !( F & FLAG_N ) );
				break;

			case 0x11:
				DisassembleInstruction("ORA", "($%04x),y", AA_ABS);
				ORA( A_IY ); CLK( 5 );
				break;
			
			case 0x15:
				DisassembleInstruction("ORA", "$%02x,x", *pPC);	
				ORA( A_ZPX ); CLK( 4 );
				break;

			case 0x16:
				DisassembleInstruction("ASL", "$%02x,x", *pPC);	
				ASL( AA_ZPX ); CLK( 6 );
				break;

			case 0x18:
				DisassembleInstruction("CLC", NULL, 0);
				RSTF( FLAG_C ); CLK( 2 );
				break;

			case 0x19:
				DisassembleInstruction("ORA", "$%04x,y", AA_ABS);	
				ORA( A_ABSY ); CLK( 4 );
				break;

			case 0x1D:
				DisassembleInstruction("ORA", "$%04x,x", AA_ABS);
				ORA( A_ABSX ); CLK( 4 );
				break;

			case 0x1E:
				DisassembleInstruction("ASL", "$%04x,x", AA_ABS);
				ASL( AA_ABSX ); pPC += 2; CLK( 7 );
				break;

			case 0x20:
				DisassembleInstruction("JSR", "$%04x", AA_ABS);
				JSR; CLK( 6 );
				break;

			case 0x21:
				DisassembleInstruction("AND", "($%02x,x)", *pPC);
				AND( A_IX ); ++pPC; CLK( 6 );
				break;

			case 0x24:
				DisassembleInstruction("BIT", "$%02x", *pPC);
				BIT( A_ZP ); CLK( 3 );
				break;

			case 0x25:
				DisassembleInstruction("AND", "$%02x", *pPC);
				AND( A_ZP ); CLK( 3 );
				break;

			case 0x26:
				DisassembleInstruction("ROL", "$%02x", *pPC);
				ROL( AA_ZP ); CLK( 5 );
				break;

			case 0x28:
				DisassembleInstruction("PLP", NULL, 0);
				POP( F ); SETF( FLAG_R ); CLK( 4 );
				break;

			case 0x29:
				DisassembleInstruction("AND", "#$%02x", *pPC);	
				AND( A_IMM ); CLK( 2 );
				break;

			case 0x2A:
				DisassembleInstruction("ROL", NULL, 0);	
				ROLA; CLK( 2 );
				break;

			case 0x2C:
				DisassembleInstruction("BIT", "$%04x", AA_ABS);	
				BIT( A_ABS ); pPC += 2; CLK( 4 );
				break;

			case 0x2D:
				DisassembleInstruction("AND", "$%04x", AA_ABS);
				AND( A_ABS ); pPC += 2; CLK( 4 );
				break;

			case 0x2E:
				DisassembleInstruction("ROL", "$%04x", AA_ABS);
				ROL( AA_ABS ); pPC += 2; CLK( 6 );
				break;

			case 0x30:
				DisassembleInstruction("BMI", "$%02x", *pPC);	
				BRA( F & FLAG_N );
				break;

			case 0x31:
				DisassembleInstruction("AND", "($%02x),y", *pPC);
				AND( A_IY ); CLK( 5 );
				break;

			case 0x35:
				DisassembleInstruction("AND", "$%02x,x", *pPC);	
				AND( A_ZPX ); CLK( 4 );
				break;

			case 0x36:
				DisassembleInstruction("ROL", "$%02x,x", *pPC);	
				ROL( AA_ZPX ); CLK( 6 );
				break;				

			case 0x38:
				DisassembleInstruction("SEC", NULL, 0);
				SETF( FLAG_C ); CLK( 2 );
				break;

			case 0x39:
				DisassembleInstruction("AND", "$%04x,y", AA_ABS);
				AND( A_ABSY ); CLK( 4 );
				break;

			case 0x3D:
				DisassembleInstruction("AND", "$%04x,x", AA_ABS);	
				AND( A_ABSX ); CLK( 4 );
				break;

			case 0x3E:
				DisassembleInstruction("ROL", "$%04x,x", AA_ABS);
				ROL( AA_ABSX ); pPC += 2; CLK( 7 );
				break;

			case 0x40:
				DisassembleInstruction("RTI", NULL, 0);
				POP( F ); SETF( FLAG_R ); POPW( PC ); REALPC; CLK( 6 );
				break;

			case 0x41:
				DisassembleInstruction("EOR", "($%02x,x)", *pPC);	
				EOR( A_IX ); ++pPC; CLK( 6 );
				break;

			case 0x45:
				DisassembleInstruction("EOR", "$%02x", *pPC);	
				EOR( A_ZP ); CLK( 3 );
				break;

			case 0x46:
				DisassembleInstruction("LSR", "$%02x", *pPC);
				LSR( AA_ZP ); CLK( 5 );
				break;

			case 0x48:
				DisassembleInstruction("PHA", NULL, 0);
				PUSH( A ); CLK( 3 );
				break;

			case 0x49:
				DisassembleInstruction("EOR", "#$%02x", *pPC);
				EOR( A_IMM ); CLK( 2 );
				break;				

			case 0x4A:
				DisassembleInstruction("LSR", NULL, 0);	
				LSRA; CLK( 2 );
				break;

			case 0x4C:
				DisassembleInstruction("JMP", "$%04x", AA_ABS);	
				JMP( AA_ABS ); CLK( 3 );
				break;

			case 0x4D:
				DisassembleInstruction("EOR", "$%04x", AA_ABS);	
				EOR( A_ABS ); pPC += 2; CLK( 4 );
				break;

			case 0x4E:
				DisassembleInstruction("LSR", "$%04x", AA_ABS);	
				LSR( AA_ABS ); pPC += 2; CLK( 6 );
				break;

			case 0x50:
				DisassembleInstruction("BVC", "$%02x", *pPC);
				BRA( !( F & FLAG_V ) );
				break;

			case 0x51:
				DisassembleInstruction("EOR", "($%02x),y", *pPC);
				EOR( A_IY ); CLK( 5 );
				break;				

			case 0x55:
				DisassembleInstruction("EOR", "$%02x,x", *pPC);	
				EOR( A_ZPX ); CLK( 4 );
				break;

			case 0x56:
				DisassembleInstruction("LSR", "$%02x,x", *pPC);	
				LSR( AA_ZPX ); CLK( 6 );
				break;						

			case 0x58:
				DisassembleInstruction("CLI", NULL, 0);	
				byD0 = F;
				RSTF( FLAG_I ); CLK( 2 );
				if ( ( byD0 & FLAG_I ) && IRQ_State == 0 ) {
					// IRQ Interrupt
					// Execute IRQ if an I flag isn't being set
					if ( !( F & FLAG_I ) ) {
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
				break;

			case 0x59:
				DisassembleInstruction("EOR", "$%04x,y", AA_ABS);	
				EOR( A_ABSY ); CLK( 4 );
				break;

			case 0x5D:
				DisassembleInstruction("EOR", "$%04x,x", AA_ABS);
				EOR( A_ABSX ); CLK( 4 );
				break;

			case 0x5E:
				DisassembleInstruction("LSR", "$%04x,x", AA_ABS);
				LSR( AA_ABSX ); pPC += 2; CLK( 7 );
				break;

			case 0x60:
				DisassembleInstruction("RTS", NULL, 0);	
				POPW( PC ); ++PC; REALPC; CLK( 6 );
				break;

			case 0x61:
				DisassembleInstruction("ADC", "($%02x,x)", *pPC);
				ADC( A_IX ); ++pPC; CLK( 6 );
				break;

			case 0x65:
				DisassembleInstruction("ADC", "$%02x", *pPC);	
				ADC( A_ZP ); CLK( 3 );
				break;

			case 0x66:
				DisassembleInstruction("ROR", "$%02x", *pPC);
				ROR( AA_ZP ); CLK( 5 );
				break;

			case 0x68:
				DisassembleInstruction("PLA", NULL, 0);	
				POP( A ); TEST( A ); CLK( 4 );
				break;

			case 0x69:
				DisassembleInstruction("ADC", "#$%02x", *pPC);
				ADC( A_IMM ); CLK( 2 );
				break;

			case 0x6A:
				DisassembleInstruction("ROR", NULL, 0);	
				RORA; CLK( 2 );
				break;

			case 0x6C:
				DisassembleInstruction2("JMP", "($%04x) = $%04x", AA_ABS, K6502_ReadW(AA_ABS));
				JMP( K6502_ReadW( AA_ABS ) ); CLK( 5 );
				break;
		
			case 0x6D:
				DisassembleInstruction("ADC", "$%04x", AA_ABS);
				ADC( A_ABS ); pPC += 2; CLK( 4 );
				break;
	
			case 0x6E:
				DisassembleInstruction("ROR", "$%04x", AA_ABS);	
				ROR( AA_ABS ); pPC += 2; CLK( 6 );
				break;

			case 0x70:
				DisassembleInstruction("BVS", "$%02x", *pPC);
				BRA( F & FLAG_V );
				break;

			case 0x71:
				DisassembleInstruction("ADC", "($%04x),y", AA_ABS);
				ADC( A_IY ); CLK( 5 );
				break;				

			case 0x75:
				DisassembleInstruction("ADC", "$%02x,x", *pPC);	
				ADC( A_ZPX ); CLK( 4 );
				break;

			case 0x76:
				DisassembleInstruction("ROR", "$%02x,x", *pPC);	
				ROR( AA_ZPX ); CLK( 6 );
				break;

			case 0x78:
				DisassembleInstruction("SEI", NULL, 0);	
				SETF( FLAG_I ); CLK( 2 );
				break;

			case 0x79:
				DisassembleInstruction("ADC", "$%04x,y", AA_ABS);
				ADC( A_ABSY ); CLK( 4 );
				break;				

			case 0x7D:
				DisassembleInstruction("ADC", "$%04x,x", AA_ABS);	
				ADC( A_ABSX ); CLK( 4 );
				break;				

			case 0x7E:
				DisassembleInstruction("ROR", "$%04x,x", AA_ABS);
				ROR( AA_ABSX ); pPC += 2; CLK( 7 );
				break;

			case 0x81:
				DisassembleInstruction("STA", "($%02x,x)", *pPC);	
				STA( AA_IX ); ++pPC; CLK( 6 );
				break;

			case 0x84:
				DisassembleInstruction("STY", "$%02x", *pPC);
				STY( AA_ZP ); CLK( 3 );
				break;

			case 0x85:
				DisassembleInstruction("STA", "$%02x", *pPC);
				STA( AA_ZP ); CLK( 3 );
				break;

			case 0x86:
				DisassembleInstruction("STX", "$%02x", *pPC);	
				STX( AA_ZP ); CLK( 3 );
				break;

			case 0x88:
				DisassembleInstruction("DEY", NULL, 0);
				--Y; TEST( Y ); CLK( 2 );
				break;

			case 0x89:
				DisassembleInstruction("BIT", "#$%02x", *pPC);
				BIT( AA_ZP ); CLK( 2 );
				break;

			case 0x8A:
				DisassembleInstruction("TXA", NULL, 0);
				A = X; TEST( A ); CLK( 2 );
				break;

			case 0x8C:
				DisassembleInstruction("STY", "$%04x", AA_ABS);
				STY( AA_ABS ); pPC += 2; CLK( 4 );
				break;

			case 0x8D:
				DisassembleInstruction("STA", "$%04x", AA_ABS);	
				STA( AA_ABS ); pPC += 2; CLK( 4 );
				break;

			case 0x8E:
				DisassembleInstruction("STX", "$%04x", AA_ABS);	
				STX( AA_ABS ); pPC += 2; CLK( 4 );
				break;

			case 0x90:
				DisassembleInstruction("BCC", "$%02x", *pPC);
				BRA( !( F & FLAG_C ) );
				break;

			case 0x91:
				DisassembleInstruction("STA", "($%04x),y", AA_ABS);	
				STA( AA_IY ); ++pPC; CLK( 6 );
				break;			

			case 0x94:
				DisassembleInstruction("STY", "$%02x,x", *pPC);
				STY( AA_ZPX ); CLK( 4 );
				break;

			case 0x95:
				DisassembleInstruction("STA", "$%02x,x", *pPC);	
				STA( AA_ZPX ); CLK( 4 );
				break;

			case 0x96:
				DisassembleInstruction("STX", "$%02x,x", *pPC);	
				STX( AA_ZPY ); CLK( 4 );
				break;

			case 0x98:
				DisassembleInstruction("TYA", NULL, 0);	
				A = Y; TEST( A ); CLK( 2 );
				break;

			case 0x99:
				DisassembleInstruction("STA", "$%04x,y", AA_ABS);
				STA( AA_ABSY ); pPC += 2; CLK( 5 );
				break;

			case 0x9A:
				DisassembleInstruction("TXS", NULL, 0);	
				SP = X; CLK( 2 );
				break;

			case 0x9D:
				DisassembleInstruction("STA", "$%04x,x", AA_ABS);
				STA( AA_ABSX ); pPC += 2; CLK( 5 );
				break;

			case 0xA0:
				DisassembleInstruction("LDY", "#$%02x", *pPC);	
				LDY( A_IMM ); CLK( 2 );
				break;

			case 0xA1:
				DisassembleInstruction("LDA", "($%04x,x)", AA_ABS);	
				LDA( A_IX ); ++pPC; CLK( 6 );
				break;

			case 0xA2:
				DisassembleInstruction("LDX", "#$%02x", *pPC);	
				LDX( A_IMM ); CLK( 2 );
				break;

			case 0xA4:
				DisassembleInstruction("LDY", "$%02x", *pPC);
				LDY( A_ZP ); CLK( 3 );
				break;

			case 0xA5:
				DisassembleInstruction("LDA", "$%02x", *pPC);
				LDA( A_ZP ); CLK( 3 );
				break;

			case 0xA6:
				DisassembleInstruction("LDX", "$%02x", *pPC);	
				LDX( A_ZP ); CLK( 3 );
				break;

			case 0xA8:
				DisassembleInstruction("TAY", NULL, 0);	
				Y = A; TEST( A ); CLK( 2 );
				break;

			case 0xA9:
				DisassembleInstruction("LDA", "#$%02x", *pPC);	
				LDA( A_IMM ); CLK( 2 );
				break;

			case 0xAA:
				DisassembleInstruction("TAX", NULL, 0);	
				X = A; TEST( A ); CLK( 2 );
				break;

			case 0xAC:
				DisassembleInstruction("LDY", "$%04x", AA_ABS);	
				LDY( A_ABS ); pPC += 2; CLK( 4 );
				break;

			case 0xAD:
				DisassembleInstruction("LDA", "$%04x", AA_ABS);	
				LDA( A_ABS ); pPC += 2; CLK( 4 );
				break;

			case 0xAE:
				DisassembleInstruction("LDX", "$%04x", AA_ABS);	
				LDX( A_ABS ); pPC += 2; CLK( 4 );
				break;

			case 0xB0:
				DisassembleInstruction("BCS", "$%02x", *pPC);	
				BRA( F & FLAG_C );
				break;

			case 0xB1:
				DisassembleInstruction("LDA", "($%04x),y", AA_ABS);	
				LDA( A_IY ); CLK( 5 );
				break;

			case 0xB4:
				DisassembleInstruction("LDY", "$%02x,x", *pPC);
				LDY( A_ZPX ); CLK( 4 );
				break;

			case 0xB5:
				DisassembleInstruction("LDA", "$%02x,x", *pPC);
				LDA( A_ZPX ); CLK( 4 );
				break;

			case 0xB6:
				DisassembleInstruction("LDX", "$%02x,x", *pPC);
				LDX( A_ZPY ); CLK( 4 );
				break;

			case 0xB8:
				DisassembleInstruction("CLV", NULL, 0);	
				RSTF( FLAG_V ); CLK( 2 );
				break;

			case 0xB9:
				DisassembleInstruction("LDA", "$%04x,y", AA_ABS);
				LDA( A_ABSY ); CLK( 4 );
				break;																

			case 0xBA:
				DisassembleInstruction("TSX", NULL, 0);
				X = SP; TEST( X ); CLK( 2 );
				break;

			case 0xBC:
				DisassembleInstruction("LDY", "$%04x,x", AA_ABS);
				LDY( A_ABSX ); CLK( 4 );
				break;

			case 0xBD:
				DisassembleInstruction("LDA", "$%04x,x", AA_ABS);
				LDA( A_ABSX ); CLK( 4 );
				break;

			case 0xBE:
				DisassembleInstruction("LDX", "$%04x,x", AA_ABS);
				LDX( A_ABSY ); CLK( 4 );
				break;

			case 0xC0:
				DisassembleInstruction("CPY", "#$%02x", *pPC);	
				CPY( A_IMM ); CLK( 2 );
				break;

			case 0xC1:
				DisassembleInstruction("CMP", "($%04x,x)", A_IX);	
				CMP( A_IX ); ++pPC; CLK( 6 );
				break;

			case 0xC4:
				DisassembleInstruction("CPY", "$%02x", *pPC);	
				CPY( A_ZP ); CLK( 3 );
				break;

			case 0xC5:
				DisassembleInstruction("CMP", "$%02x", *pPC);	
				CMP( A_ZP ); CLK( 3 );
				break;

			case 0xC6:
				DisassembleInstruction("DEC", "$%02x", *pPC);
				DEC( AA_ZP ); CLK( 5 );
				break;

			case 0xC8:
				DisassembleInstruction("INY", NULL, 0);	
				++Y; TEST( Y ); CLK( 2 );	
				break;

			case 0xC9:
				DisassembleInstruction("CMP", "#$%02x", *pPC);
				CMP( A_IMM ); CLK( 2 );
				break;

			case 0xCA:
				DisassembleInstruction("DEX", NULL, 0);	
				--X; TEST( X ); CLK( 2 );
				break;

			case 0xCB:
				DisassembleInstruction("AXS", NULL, 0);	
				pPC++; X = X & A; TEST( X ); CLK( 2 );
				break;				

			case 0xCC:
				DisassembleInstruction("CPY", "$%04x", AA_ABS);	
				CPY( A_ABS ); pPC += 2; CLK( 4 );
				break;

			case 0xCD:
				DisassembleInstruction("CMP", "$%04x", AA_ABS);	
				CMP( A_ABS ); pPC += 2; CLK( 4 );
				break;

			case 0xCE:
				DisassembleInstruction("DEC", "$%04x", AA_ABS);	
				DEC( AA_ABS ); pPC += 2;CLK( 6 );
				break;

			case 0xD0:
				DisassembleInstruction("BNE", "$%04x", ((pPC - pPC_Offset) + (char)(*pPC) + 1));
				BRA( !( F & FLAG_Z ) );
				break;																																					

			case 0xD1:
				DisassembleInstruction("CMP", "($%04x),y", AA_ABS);	
				CMP( A_IY ); CLK( 5 );
				break;

			case 0xD5:
				DisassembleInstruction("CMP", "$%02x,x", *pPC);	
				CMP( A_ZPX ); CLK( 4 );
				break;

			case 0xD6:
				DisassembleInstruction("DEC", "$%02x,x", *pPC);	
				DEC( AA_ZPX ); CLK( 6 );
				break;

			case 0xD8:
				DisassembleInstruction("CLD", NULL, 0);	
				RSTF( FLAG_D ); CLK( 2 );
				break;

			case 0xD9:
				DisassembleInstruction("CMP", "$%04x,y", AA_ABS);
				CMP( A_ABSY ); CLK( 4 );
				break;

			case 0xDD:
				DisassembleInstruction("CMP", "$%04x,x", AA_ABS);
				CMP( A_ABSX ); CLK( 4 );
				break;

			case 0xDE:
				DisassembleInstruction("DEC", "$%04x,x", AA_ABS);
				DEC( AA_ABSX ); pPC += 2; CLK( 7 );
				break;

			case 0xE0:
				DisassembleInstruction("CPX", "#$%02x", *pPC);
				CPX( A_IMM ); CLK( 2 );
				break;

			case 0xE1:
				DisassembleInstruction("SBC", "($%04x,x)", AA_ABS);	
				SBC( A_IX ); ++pPC; CLK( 6 );
				break;

			case 0xE4:
				DisassembleInstruction("CPX", "$%02x", *pPC);
				CPX( A_ZP ); CLK( 3 );
				break;

			case 0xE5:
				DisassembleInstruction("SBC", "$%02x", *pPC);
				SBC( A_ZP ); CLK( 3 );
				break;

			case 0xE6:
				DisassembleInstruction("INC", "$%02x", *pPC);
				INC( AA_ZP ); CLK( 5 );
				break;

			case 0xE8:
				DisassembleInstruction("INX", NULL, 0);
				++X; TEST( X ); CLK( 2 );
				break;

			case 0xE9:
				DisassembleInstruction("SBC", "#$%02x", *pPC);
				SBC( A_IMM ); CLK( 2 );
				break;

			case 0xEA:
				DisassembleInstruction("NOP", NULL, 0);	
				CLK( 2 );
				break;

			case 0xEC:
				DisassembleInstruction("CPX", "$%04x", AA_ABS);
				CPX( A_ABS ); pPC += 2; CLK( 4 );
				break;

			case 0xED:
				DisassembleInstruction("SBC", "$%04x", AA_ABS);	
				SBC( A_ABS ); pPC += 2; CLK( 4 );
				break;
				
			case 0xEE:
				DisassembleInstruction("INC", "$%04x", AA_ABS);	
				INC( AA_ABS ); pPC += 2; CLK( 6 );
				break;

			case 0xF0:
				DisassembleInstruction("BEQ", "$%04x", PC + *pPC);	
				BRA( F & FLAG_Z );
				break;

			case 0xF1:
				DisassembleInstruction("SBC", "($%04x),y", AA_ABS);	
				SBC( A_IY ); CLK( 5 );
				break;

			case 0xF5:
				DisassembleInstruction("SBC", "$%02x,x", *pPC);	
				SBC( A_ZPX ); CLK( 4 );
				break;

			case 0xF6:
				DisassembleInstruction("INC", "$%02x,x", *pPC);	
				INC( AA_ZPX ); CLK( 6 );
				break;

			case 0xF8:
				DisassembleInstruction("SED", NULL, 0);	
				SETF( FLAG_D ); CLK( 2 );
				break;

			case 0xF9:
				DisassembleInstruction("SBC", "$%04x,y", AA_ABS);
				SBC( A_ABSY ); CLK( 4 );
				break;

			case 0xFD:
				DisassembleInstruction("SBC", "$%04x,x", AA_ABS);
				SBC( A_ABSX ); CLK( 4 );
				break;

			case 0xFE:
				DisassembleInstruction("INC", "$%04x,x", AA_ABS);
				INC( AA_ABSX ); pPC += 2; CLK( 7 );
				break;

			default:
				Op_XX();
				break;
		}

		// We crossed a bank boundary, make sure we end up in the right PRG bank
		if (pPC - pPC_Offset > 0x1FFF) {
			VIRPC;
			REALPC;
		}

		if (HALT) {
			printf("Attempted to Run Illegal Opcode [%02X]\n", opcode);
			break;
		}
	}  /* end of while ... */

	// Correct the number of the clocks
	g_wPassedClocks -= wClocks;
	endProfiling(0);
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

void K6502_Burn(uint16 wClocks) {
	g_wPassedClocks += wClocks;
	totalClocks += wClocks;
}

uint32 K6502_GetCycles() {
	return totalClocks;
}
