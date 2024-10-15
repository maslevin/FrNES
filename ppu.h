#ifndef __PPU_H
#define __PPU_H

#include <kos.h>

#define NTH_BIT(x, n) (((x) >> (n)) & 1)

#define SCANLINE_VISIBLE 0
#define SCANLINE_POST    1
#define SCANLINE_NMI     2
#define SCANLINE_PRE     3

#define PPU_MIRRORING_VERTICAL     0
#define PPU_MIRRORING_HORIZONTAL   1

/* Sprite buffer */
typedef struct __attribute__ ((packed, aligned(4))) Sprite_s {
    uint8 id;     // Index in OAM.
    uint8 x;      // X position.
    uint8 y;      // Y position.
    uint8 tile;   // Tile index.
    uint8 attr;   // Attributes.
    uint8 dataL;  // Tile data (low).
    uint8 dataH;  // Tile data (high).
} Sprite_t;

/* PPUCTRL ($2000) register */
typedef union PPUCTRL_u {
    struct {
        unsigned nt     : 2;  // Nametable ($2000 / $2400 / $2800 / $2C00).
        unsigned incr   : 1;  // Address increment (1 / 32).
        unsigned sprTbl : 1;  // Sprite pattern table ($0000 / $1000).
        unsigned bgTbl  : 1;  // BG pattern table ($0000 / $1000).
        unsigned sprSz  : 1;  // Sprite size (8x8 / 8x16).
        unsigned slave  : 1;  // PPU master/slave.
        unsigned nmi    : 1;  // Enable NMI.
    };
    uint8 r;
} PPUCTRL_t;

/* PPUMASK ($2001) register */
typedef union PPUMASK_u {
    struct {
        unsigned gray    : 1;  // Grayscale.
        unsigned bgLeft  : 1;  // Show background in leftmost 8 pixels.
        unsigned sprLeft : 1;  // Show sprite in leftmost 8 pixels.
        unsigned bg      : 1;  // Show background.
        unsigned spr     : 1;  // Show sprites.
        unsigned red     : 1;  // Intensify reds.
        unsigned green   : 1;  // Intensify greens.
        unsigned blue    : 1;  // Intensify blues.
    };
    uint8 r;
} PPUMASK_t;

/* PPUSTATUS ($2002) register */
typedef union PPUSTATUS_u {
    struct {
        unsigned bus    : 5;  // Not significant.
        unsigned sprOvf : 1;  // Sprite overflow.
        unsigned sprHit : 1;  // Sprite 0 Hit.
        unsigned vBlank : 1;  // In VBlank?
    };
    uint8 r;
} PPUSTATUS_t;

/* Loopy's VRAM address */
typedef union LOOPY_u {
    struct {
        unsigned cX : 5;  // Coarse X.
        unsigned cY : 5;  // Coarse Y.
        unsigned nt : 2;  // Nametable.
        unsigned fY : 3;  // Fine Y.
    };
    struct {
        unsigned l : 8;
        unsigned h : 7;
    };
    unsigned addr : 14;
    unsigned r : 15;
} LOOPY_t;

uint8 ppu_read(uint32 index);
void ppu_write(uint32 index, uint8 value);
void ppu_set_mirroring(uint32 mode);
void ppu_step();
void ppu_reset();

#endif