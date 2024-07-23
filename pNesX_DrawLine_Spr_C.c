#include <kos.h>
#include "macros.h"

#include "pNesX.h"
#include "pNesX_PPU_DC.h"
#include "pNesX_DrawLine_BG_C.h"
#include "pNesX_mapper.h"
#include "pNesX_System_DC.h"
#include "pNesX_mapper.h"

extern PPU_Info ppuinfo;
extern unsigned char pSprBuf[];
extern int SpriteJustHit;
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

uint16 pNesX_DrawLine_Spr_C(uint16* pSprBuf) {
	int nX;
	int nY;
	int nYBit;
	unsigned char *pSPRRAM;
	int nAttr;
	uint16 bySprCol;
	unsigned char* pbyChrData;
	unsigned char* pbyBGData;
	unsigned char patternData[8];
	unsigned char byData1;
	unsigned char byData2;

	// Sprite Evaluation Logic
	// Reset buffers and counts on scanline 0
	if (ppuinfo.PPU_Scanline == 0) {
		NumSpritesToDrawNextScanline = 0;
		memset(SpritesToDrawNextScanline, 0, 8);
		NumSpritesToDraw = 0;
		memset(SpritesToDraw, 0, 8);
		OverflowSpritesOnNextScanline = false;
		OverflowedSprites = false;

		memset4(pSprBuf, 0, 272 * 2);		
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
//	uint32 NextPPUScanline = ppuinfo.PPU_Scanline + 1;
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

		// Clear the sprite render buffer
		memset4(pSprBuf, 0, 272 * 2);

//		printf("Drawing %u Sprites\n", NumSpritesToDraw);
		// Draw from lowest priority sprite to highest priority sprite
		for (int spriteIndex = (NumSpritesToDraw - 1); spriteIndex >= 0; spriteIndex--) {
//			printf("Drawing Sprite at Buffer index [%i]\n", spriteIndex);
			//Calculate sprite address by taking index and multiplying by 4, since each entry is 4 bytes
			pSPRRAM = SPRRAM + (SpritesToDraw[spriteIndex] << 2);

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
			patternData[ 0 ] = ( byData1 >> 6 ) & 3;
			patternData[ 1 ] = ( byData2 >> 6 ) & 3;
			patternData[ 2 ] = ( byData1 >> 4 ) & 3;
			patternData[ 3 ] = ( byData2 >> 4 ) & 3;
			patternData[ 4 ] = ( byData1 >> 2 ) & 3;
			patternData[ 5 ] = ( byData2 >> 2 ) & 3;
			patternData[ 6 ] = byData1 & 3;
			patternData[ 7 ] = byData2 & 3;
			pbyChrData = patternData;

			// we invert the priority attribute here for some reason?
			nAttr ^= SPR_ATTR_PRI;
			bySprCol = ( nAttr & ( SPR_ATTR_COLOR | SPR_ATTR_PRI ) ) << 2;
			nX = pSPRRAM[ SPR_X ];
			uint16 isSpriteZeroPixel = ((pSPRRAM == SPRRAM) ? 0x0100 : 0x0000);

			if ( nAttr & SPR_ATTR_H_FLIP ) {
				// Horizontal flip
				if ( pbyChrData[ 7 ] ) {
					pSprBuf[ nX + 0 ] = isSpriteZeroPixel | bySprCol | pbyChrData[ 7 ];
				}
				if ( pbyChrData[ 6 ] ) {
					pSprBuf[ nX + 1 ] = isSpriteZeroPixel | bySprCol | pbyChrData[ 6 ];
				}
				if ( pbyChrData[ 5 ] ) {
					pSprBuf[ nX + 2 ] = isSpriteZeroPixel | bySprCol | pbyChrData[ 5 ];
				}
				if ( pbyChrData[ 4 ] ) {
					pSprBuf[ nX + 3 ] = isSpriteZeroPixel | bySprCol | pbyChrData[ 4 ];
				}
				if ( pbyChrData[ 3 ] ) {
					pSprBuf[ nX + 4 ] = isSpriteZeroPixel | bySprCol | pbyChrData[ 3 ];
				}
				if ( pbyChrData[ 2 ] ) {
					pSprBuf[ nX + 5 ] = isSpriteZeroPixel | bySprCol | pbyChrData[ 2 ];
				}
				if ( pbyChrData[ 1 ] ) {
					pSprBuf[ nX + 6 ] = isSpriteZeroPixel | bySprCol | pbyChrData[ 1 ];
				}
				if ( pbyChrData[ 0 ] ) {
					pSprBuf[ nX + 7 ] = isSpriteZeroPixel | bySprCol | pbyChrData[ 0 ];
				}
			} else {
				// Non flip
				if ( pbyChrData[ 0 ] ) {
					pSprBuf[ nX + 0 ] = isSpriteZeroPixel | bySprCol | pbyChrData[ 0 ];
				}
				if ( pbyChrData[ 1 ] ) {
					pSprBuf[ nX + 1 ] = isSpriteZeroPixel | bySprCol | pbyChrData[ 1 ];
				}
				if ( pbyChrData[ 2 ] ) {
					pSprBuf[ nX + 2 ] = isSpriteZeroPixel | bySprCol | pbyChrData[ 2 ];
				}
				if ( pbyChrData[ 3 ] ) {
					pSprBuf[ nX + 3 ] = isSpriteZeroPixel | bySprCol | pbyChrData[ 3 ];
				}
				if ( pbyChrData[ 4 ] ) {
					pSprBuf[ nX + 4 ] = isSpriteZeroPixel | bySprCol | pbyChrData[ 4 ];
				}
				if ( pbyChrData[ 5 ] ) {
					pSprBuf[ nX + 5 ] = isSpriteZeroPixel | bySprCol | pbyChrData[ 5 ];
				}
				if ( pbyChrData[ 6 ] ) {
					pSprBuf[ nX + 6 ] = isSpriteZeroPixel | bySprCol | pbyChrData[ 6 ];
				}
				if ( pbyChrData[ 7 ] ) {
					pSprBuf[ nX + 7 ] = isSpriteZeroPixel | bySprCol | pbyChrData[ 7 ];
				}
			}
		}

		// If the left clip flag for sprites is set, clear out the leftmost 8 pixels of the sprite buffer
		if (!(PPU_R1 & 0x04)) {
			memset(pSprBuf, 0, 16);
		}
	}
	// This is probably not necessary, we don't need to report whether we overflowed sprites anymore since this routine is handling the flagging	
	return NumSpritesToDraw + (OverflowedSprites ? 1 : 0); 
}