#include <kos.h>
#include "macros.h"

#include "pNesX.h"
#include "pNesX_PPU_DC.h"
#include "pNesX_DrawLine_BG_C.h"
#include "Mapper.h"
#include "Mapper_5.h"
#include "pNesX_System_DC.h"

extern unsigned char pSprBuf[];
extern int SprBufClean;

// How many entries in SpritesToDrawNextScanline are valid
unsigned char NumSpritesToDrawNextScanline;
// The indexes of the top 8 sprites to draw on the next scanline
unsigned char SpritesToDrawNextScanline[8];
// Mark whether the sprite overflow flag should be set on next scanline
bool OverflowSpritesOnNextScanline;

// How many entries in SpritesToDraw are valid
unsigned char NumSpritesToDraw;
// The top 8 sprites to draw this scanline
unsigned char SpritesToDraw[8];
// Whether we overflowed sprites this scanline
bool OverflowedSprites;

void pNesX_DrawLine_Spr_C(unsigned char* scanline_buffer) {
	int nX;
	int nY;
	int nYBit;
	unsigned char *pSPRRAM;
	int nAttr;
	uint16 bySprCol;
	unsigned char patternData[8];
	unsigned char* pbyBGData;
	unsigned char* pPalTbl;
	unsigned char byData1;
	unsigned char byData2;

	if (MapperNo == 5) {
		Mapper_5_PPU_Latch_RenderScreen(0, 0);
	}

	// Sprite Evaluation Logic
	// Reset buffers and counts on scanline 0
	if (ppuinfo.PPU_Scanline == 0) {
		NumSpritesToDrawNextScanline = 0;
		memset(SpritesToDrawNextScanline, 0, 8);
		NumSpritesToDraw = 0;
		memset(SpritesToDraw, 0, 8);
		OverflowSpritesOnNextScanline = false;
		OverflowedSprites = false;
	} else {
	// Otherwise move the SpritesToDrawNextScanline to this scanline
		NumSpritesToDraw = NumSpritesToDrawNextScanline;
		memcpy(SpritesToDraw, SpritesToDrawNextScanline, 8);
		NumSpritesToDrawNextScanline = 0;

		// More than 8 sprites were requested to be drawn on this scanline, so set the PPU flag for overflow
		if (OverflowSpritesOnNextScanline) {
			PPU_R2 |= R2_MAX_SP;
			OverflowedSprites = true;
			OverflowSpritesOnNextScanline = false;
		} else {
		// Or if we are not going to overflow this scanline, set the PPU flag back
			PPU_R2 &= ~R2_MAX_SP;			
			OverflowedSprites = false;
		}
	}

	// Calculate the sprites to draw on the next scanline
	for ( unsigned char spriteIndex = 0; spriteIndex < 64; spriteIndex++) {
		pSPRRAM = SPRRAM + (spriteIndex << 2);
		nY = pSPRRAM[ SPR_Y ];

		if ((nY > ppuinfo.PPU_Scanline) || ((nY + ppuinfo.PPU_SP_Height) <= ppuinfo.PPU_Scanline))
			continue;  // This sprite is not active on the next scanline, skip to next one

		if (NumSpritesToDrawNextScanline < 8) {
			SpritesToDrawNextScanline[NumSpritesToDrawNextScanline] = spriteIndex;
			NumSpritesToDrawNextScanline++;
		} else {
			OverflowSpritesOnNextScanline = true;
			break;
		}
	}

	// If sprite Rendering is Enabled and we're not on scanline 0 and we have sprites to draw
	if ((ppuinfo.PPU_Scanline > 0) && 
		(NumSpritesToDraw > 0) && 
		(PPU_R1 & R1_SHOW_SP)) {

		for (int spriteIndex = (NumSpritesToDraw - 1); spriteIndex >= 0; spriteIndex--) {
			//Calculate sprite address by taking index and multiplying by 4, since each entry is 4 bytes
			pSPRRAM = SPRRAM + (SpritesToDraw[spriteIndex] << 2);

			nX = pSPRRAM[ SPR_X ];
			nY = pSPRRAM[ SPR_Y ] + 1;
			nAttr = pSPRRAM[ SPR_ATTR ];
			nYBit = ppuinfo.PPU_Scanline - nY;
			nYBit = ( nAttr & SPR_ATTR_V_FLIP ) ? ( ppuinfo.PPU_SP_Height - nYBit - 1) : nYBit;

			unsigned char nameTableValue = (ppuinfo.PPU_R0 & R0_SP_SIZE) ? 
				(pSPRRAM[SPR_CHR] & 0xfe) :
				(pSPRRAM[SPR_CHR]);
			unsigned char characterBank = ((ppuinfo.PPU_R0 & R0_SP_SIZE) ? 
				((pSPRRAM[SPR_CHR] & 1) ? 4 : 0) + (nameTableValue >> 6) :
				((ppuinfo.PPU_R0 & R0_SP_ADDR) ? 4 : 0) + (nameTableValue >> 6));
			unsigned char characterIndex = ((ppuinfo.PPU_R0 & R0_SP_SIZE) && (nYBit >= 8)) ?
				(nameTableValue & 0x3F) + 1 :
				(nameTableValue & 0x3F);

			pbyBGData = PPUBANK[characterBank] + (characterIndex << 4) + (nYBit % 8);
			byData1 = ( ( pbyBGData[ 0 ] >> 1 ) & 0x55 ) | ( pbyBGData[ 8 ] & 0xAA );
			byData2 = ( pbyBGData[ 0 ] & 0x55 ) | ( ( pbyBGData[ 8 ] << 1 ) & 0xAA );
			pPalTbl = &PPURAM[0x3F10 + ((nAttr & SPR_ATTR_COLOR) << 2)];

			unsigned char offset = 0;
			unsigned char pixelsToDraw = 8;
			// If we aren't rendering the left 8 pixels of sprites skip past their offset
			// before merging the sprite pixels into the render buffer
			if ((nX < 8) && (!(PPU_R1 & R1_CLIP_SP))) {
				offset = 8 - nX;
			}
			// If the sprite would go off the right side of the buffer, reduce the number of
			// pixels to draw so that we don't overflow
			if (nX > 248) {
				pixelsToDraw = 256 - nX;
			}

			if ( nAttr & SPR_ATTR_H_FLIP ) {
				patternData[ 0 ] = byData2 & 3;
				patternData[ 1 ] = byData1 & 3;
				patternData[ 2 ] = ( byData2 >> 2 ) & 3;
				patternData[ 3 ] = ( byData1 >> 2 ) & 3;
				patternData[ 4 ] = ( byData2 >> 4 ) & 3;
				patternData[ 5 ] = ( byData1 >> 4 ) & 3;
				patternData[ 6 ] = ( byData2 >> 6 ) & 3;
				patternData[ 7 ] = ( byData1 >> 6 ) & 3;
			} else {
				patternData[ 0 ] = ( byData1 >> 6 ) & 3;
				patternData[ 1 ] = ( byData2 >> 6 ) & 3;
				patternData[ 2 ] = ( byData1 >> 4 ) & 3;
				patternData[ 3 ] = ( byData2 >> 4 ) & 3;
				patternData[ 4 ] = ( byData1 >> 2 ) & 3;
				patternData[ 5 ] = ( byData2 >> 2 ) & 3;
				patternData[ 6 ] = byData1 & 3;
				patternData[ 7 ] = byData2 & 3;
			}

			// If we haven't hit sprite 0 yet - check for it on this sprite draw
			if (SpriteJustHit == SPRITE_HIT_SENTINEL) {
				for (offset; offset < pixelsToDraw; offset++) {
					unsigned char drawOffset = nX + offset;
					// if we are in column less than 255, the sprite pixel is not transparent and the background pixel is not transparent, record a hit
					if ((drawOffset < 255) && (patternData[offset]) && ((scanline_buffer[drawOffset] & 0x40) == 0)) {
						SpriteJustHit = ppuinfo.PPU_Scanline;
					}

					// if the sprite pixel is not transparent, and priority over background is set for this sprite or the background pixel is not transparent, draw the pixel
					if (patternData[offset] && (((nAttr & SPR_ATTR_PRI) == 0) || (scanline_buffer[drawOffset] & 0x40))) {
						// keep the background transparent pixel bit set, so that no matter whether this pixel is overwritten with a sprite
						// we remember that it was once a non-transparent pixel for the purposes of sprite 0 hit
						scanline_buffer[drawOffset] = (scanline_buffer[drawOffset] & 0x40) | pPalTbl[patternData[ offset ]];
					}
				}
			// Otherwise we've hit sprite 0 so we don't need to check for sprite 0 collisions
			} else {
				for (offset; offset < pixelsToDraw; offset++) {
					unsigned char drawOffset = nX + offset;

					// if the sprite pixel is not transparent, and priority over background is set for this sprite or the background pixel is not transparent, draw the pixel
					if (patternData[ offset ] && (((nAttr & SPR_ATTR_PRI) == 0) || (scanline_buffer[drawOffset] & 0x40))) {
						// keep the background transparent pixel bit set, so that no matter whether this pixel is overwritten with a sprite
						// we remember that it was once a non-transparent pixel for the purposes of sprite 0 hit						
						scanline_buffer[drawOffset] = (scanline_buffer[drawOffset] & 0x40) | pPalTbl[patternData[ offset ]];
					}
				}			
			}
		}	
	}
}