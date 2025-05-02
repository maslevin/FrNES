#ifndef ASM_6502
#define ASM_6502

#include <kos.h>

/* 6502 Debug Functions */
void print_6502_registers();

/* 6502 Emulator Functions */
uint8 read_byte_6502(uint16 wAddr);

void reset_6502();

void emulate_6502(uint32 cycles);

#endif