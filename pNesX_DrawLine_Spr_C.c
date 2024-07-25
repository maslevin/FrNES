#include <kos.h>
#include "macros.h"

#include "pNesX.h"
#include "pNesX_PPU_DC.h"
#include "pNesX_DrawLine_BG_C.h"
#include "Mapper.h"
#include "Mapper_5.h"
#include "pNesX_System_DC.h"

extern PPU_Info ppuinfo;
extern unsigned char pSprBuf[];
extern int SpriteJustHit;
extern int SprBufClean;

uint16 pNesX_DrawLine_Spr_C(uint16* pSprBuf) {
	int nX;
	int nY;
	int nYBit;
	unsigned char *pSPRRAM;
	int nAttr;
	uint16 nSprCnt = 0;
	uint16 bySprCol;
	unsigned char* pbyChrData;
	unsigned char* pbyBGData;
	unsigned char patternData[8];
	unsigned char byData1;
	unsigned char byData2;
	
	if ( PPU_R1 & R1_SHOW_SP ) {
		// Reset Scanline Sprite Count
		PPU_R2 &= ~R2_MAX_SP;

		if (MapperNo == 5) {
			Mapper_5_PPU_Latch_RenderScreen(0, 0);
		}

		for ( pSPRRAM = SPRRAM + ( 63 << 2 ); (pSPRRAM >= SPRRAM); pSPRRAM -= 4 ) {
			nY = pSPRRAM[ SPR_Y ];
			if ( nY > ppuinfo.PPU_Scanline || nY + ppuinfo.PPU_SP_Height <= ppuinfo.PPU_Scanline )
				continue;  // Next sprite

			if (nSprCnt == 0) {
				memset4(pSprBuf, 0, 272 * 2);
			}

			nSprCnt++;

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

		if (!(PPU_R1 & 0x04)) {
			memset(pSprBuf, 0, 16);
		}
	}
	return nSprCnt;
}