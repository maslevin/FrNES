#include <kos.h>

uint8 RAM[0x800];

uint8 

uint32 A, X, Y, SP, PC;
bool C, Z, I, D, V, N;

inline uint8 read_access(uint16 addr) {
    uint8* r;
    switch (addr) {
        case 0x0000 ... 0x1FFF:  r = &RAM[addr % 0x800]; return *r;                  // RAM.
        case 0x2000 ... 0x3FFF:  return PPU::access<wr>(addr % 8, v);                // PPU.

        // APU:
        case 0x4000 ... 0x4013:
        case            0x4015:          return APU::access<wr>(elapsed(), addr, v);
        case            0x4017:  if (wr) return APU::access<wr>(elapsed(), addr, v);
                                 else return Joypad::read_state(1);                  // Joypad 1.

        case            0x4014:  if (wr) dma_oam(v); break;                          // OAM DMA.
        case            0x4016:  if (wr) { Joypad::write_strobe(v & 1); break; }     // Joypad strobe.
                                 else return Joypad::read_state(0);                  // Joypad 0.
        case 0x4018 ... 0xFFFF:  return Cartridge::access<wr>(addr, v);              // Cartridge.
    }
    return 0;
} 
    