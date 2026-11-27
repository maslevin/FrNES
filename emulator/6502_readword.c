#include <kos.h>

#include "cpu.h"
#include "memory.h"

// (the C core's: in the "c" slot, tools/hot_slots.py; the assembly core's builds inline or drop it)
#ifdef CPU_C_ONLY
FRNES_HOT(c_read_word)
#endif
uint16 read_word_6502_c(uint16 address) {
    uint16 nextAddress = address + 1;
    if ((address & 0xff) == 0xff) {
        nextAddress = address & 0xff00;
    }
    return read_byte(address) | (read_byte(nextAddress) << 8);
}