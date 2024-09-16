#include <kos.h>
#include "macros.h"

#include "pNesX.h"
#include "Mapper_5.h"
#include "pNesX_PPU_DC.h"
#include "pNesX_DrawLine_BG_C.h"

#include "profile.h"

extern uint8 MapperNo;

__attribute__ ((hot)) void pNesX_DrawLine_BG_C(unsigned char* pPoint) {
	startProfiling(2);
	uint16 nX;
	uint16 nY;
	uint16 nY4;
	uint16 nYBit;
	unsigned char* pPalTbl;
	uint16 nNameTable;
	unsigned char* pbyNameTable;
	unsigned char* pAlBase;
	unsigned char* paletteRegisters = &PPURAM[0x3f00];

	nNameTable = ((ppuinfo.PPU_Addr & 0x0C00) >> 10) + 8;
	nX = (ppuinfo.PPU_Addr & 0x001F);
	nY = ((ppuinfo.PPU_Addr >> 5) & 0x001F);
	nYBit = ((ppuinfo.PPU_Addr >> 12) & 0x0007);
	nY4 = ( ( nY & 2 ) << 1 );

	pbyNameTable = PPUBANK[nNameTable] + (nY << 5) + nX;
	pAlBase = PPUBANK[nNameTable] + 0x03C0 + ((nY >> 2) << 3);
	pPalTbl = &paletteRegisters[(( (pAlBase[nX >> 2] >> ( ( nX & 2 ) + nY4 ) ) & 0x3 ) << 2 )];

	if (MapperNo == 5) {
		Mapper_5_PPU_Latch_RenderScreen(1, 0);
	}

	unsigned char nameTableValue = *pbyNameTable;
	unsigned char characterBank = ((ppuinfo.PPU_R0 & R0_BG_ADDR) ? 4 : 0) + (nameTableValue >> 6);
	unsigned char characterIndex = (nameTableValue & 0x3F);
	unsigned char* pbyBGData = PPUBANK[characterBank] + (characterIndex << 4) + (nYBit);
	unsigned char byData1 = ( ( pbyBGData[ 0 ] >> 1 ) & 0x55 ) | ( pbyBGData[ 8 ] & 0xAA );
    unsigned char byData2 = ( pbyBGData[ 0 ] & 0x55 ) | ( ( pbyBGData[ 8 ] << 1 ) & 0xAA );

	switch (ppuinfo.PPU_Scr_H_Bit) {
		case 0:
			*(pPoint++) = pPalTbl[( byData1 >> 6 ) & 0x3];
		case 1:
			*(pPoint++) = pPalTbl[( byData2 >> 6 ) & 0x3];
		case 2:
			*(pPoint++) = pPalTbl[( byData1 >> 4 ) & 0x3];
		case 3:
			*(pPoint++) = pPalTbl[( byData2 >> 4 ) & 0x3];
		case 4:
			*(pPoint++) = pPalTbl[( byData1 >> 2 ) & 0x3];
		case 5:
			*(pPoint++) = pPalTbl[( byData2 >> 2 ) & 0x3];
		case 6:
			*(pPoint++) = pPalTbl[byData1 & 0x3];
		case 7:
			*(pPoint++) = pPalTbl[byData2 & 0x3];
	}

	nX++;

	// crossing a name table boundary
	if (!(nX & 0x001f)) {
		nNameTable ^= NAME_TABLE_H_MASK;
		nX = 0;
		pbyNameTable = PPUBANK[nNameTable] + (nY << 5) + nX;
		pAlBase = PPUBANK[nNameTable] + 0x03C0 + ((nY >> 2) << 3);
	} else {
		pbyNameTable++;
	}

	for (uint8 nIdx = 1; nIdx < 32; nIdx++) {
		nameTableValue = *pbyNameTable;
		characterBank = ((ppuinfo.PPU_R0 & R0_BG_ADDR) ? 4 : 0) + (nameTableValue >> 6);
		characterIndex = (nameTableValue & 0x3F);
		pbyBGData = PPUBANK[characterBank] + (characterIndex << 4) + (nYBit);
		byData1 = ( ( pbyBGData[ 0 ] >> 1 ) & 0x55 ) | ( pbyBGData[ 8 ] & 0xAA );
		byData2 = ( pbyBGData[ 0 ] & 0x55 ) | ( ( pbyBGData[ 8 ] << 1 ) & 0xAA );
		pPalTbl = &paletteRegisters[(( (pAlBase[nX >> 2] >> ( ( nX & 2 ) + nY4 ) ) & 0x3 ) << 2 )];

		*(pPoint++) = pPalTbl[( byData1 >> 6 ) & 0x3];
		*(pPoint++) = pPalTbl[( byData2 >> 6 ) & 0x3];
		*(pPoint++) = pPalTbl[( byData1 >> 4 ) & 0x3];
		*(pPoint++) = pPalTbl[( byData2 >> 4 ) & 0x3];
		*(pPoint++) = pPalTbl[( byData1 >> 2 ) & 0x3];
		*(pPoint++) = pPalTbl[( byData2 >> 2 ) & 0x3];
		*(pPoint++) = pPalTbl[byData1 & 0x3];
		*(pPoint++) = pPalTbl[byData2 & 0x3];

		nX++;

		// are we crossing a name table boundary?
		if (!(nX & 0x001F)) {
			nNameTable ^= NAME_TABLE_H_MASK;
			nX = 0;
			pbyNameTable = PPUBANK[nNameTable] + (nY << 5) + nX;
			pAlBase = PPUBANK[nNameTable] + 0x03C0 + ((nY >> 2) << 3);
		} else {
			pbyNameTable++;
		}
	}

	nameTableValue = *pbyNameTable;
	characterBank = ((ppuinfo.PPU_R0 & R0_BG_ADDR) ? 4 : 0) + (nameTableValue >> 6);
	characterIndex = (nameTableValue & 0x3F);
	pbyBGData = PPUBANK[characterBank] + (characterIndex << 4) + (nYBit);
	byData1 = ( ( pbyBGData[ 0 ] >> 1 ) & 0x55 ) | ( pbyBGData[ 8 ] & 0xAA );
	byData2 = ( pbyBGData[ 0 ] & 0x55 ) | ( ( pbyBGData[ 8 ] << 1 ) & 0xAA );
	pPalTbl = &paletteRegisters[(( (pAlBase[nX >> 2] >> ( ( nX & 2 ) + nY4 ) ) & 0x3 ) << 2 )];
	
	switch (ppuinfo.PPU_Scr_H_Bit) {
		case 1:
			*(pPoint++) = pPalTbl[( byData1 >> 6 ) & 0x3];
			break;
		case 2:
			*(pPoint++) = pPalTbl[( byData1 >> 6 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData2 >> 6 ) & 0x3];
			break;
		case 3:
			*(pPoint++) = pPalTbl[( byData1 >> 6 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData2 >> 6 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData1 >> 4 ) & 0x3];	
			break;
		case 4:
			*(pPoint++) = pPalTbl[( byData1 >> 6 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData2 >> 6 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData1 >> 4 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData2 >> 4 ) & 0x3];	
			break;
		case 5:
			*(pPoint++) = pPalTbl[( byData1 >> 6 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData2 >> 6 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData1 >> 4 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData2 >> 4 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData1 >> 2 ) & 0x3];	
			break;
		case 6:
			*(pPoint++) = pPalTbl[( byData1 >> 6 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData2 >> 6 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData1 >> 4 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData2 >> 4 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData1 >> 2 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData2 >> 2 ) & 0x3];	
			break;
		case 7:
			*(pPoint++) = pPalTbl[( byData1 >> 6 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData2 >> 6 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData1 >> 4 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData2 >> 4 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData1 >> 2 ) & 0x3];
			*(pPoint++) = pPalTbl[( byData2 >> 2 ) & 0x3];
			*(pPoint++) = pPalTbl[byData1 & 0x3];
			break;
	}

	if (!(PPU_R1 & 0x02)) {
		pPoint -= 256;
		memset(pPoint, pPalTbl[0], 8);
	}
	endProfiling(2);
}