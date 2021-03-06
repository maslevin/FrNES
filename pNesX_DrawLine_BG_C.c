#include <kos.h>
#include "pNesX.h"
#include "pNesX_PPU_DC.h"
#include "pNesX_DrawLine_BG_C.h"

extern PPU_Info ppuinfo;

void pNesX_DrawLine_BG_C(uint16* pPoint)
{
	/* C Background Renderer Vars */
	uint16 nX;
	uint16 nY;
	uint16 nY4;
	uint16 nYBit;
	uint16 *pPalTbl;
	int nIdx;
	int index;
	uint16 nNameTable;
	unsigned char* pbyCharData;
	unsigned char* pbyNameTable;
	unsigned char* pAlBase;

	nNameTable = ((ppuinfo.PPU_Addr & 0x0C00) >> 10) + 8;
	nX = (ppuinfo.PPU_Addr & 0x001F);
	nY = ((ppuinfo.PPU_Addr >> 5) & 0x001F);
	nYBit = ((ppuinfo.PPU_Addr >> 12) & 0x0007) << 3;
	nY4 = ( ( nY & 2 ) << 1 );

	pbyNameTable = PPUBANK[nNameTable] + nY * 32 + nX;
	pbyCharData = ppuinfo.PPU_BG_Base + (*pbyNameTable << 6) + nYBit;
	pAlBase = PPUBANK[nNameTable] + 0x03C0 + ((nY / 4) * 8);
	pPalTbl = &PalTable[ (( (pAlBase[nX >> 2] >> ( ( nX & 2 ) + nY4 ) ) & 3 ) << 2 )];

	for (index = ppuinfo.PPU_Scr_H_Bit; index < 8; index++)
	{
		*(pPoint++) = pPalTbl[pbyCharData[index]];
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
		pbyCharData = ppuinfo.PPU_BG_Base + (*pbyNameTable << 6) + nYBit;
		pPalTbl = &PalTable[ (( (pAlBase[nX >> 2] >> ( ( nX & 2 ) + nY4 ) ) & 3 ) << 2 )];

		pPoint[0] = pPalTbl[pbyCharData[0]];
		pPoint[1] = pPalTbl[pbyCharData[1]];
		pPoint[2] = pPalTbl[pbyCharData[2]];
		pPoint[3] = pPalTbl[pbyCharData[3]];
		pPoint[4] = pPalTbl[pbyCharData[4]];
		pPoint[5] = pPalTbl[pbyCharData[5]];
		pPoint[6] = pPalTbl[pbyCharData[6]];
		pPoint[7] = pPalTbl[pbyCharData[7]];

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

	pbyCharData = ppuinfo.PPU_BG_Base + (*pbyNameTable << 6) + nYBit;
	pPalTbl = &PalTable[ (( (pAlBase[nX >> 2] >> ( ( nX & 2 ) + nY4 ) ) & 3 ) << 2 )];

	for (index = 0; index < ppuinfo.PPU_Scr_H_Bit; index++)
	{
		*(pPoint++) = pPalTbl[pbyCharData[index]];
	}

}