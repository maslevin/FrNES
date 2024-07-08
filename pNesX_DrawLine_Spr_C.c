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

uint16 pNesX_DrawLine_Spr_C() {
	int nX;
	int nY;
	int nYBit;
	unsigned char *pbyChrData;
	unsigned char *pSPRRAM;
	int nAttr;
	int nSprCnt = 0;
	unsigned char bySprCol;
	
	if ( PPU_R1 & R1_SHOW_SP )
	{
		// Reset Scanline Sprite Count
		PPU_R2 &= ~R2_MAX_SP;

		for ( pSPRRAM = SPRRAM + ( 63 << 2 ); (pSPRRAM >= SPRRAM); pSPRRAM -= 4 )
		{
			nY = pSPRRAM[ SPR_Y ];
			if ( nY > ppuinfo.PPU_Scanline || nY + ppuinfo.PPU_SP_Height <= ppuinfo.PPU_Scanline )
				continue;  // Next sprite

			if (nSprCnt == 0) {
				memset4(pSprBuf, 0, 66 * 4);
			}

			nSprCnt++;

			nAttr = pSPRRAM[ SPR_ATTR ];
			nYBit = ppuinfo.PPU_Scanline - nY;
			nYBit = ( nAttr & SPR_ATTR_V_FLIP ) ? ( ppuinfo.PPU_SP_Height - nYBit - 1) << 3 : nYBit << 3;

			unsigned char nameTableValue = (ppuinfo.PPU_R0 & R0_SP_SIZE) ? 
				(pSPRRAM[SPR_CHR] & 0xfe) :
				(pSPRRAM[SPR_CHR]);
			unsigned char characterBank = ((ppuinfo.PPU_R0 & R0_SP_ADDR) ? 4 : 0) + (nameTableValue >> 6);
			unsigned char characterIndex = (nameTableValue & 0x3F);
			pbyChrData = decompressCharacter(characterBank, characterIndex) + nYBit;

			nAttr ^= SPR_ATTR_PRI;
			bySprCol = ( nAttr & ( SPR_ATTR_COLOR | SPR_ATTR_PRI ) ) << 2;
			nX = pSPRRAM[ SPR_X ];

			if ( nAttr & SPR_ATTR_H_FLIP )
			{
				// Horizontal flip
				if ( pbyChrData[ 7 ] )
					pSprBuf[ nX + 0 ] = bySprCol | pbyChrData[ 7 ];
				if ( pbyChrData[ 6 ] )
					pSprBuf[ nX + 1 ] = bySprCol | pbyChrData[ 6 ];
				if ( pbyChrData[ 5 ] )
					pSprBuf[ nX + 2 ] = bySprCol | pbyChrData[ 5 ];
				if ( pbyChrData[ 4 ] )
					pSprBuf[ nX + 3 ] = bySprCol | pbyChrData[ 4 ];
				if ( pbyChrData[ 3 ] )
					pSprBuf[ nX + 4 ] = bySprCol | pbyChrData[ 3 ];
				if ( pbyChrData[ 2 ] )
					pSprBuf[ nX + 5 ] = bySprCol | pbyChrData[ 2 ];
				if ( pbyChrData[ 1 ] )
					pSprBuf[ nX + 6 ] = bySprCol | pbyChrData[ 1 ];
				if ( pbyChrData[ 0 ] )
					pSprBuf[ nX + 7 ] = bySprCol | pbyChrData[ 0 ];
			}
			else
			{
				// Non flip
				if ( pbyChrData[ 0 ] )
					pSprBuf[ nX + 0 ] = bySprCol | pbyChrData[ 0 ];
				if ( pbyChrData[ 1 ] )
					pSprBuf[ nX + 1 ] = bySprCol | pbyChrData[ 1 ];
				if ( pbyChrData[ 2 ] )
					pSprBuf[ nX + 2 ] = bySprCol | pbyChrData[ 2 ];
				if ( pbyChrData[ 3 ] )
					pSprBuf[ nX + 3 ] = bySprCol | pbyChrData[ 3 ];
				if ( pbyChrData[ 4 ] )
					pSprBuf[ nX + 4 ] = bySprCol | pbyChrData[ 4 ];
				if ( pbyChrData[ 5 ] )
					pSprBuf[ nX + 5 ] = bySprCol | pbyChrData[ 5 ];
				if ( pbyChrData[ 6 ] )
					pSprBuf[ nX + 6 ] = bySprCol | pbyChrData[ 6 ];
				if ( pbyChrData[ 7 ] )
					pSprBuf[ nX + 7 ] = bySprCol | pbyChrData[ 7 ];
			}
		}	
	}
	return nSprCnt;
}