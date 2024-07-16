#include <kos.h>
#include "macros.h"

#include "pNesX.h"
#include "pNesX_PPU_DC.h"
//#include "pNesX_System_DC.h"
#include "pNesX_DrawLine_BG_C.h"
#include "pNesX_mapper.h"

extern PPU_Info ppuinfo;

void pNesX_Map9DrawLine_BG_C(unsigned char* pPoint) {
	/* C Background Renderer Vars */
	uint16 nX;
	uint16 nY;
	uint16 nY4;
	uint16 nYBit;
	unsigned char pPalTbl;
	uint16 nesaddr;
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

	nesaddr = (characterBank * 0x400) + (characterIndex << 4) + nYBit;
	switch (nesaddr) {
		case 0x1FD8:
		case 0x1FD9:
		case 0x1FDA:
		case 0x1FDB:
		case 0x1FDC:
		case 0x1FDD:
		case 0x1FDE:
		case 0x1FDF:
		case 0x1FE8:
		case 0x1FE9:
		case 0x1FEA:
		case 0x1FEB:
		case 0x1FEC:
		case 0x1FED:
		case 0x1FEE:
		case 0x1FEF:		
			Map9_PPU_Latch_FDFE(nesaddr);
			break;
	}
	nesaddr += 8;
	switch (nesaddr) {
		case 0x1FD8:
		case 0x1FD9:
		case 0x1FDA:
		case 0x1FDB:
		case 0x1FDC:
		case 0x1FDD:
		case 0x1FDE:
		case 0x1FDF:
		case 0x1FE8:
		case 0x1FE9:
		case 0x1FEA:
		case 0x1FEB:
		case 0x1FEC:
		case 0x1FED:
		case 0x1FEE:
		case 0x1FEF:		
			Map9_PPU_Latch_FDFE(nesaddr);
			break;
	}

	for (index = ppuinfo.PPU_Scr_H_Bit; index < 8; index++) {
		*(pPoint++) = pPalTbl + pbyCharData[index];
	}

	nX++;

	// crossing a name table boundary
	if (!(nX & 0x001f)) {
		nNameTable ^= NAME_TABLE_H_MASK;
		nX = 0;
		pbyNameTable = PPUBANK[nNameTable] + nY * 32 + nX;
		pAlBase = PPUBANK[nNameTable] + 0x03C0 + ((nY / 4) * 8);
	} else {
		pbyNameTable++;
	}

	for (nIdx = 1; nIdx < 32; nIdx++) {
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

		nesaddr = (characterBank * 0x400) + (characterIndex << 4) + nYBit;
		switch (nesaddr) {
			case 0x1FD8:
			case 0x1FD9:
			case 0x1FDA:
			case 0x1FDB:
			case 0x1FDC:
			case 0x1FDD:
			case 0x1FDE:
			case 0x1FDF:
			case 0x1FE8:
			case 0x1FE9:
			case 0x1FEA:
			case 0x1FEB:
			case 0x1FEC:
			case 0x1FED:
			case 0x1FEE:
			case 0x1FEF:		
				Map9_PPU_Latch_FDFE(nesaddr);
				break;
		}
		nesaddr += 8;
		switch (nesaddr) {
			case 0x1FD8:
			case 0x1FD9:
			case 0x1FDA:
			case 0x1FDB:
			case 0x1FDC:
			case 0x1FDD:
			case 0x1FDE:
			case 0x1FDF:
			case 0x1FE8:
			case 0x1FE9:
			case 0x1FEA:
			case 0x1FEB:
			case 0x1FEC:
			case 0x1FED:
			case 0x1FEE:
			case 0x1FEF:		
				Map9_PPU_Latch_FDFE(nesaddr);
				break;
		}

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

	nesaddr = (characterBank * 0x400) + (characterIndex << 4) + nYBit;
	switch (nesaddr) {
		case 0x1FD8:
		case 0x1FD9:
		case 0x1FDA:
		case 0x1FDB:
		case 0x1FDC:
		case 0x1FDD:
		case 0x1FDE:
		case 0x1FDF:
		case 0x1FE8:
		case 0x1FE9:
		case 0x1FEA:
		case 0x1FEB:
		case 0x1FEC:
		case 0x1FED:
		case 0x1FEE:
		case 0x1FEF:		
			Map9_PPU_Latch_FDFE(nesaddr);
			break;
	}
	nesaddr += 8;
	switch (nesaddr) {
		case 0x1FD8:
		case 0x1FD9:
		case 0x1FDA:
		case 0x1FDB:
		case 0x1FDC:
		case 0x1FDD:
		case 0x1FDE:
		case 0x1FDF:
		case 0x1FE8:
		case 0x1FE9:
		case 0x1FEA:
		case 0x1FEB:
		case 0x1FEC:
		case 0x1FED:
		case 0x1FEE:
		case 0x1FEF:		
			Map9_PPU_Latch_FDFE(nesaddr);
			break;
	}	

	for (index = 0; index < ppuinfo.PPU_Scr_H_Bit; index++) {
		*(pPoint++) = pPalTbl + pbyCharData[index];
	}

}

void pNesX_Map9Simulate_BG_C(uint16* pPoint) {
/*
	uint16 nX;
	uint16 nY;
//	uint16 nY4;
	uint16 nYBit;

	uint16 nesaddr;
	int nIdx;
	uint16 nNameTable;
	unsigned char* pbyCharData;
	unsigned char* pbyNameTable;

	nNameTable = ((ppuinfo.PPU_Addr & 0x0C00) >> 10) + 8;
	nX = (ppuinfo.PPU_Addr & 0x001F);
	nY = ((ppuinfo.PPU_Addr >> 5) & 0x001F);
	nYBit = ((ppuinfo.PPU_Addr >> 12) & 0x0007) << 3;
//	nY4 = ( ( nY & 2 ) << 1 );

	pbyNameTable = PPUBANK[nNameTable] + nY * 32 + nX;
	pbyCharData = ppuinfo.PPU_BG_Base + (*pbyNameTable << 6) + nYBit;

	nesaddr = ((uint32)pbyCharData - (uint32)ChrBuf) / 4;
	if(((nesaddr) & 0x0FC0) == 0x0FC0)
	{
		if((((nesaddr) & 0x0FF0) == 0x0FD0) || (((nesaddr) & 0x0FF0) == 0x0FE0))
		{
			Map9_PPU_Latch_FDFE(nesaddr);
		}
	}

	nX++;

	// crossing a name table boundary
	if (!(nX & 0x001f))
	{
		nNameTable ^= NAME_TABLE_H_MASK;
		nX = 0;
		pbyNameTable = PPUBANK[nNameTable] + nY * 32 + nX;
	}
	else
		pbyNameTable++;

	for (nIdx = 1; nIdx < 32; nIdx++)
	{
		pbyCharData = ppuinfo.PPU_BG_Base + (*pbyNameTable << 6) + nYBit;

		nesaddr = ((uint32)pbyCharData - (uint32)ChrBuf) / 4;
		if(((nesaddr) & 0x0FC0) == 0x0FC0)
		{
			if((((nesaddr) & 0x0FF0) == 0x0FD0) || (((nesaddr) & 0x0FF0) == 0x0FE0))
			{
				Map9_PPU_Latch_FDFE(nesaddr);
			}
		}

		nX++;

		// are we crossing a name table boundary?
		if(!(nX & 0x001F))
		{
			nNameTable ^= NAME_TABLE_H_MASK;
			nX = 0;
			pbyNameTable = PPUBANK[nNameTable] + nY * 32 + nX;
		}
		else
			pbyNameTable++;
	}

	pbyCharData = ppuinfo.PPU_BG_Base + (*pbyNameTable << 6) + nYBit;

	nesaddr = ((uint32)pbyCharData - (uint32)ChrBuf) / 4;
	if(((nesaddr) & 0x0FC0) == 0x0FC0)
	{
		if((((nesaddr) & 0x0FF0) == 0x0FD0) || (((nesaddr) & 0x0FF0) == 0x0FE0))
		{
			Map9_PPU_Latch_FDFE(nesaddr);
		}
	}
*/
}