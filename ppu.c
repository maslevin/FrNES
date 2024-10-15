#include "ppu.h"

#include <kos.h>

#include "pNesX.h"
#include "cpu.h"
#include "Mapper.h"
#include "cartridge.h"

uint32 mirroring;       // Mirroring mode.

//#define CIRAM_SIZE 0x800
#define CIRAM_SIZE 0x1000 // Be dumb and just make ciram 2x as big, so that 4 screen mirroring will work out of the box
#define CGRAM_SIZE 0x20
#define OAMRAM_SIZE 0x100

uint8 ciRam[CIRAM_SIZE];           // VRAM for nametables.
uint8 cgRam[CGRAM_SIZE];            // VRAM for palettes.
uint8 oamMem[OAMRAM_SIZE];          // VRAM for sprite properties.
Sprite_t oam[8], secOam[8];  // Sprite buffers.

LOOPY_t vAddr, tAddr;  // Loopy V, T.
uint8 fX;              // Fine X.
uint8 oamAddr;         // OAM address.

PPUCTRL_t ctrl;      // PPUCTRL   ($2000) register.
PPUMASK_t mask;      // PPUMASK   ($2001) register.
PPUSTATUS_t status;  // PPUSTATUS ($2002) register.

// Background latches:
uint8 nt, at, bgL, bgH;
// Background shift registers:
uint8 atShiftL, atShiftH; 
uint16 bgShiftL, bgShiftH;
bool atLatchL, atLatchH;

// Rendering counters:
int scanline, dot;
bool frameOdd;

//inline bool rendering() { return mask.bg || mask.spr; }
inline bool rendering() { return true; }
inline int spr_height() { return ctrl.sprSz ? 16 : 8; }

#define __ALIGN32__		__attribute__ ((aligned (32)))
__ALIGN32__ unsigned char scanlineBuffer[256];
void* textureAddress;

/* Get CIRAM address according to mirroring */
uint16 nt_mirror(uint16 addr) {
    switch (mirroring) {
        case PPU_MIRRORING_VERTICAL:        return addr & 0x07FF;
        case PPU_MIRRORING_HORIZONTAL:      return ((addr >> 1) & 0x400) + (addr % 0x03FF);
        case PPU_MIRRORING_ONE_SCREEN_LOW:  return addr & 0x03FF;
        case PPU_MIRRORING_ONE_SCREEN_HIGH: return 0x0400 + (addr & 0x03FF);
        case PPU_MIRRORING_FOUR_SCREEN:     return addr & 0x0FFF;
        default:                            return addr - 0x2000;
    }
}
void ppu_set_mirroring(uint32 mode) { mirroring = mode; }

/* Access PPU memory */
inline uint8 ppu_memory_read(uint16 addr) {
    switch (addr) {
        case 0x0000 ... 0x1FFF:  return cartridge_ppu_read(addr);        // CHR-ROM/RAM.
        case 0x2000 ... 0x3EFF:  return ciRam[nt_mirror(addr)];          // Nametables.
        case 0x3F00 ... 0x3FFF:  // Palettes:
            if ((addr & 0x13) == 0x10) addr &= ~0x10;
            return cgRam[addr & 0x1F] & (mask.gray ? 0x30 : 0xFF);
        default: return 0;
    }
}
inline void ppu_memory_write(uint16 addr, uint8 value) {
//    printf("PPU WRITE [$%04X]: [$%02X]\n", addr, value);
    switch (addr) {
        case 0x0000 ... 0x1FFF:  cartridge_ppu_write(addr, value); break;  // CHR-ROM/RAM.
        case 0x2000 ... 0x3EFF:  {
            ciRam[nt_mirror(addr)] = value; 
        } break;    // Nametables.
        case 0x3F00 ... 0x3FFF:  // Palettes:
            if ((addr & 0x13) == 0x10) addr &= ~0x10;
            cgRam[addr & 0x1F] = value; break;
    }
}

uint8 ppu_result;
uint8 ppu_buffer;
uint32 ppu_latch;

/* Access PPU through registers. */
uint8 ppu_read(uint32 index) {
    switch (index) {
        // PPUSTATUS ($2002):
        case 2:  ppu_result = (ppu_result & 0x1F) | status.r; status.vBlank = 0; ppu_latch = 0; break;
        case 4:  ppu_result = oamMem[oamAddr]; break;               // OAMDATA ($2004).
        case 7:                                                     // PPUDATA ($2007).
            if (vAddr.addr <= 0x3EFF)
            {
                ppu_result = ppu_buffer;
                ppu_buffer = ppu_memory_read(vAddr.addr);
            }
            else
                ppu_result = ppu_buffer = ppu_memory_read(vAddr.addr);
            vAddr.addr += ctrl.incr ? 32 : 1;
    }
    return ppu_result;
}

uint8 ppu_write(uint32 index, uint8 value) {
    ppu_result = value;

    switch (index) {
        case 0:  ctrl.r = value; tAddr.nt = ctrl.nt; break;         // PPUCTRL   ($2000).
        case 1:  mask.r = value; break;                             // PPUMASK   ($2001).
        case 3:  oamAddr = value; break;                            // OAMADDR   ($2003).
        case 4:  oamMem[oamAddr++] = value; break;                  // OAMDATA   ($2004).
        case 5:                                                     // PPUSCROLL ($2005).
            if (!ppu_latch) { fX = value & 7; tAddr.cX = value >> 3; }  // First write.
            else  { tAddr.fY = value & 7; tAddr.cY = value >> 3; }  // Second write.
            ppu_latch = !ppu_latch; break;
        case 6:                                                     // PPUADDR   ($2006).
            if (!ppu_latch) { tAddr.h = value & 0x3F; }                 // First write.
            else        { tAddr.l = value; vAddr.r = tAddr.r; }     // Second write.
            ppu_latch = !ppu_latch; break;
        case 7:  ppu_memory_write(vAddr.addr, value); vAddr.addr += ctrl.incr ? 32 : 1;  // PPUDATA ($2007).
    }
    return ppu_result;
}

/* Calculate graphics addresses */
inline uint16 nt_addr() { return 0x2000 | (vAddr.r & 0xFFF); }
inline uint16 at_addr() { return 0x23C0 | (vAddr.nt << 10) | ((vAddr.cY / 4) << 3) | (vAddr.cX / 4); }
inline uint16 bg_addr() { return (ctrl.bgTbl * 0x1000) + (nt * 16) + vAddr.fY; }
/* Increment the scroll by one pixel */
inline void h_scroll() { if (!rendering()) return; if (vAddr.cX == 31) vAddr.r ^= 0x41F; else vAddr.cX++; }
inline void v_scroll() {
    if (!rendering()) return;
    if (vAddr.fY < 7) vAddr.fY++;
    else
    {
        vAddr.fY = 0;
        if      (vAddr.cY == 31)   vAddr.cY = 0;
        else if (vAddr.cY == 29) { vAddr.cY = 0; vAddr.nt ^= 0b10; }
        else                       vAddr.cY++;
    }
}
/* Copy scrolling data from loopy T to loopy V */
inline void h_update() { if (!rendering()) return; vAddr.r = (vAddr.r & ~0x041F) | (tAddr.r & 0x041F); }
inline void v_update() { if (!rendering()) return; vAddr.r = (vAddr.r & ~0x7BE0) | (tAddr.r & 0x7BE0); }
/* Put new data into the shift registers */
inline void reload_shift() {
    bgShiftL = (bgShiftL & 0xFF00) | bgL;
    bgShiftH = (bgShiftH & 0xFF00) | bgH;

    atLatchL = (at & 1);
    atLatchH = (at & 2);
}

/* Clear secondary OAM */
void clear_oam() {
    for (int i = 0; i < 8; i++) {
        secOam[i].id    = 64;
        secOam[i].y     = 0xFF;
        secOam[i].tile  = 0xFF;
        secOam[i].attr  = 0xFF;
        secOam[i].x     = 0xFF;
        secOam[i].dataL = 0;
        secOam[i].dataH = 0;
    }
}

/* Fill secondary OAM with the sprite infos for the next scanline */
void eval_sprites() {
    int n = 0;
    for (int i = 0; i < 64; i++) {
        int line = (scanline == 261 ? -1 : scanline) - oamMem[i*4 + 0];
        // If the sprite is in the scanline, copy its properties into secondary OAM:
        if ((line >= 0) && (line < spr_height())) {
            secOam[n].id   = i;
            secOam[n].y    = oamMem[i*4 + 0];
            secOam[n].tile = oamMem[i*4 + 1];
            secOam[n].attr = oamMem[i*4 + 2];
            secOam[n].x    = oamMem[i*4 + 3];

            if (++n >= 8) {
                status.sprOvf = true;
                break;
            }
        }
    }
}

/* Load the sprite info into primary OAM and fetch their tile data. */
void load_sprites() {
    uint16 addr;
    for (int i = 0; i < 8; i++) {
        oam[i] = secOam[i];  // Copy secondary OAM into primary.

        // Different address modes depending on the sprite height:
        if (spr_height() == 16)
            addr = ((oam[i].tile & 1) * 0x1000) + ((oam[i].tile & ~1) * 16);
        else
            addr = ( ctrl.sprTbl      * 0x1000) + ( oam[i].tile       * 16);

        unsigned sprY = (scanline - oam[i].y) % spr_height();  // Line inside the sprite.
        if (oam[i].attr & 0x80) sprY ^= spr_height() - 1;      // Vertical flip.
        addr += sprY + (sprY & 8);  // Select the second tile if on 8x16.

        oam[i].dataL = ppu_memory_read(addr + 0);
        oam[i].dataH = ppu_memory_read(addr + 8);
    }
}

/* Process a pixel, draw it if it's on screen */
void pixel() {
    uint8 palette = 0, objPalette = 0;
    bool objPriority = 0;
    int x = dot - 2;

    if ((scanline < 240) && (x >= 0) && (x < 256)) {
        if (mask.bg && !(!mask.bgLeft && (x < 8))) {
            // Background:
            palette = (NTH_BIT(bgShiftH, 15 - fX) << 1) |
                       NTH_BIT(bgShiftL, 15 - fX);
            if (palette)
                palette |= ((NTH_BIT(atShiftH,  7 - fX) << 1) |
                             NTH_BIT(atShiftL,  7 - fX))      << 2;
        }
        // Sprites:
        if (mask.spr && !(!mask.sprLeft && (x < 8)))
            for (int i = 7; i >= 0; i--) {
                if (oam[i].id == 64) continue;  // Void entry.
                unsigned sprX = x - oam[i].x;
                if (sprX >= 8) continue;            // Not in range.
                if (oam[i].attr & 0x40) sprX ^= 7;  // Horizontal flip.

                uint8 sprPalette = (NTH_BIT(oam[i].dataH, 7 - sprX) << 1) |
                                 NTH_BIT(oam[i].dataL, 7 - sprX);
                if (sprPalette == 0) continue;  // Transparent pixel.

                if (oam[i].id == 0 && palette && x != 255) status.sprHit = true;
                sprPalette |= (oam[i].attr & 3) << 2;
                objPalette  = sprPalette + 16;
                objPriority = oam[i].attr & 0x20;
            }
        // Evaluate priority:
        if (objPalette && (palette == 0 || objPriority == 0)) palette = objPalette;

        scanlineBuffer[x] = ppu_memory_read(0x3F00 + (rendering() ? palette : 0));
    }
    // Perform background shifts:
    bgShiftL <<= 1; bgShiftH <<= 1;
    atShiftL = (atShiftL << 1) | atLatchL;
    atShiftH = (atShiftH << 1) | atLatchH;
}

/* Execute a cycle of a scanline */
void scanline_cycle(uint32 s) {
    static uint16 addr;

    if ((s == SCANLINE_NMI) && (dot == 1)) { status.vBlank = true; if (ctrl.nmi) set_nmi(true); }
    else if ((s == SCANLINE_POST) && (dot == 0)) { 
        new_frame(); 
    }
    else if ((s == SCANLINE_VISIBLE) || (s == SCANLINE_PRE)) {
        // Sprites:
        switch (dot) {
            case   1: clear_oam(); if (s == SCANLINE_PRE) { status.sprOvf = status.sprHit = false; } break;
            case 257: eval_sprites(); break;
            case 321: load_sprites(); break;
        }
        // Background:
        switch (dot) {
            case 2 ... 255: case 322 ... 337:
                pixel();
                switch (dot % 8) {
                    // Nametable:
                    case 1:  addr  = nt_addr(); reload_shift(); break;
                    case 2:  nt    = ppu_memory_read(addr);  break;
                    // Attribute:
                    case 3:  addr  = at_addr(); break;
                    case 4:  at    = ppu_memory_read(addr);
                        if (vAddr.cY & 2) {
                            at >>= 4;
                        }
                        if (vAddr.cX & 2) {
                            at >>= 2;
                        }
                        break;
                    // Background (low bits):
                    case 5:  addr  = bg_addr(); break;
                    case 6:  bgL   = ppu_memory_read(addr);  break;
                    // Background (high bits):
                    case 7:  addr += 8;         break;
                    case 0:  bgH   = ppu_memory_read(addr); h_scroll(); break;
                } break;
            case         256:  pixel(); bgH = ppu_memory_read(addr); v_scroll(); break;  // Vertical bump.
            case         257:  pixel(); reload_shift(); h_update(); break;  // Update horizontal position.
            case 280 ... 304:  if (s == SCANLINE_PRE)            v_update(); break;  // Update vertical position.

            // No shift reloading:
            case             1:  addr = nt_addr(); if (s == SCANLINE_PRE) status.vBlank = false; break;
            case 321: case 339:  addr = nt_addr(); break;
            // Nametable fetch instead of attribute:
            case           338:  nt = ppu_memory_read(addr); break;
            case           340:  nt = ppu_memory_read(addr); if (s == SCANLINE_PRE && rendering() && frameOdd) dot++;
        }
        // Signal scanline to mapper:
        if (dot == 260) {
            if (rendering()) {
                mapper -> hsync();
            }
            if (s == SCANLINE_VISIBLE) {
                pvr_txr_load(scanlineBuffer, &(WorkFrame -> texture[scanline * 256]), 256);
            }
        }
    }
}

/* Execute a PPU cycle. */
void ppu_step() {
    switch (scanline) {
        case 0 ... 239:  scanline_cycle(SCANLINE_VISIBLE); break;
        case       240:  scanline_cycle(SCANLINE_POST);    break;
        case       241:  scanline_cycle(SCANLINE_NMI);     break;
        case       261:  scanline_cycle(SCANLINE_PRE);     break;
    }
    // Update dot and scanline counters:
    if (++dot > 340) {
        dot %= 341;
        if (++scanline > 261) {
            scanline = 0;
            frameOdd ^= 1;
        }
    }
}

void ppu_reset() {
    frameOdd = false;
    scanline = dot = 0;
    ctrl.r = mask.r = status.r = 0;
    ppu_result = 0;
    ppu_buffer = 0;
    ppu_latch = 0;

    memset(scanlineBuffer, 0x00, 256);
    memset(ciRam,  0xFF, sizeof(ciRam));
    memset(oamMem, 0x00, sizeof(oamMem));
}