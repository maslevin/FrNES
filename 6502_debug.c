#include "6502.h"

#include <kos.h>
#include <stdlib.h>

void print_6502_registers() {
    uint16* PC = (uint16*)(void*)(0x7c000b20);
    uint8* SP = (uint8*)(void*)(0x7c000b22);
    uint8* S = (uint8*)(void*)(0x7c000b23);
    uint8* A = (uint8*)(void*)(0x7c000b24);
    uint8* X = (uint8*)(void*)(0x7c000b25);
    uint8* Y = (uint8*)(void*)(0x7c000b26);

    printf("6502_Registers: PC:[%04X] SP:[%02X] A:[%02X] X:[%02X] Y:[%02X] S:[%02X]\n", *PC, *SP, *A, *X, *Y, *S);
}