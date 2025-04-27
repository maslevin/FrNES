#ifndef ASM_6502
#define ASM_6502

#include <kos.h>

void print_6502_registers();
void reset_6502();
void emulate_6502(uint32 cycles);

#endif