#include <kos.h>
#include "macros.h"

#include "pNesX.h"
#include "pNesX_PPU_DC.h"
#include "pNesX_DrawLine_BG_C.h"

extern PPU_Info ppuinfo;

void pNesX_DrawLine_BG_C(unsigned char* pPoint) {
	/* C Background Renderer Vars */
	uint16 nX;
	uint16 nY;
	uint16 nY4;
	uint16 nYBit;
	unsigned char pPalTbl;
	int nIdx;
	int index;
	uint16 nNameTable;
	unsigned char* pbyCharData;
	unsigned char* pbyNameTable;
	unsigned char* pAlBase;

	nNameTable = ((ppuinfo.PPU_Addr & 0x0C00) >> 10) + 8;
	nX = (ppuinfo.PPU_Addr & 0x001F);
	nY = ((ppuinfo.PPU_Addr >> 5) & 0x001F);
	nYBit = ((ppuinfo.PPU_Addr >> 12) & 0x0007);
	nY4 = ( ( nY & 2 ) << 1 );

	pbyNameTable = PPUBANK[nNameTable] + nY * 32 + nX;
	pAlBase = PPUBANK[nNameTable] + 0x03C0 + ((nY / 4) * 8);
	pPalTbl = (( (pAlBase[nX >> 2] >> ( ( nX & 2 ) + nY4 ) ) & 3 ) << 2 );

	unsigned char nameTableValue = *pbyNameTable;
	unsigned char characterBank = ((ppuinfo.PPU_R0 & R0_BG_ADDR) ? 4 : 0) + (nameTableValue >> 6);
	unsigned char characterIndex = (nameTableValue & 0x3F);
	unsigned char patternData[8];
	unsigned char* pbyBGData = PPUBANK[characterBank] + (characterIndex << 4) + (nYBit);
	unsigned char byData1 = ( ( pbyBGData[ 0 ] >> 1 ) & 0x55 ) | ( pbyBGData[ 8 ] & 0xAA );
    unsigned char byData2 = ( pbyBGData[ 0 ] & 0x55 ) | ( ( pbyBGData[ 8 ] << 1 ) & 0xAA );
	patternData[ 0 ]     = ( byData1 >> 6 ) & 3;
	patternData[ 1 ] = ( byData2 >> 6 ) & 3;
	patternData[ 2 ] = ( byData1 >> 4 ) & 3;
	patternData[ 3 ] = ( byData2 >> 4 ) & 3;
	patternData[ 4 ] = ( byData1 >> 2 ) & 3;
	patternData[ 5 ] = ( byData2 >> 2 ) & 3;
	patternData[ 6 ] = byData1 & 3;
	patternData[ 7 ] = byData2 & 3;
	pbyCharData = patternData;

	pAlBase = PPUBANK[nNameTable] + 0x03C0 + ((nY / 4) * 8);
	pPalTbl = (( (pAlBase[nX >> 2] >> ( ( nX & 2 ) + nY4 ) ) & 3 ) << 2 );	

	for (index = ppuinfo.PPU_Scr_H_Bit; index < 8; index++)
	{
		*(pPoint++) = pPalTbl + pbyCharData[index];
	}

	nX++;

	// crossing a name table boundary
	if (!(nX & 0x001f))
	{
		nNameTable ^= NAME_TABLE_H_MASK;
		nX = 0;
		pbyNameTable = PPUBANK[nNameTable] + nY * 32 + nX;
		pAlBase = PPUBANK[nNameTable] + 0x03C0 + ((nY / 4) * 8);
	}
	else
		pbyNameTable++;

	for (nIdx = 1; nIdx < 32; nIdx++)
	{
		nameTableValue = *pbyNameTable;
		characterBank = ((ppuinfo.PPU_R0 & R0_BG_ADDR) ? 4 : 0) + (nameTableValue >> 6);
		characterIndex = (nameTableValue & 0x3F);
		pbyBGData = PPUBANK[characterBank] + (characterIndex << 4) + (nYBit);
		byData1 = ( ( pbyBGData[ 0 ] >> 1 ) & 0x55 ) | ( pbyBGData[ 8 ] & 0xAA );
		byData2 = ( pbyBGData[ 0 ] & 0x55 ) | ( ( pbyBGData[ 8 ] << 1 ) & 0xAA );
		patternData[ 0 ]     = ( byData1 >> 6 ) & 3;
		patternData[ 1 ] = ( byData2 >> 6 ) & 3;
		patternData[ 2 ] = ( byData1 >> 4 ) & 3;
		patternData[ 3 ] = ( byData2 >> 4 ) & 3;
		patternData[ 4 ] = ( byData1 >> 2 ) & 3;
		patternData[ 5 ] = ( byData2 >> 2 ) & 3;
		patternData[ 6 ] = byData1 & 3;
		patternData[ 7 ] = byData2 & 3;
		pbyCharData = patternData;

		pPalTbl = (( (pAlBase[nX >> 2] >> ( ( nX & 2 ) + nY4 ) ) & 3 ) << 2 );

		pPoint[0] = pPalTbl + pbyCharData[0];
		pPoint[1] = pPalTbl + pbyCharData[1];
		pPoint[2] = pPalTbl + pbyCharData[2];
		pPoint[3] = pPalTbl + pbyCharData[3];
		pPoint[4] = pPalTbl + pbyCharData[4];
		pPoint[5] = pPalTbl + pbyCharData[5];
		pPoint[6] = pPalTbl + pbyCharData[6];
		pPoint[7] = pPalTbl + pbyCharData[7];

		pPoint += 8;
		nX++;

		// are we crossing a name table boundary?
		if(!(nX & 0x001F))
		{
			nNameTable ^= NAME_TABLE_H_MASK;
			nX = 0;
			pbyNameTable = PPUBANK[nNameTable] + nY * 32 + nX;
			pAlBase = PPUBANK[nNameTable] + 0x03C0 + ((nY / 4) * 8);
		}
		else
			pbyNameTable++;
	}

	nameTableValue = *pbyNameTable;
	characterBank = ((ppuinfo.PPU_R0 & R0_BG_ADDR) ? 4 : 0) + (nameTableValue >> 6);
	characterIndex = (nameTableValue & 0x3F);
	pbyBGData = PPUBANK[characterBank] + (characterIndex << 4) + (nYBit);
	byData1 = ( ( pbyBGData[ 0 ] >> 1 ) & 0x55 ) | ( pbyBGData[ 8 ] & 0xAA );
	byData2 = ( pbyBGData[ 0 ] & 0x55 ) | ( ( pbyBGData[ 8 ] << 1 ) & 0xAA );
	patternData[ 0 ]     = ( byData1 >> 6 ) & 3;
	patternData[ 1 ] = ( byData2 >> 6 ) & 3;
	patternData[ 2 ] = ( byData1 >> 4 ) & 3;
	patternData[ 3 ] = ( byData2 >> 4 ) & 3;
	patternData[ 4 ] = ( byData1 >> 2 ) & 3;
	patternData[ 5 ] = ( byData2 >> 2 ) & 3;
	patternData[ 6 ] = byData1 & 3;
	patternData[ 7 ] = byData2 & 3;
	pbyCharData = patternData;
	pPalTbl = (( (pAlBase[nX >> 2] >> ( ( nX & 2 ) + nY4 ) ) & 3 ) << 2 );

	for (index = 0; index < ppuinfo.PPU_Scr_H_Bit; index++)
	{
		*(pPoint++) = pPalTbl + pbyCharData[index];
	}

}