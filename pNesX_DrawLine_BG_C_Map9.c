#include <kos.h>
#include "macros.h"

#include "pNesX.h"
#include "pNesX_PPU_DC.h"
#include "pNesX_DrawLine_BG_C.h"
#include "Mapper.h"
#include "Mapper_9.h"

void pNesX_Map9DrawLine_BG_C(unsigned char* pPoint) {
	uint16 nX;
	uint16 nY;
	uint16 nY4;
	uint16 nYBit;
	unsigned char pPalTbl;
	uint16 nesaddr;
	int nIdx;
	uint16 nNameTable;
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

	nesaddr = (characterBank * 0x400) + (characterIndex << 4) + nYBit;
	switch (nesaddr) {
		case 0x1FD8 ... 0x1FDF:
		case 0x1FE8 ... 0x1FEF:
			Mapper_9_PPU_Latch_FDFE(nesaddr);
			break;
		default:
			nesaddr += 8;
			switch (nesaddr) {
				case 0x1FD8 ... 0x1FDF:
				case 0x1FE8 ... 0x1FEF:		
					Mapper_9_PPU_Latch_FDFE(nesaddr);
					break;
			}
			break;
	}

	unsigned char* pbyBGData = PPUBANK[characterBank] + (characterIndex << 4) + (nYBit);
	unsigned char byData1 = ( ( pbyBGData[ 0 ] >> 1 ) & 0x55 ) | ( pbyBGData[ 8 ] & 0xAA );
    unsigned char byData2 = ( pbyBGData[ 0 ] & 0x55 ) | ( ( pbyBGData[ 8 ] << 1 ) & 0xAA );

	switch (ppuinfo.PPU_Scr_H_Bit) {
		case 0:
			*(pPoint++) = pPalTbl + (( byData1 >> 6 ) & 3);
		case 1:
			*(pPoint++) = pPalTbl + (( byData2 >> 6 ) & 3);
		case 2:
			*(pPoint++) = pPalTbl + (( byData1 >> 4 ) & 3);
		case 3:
			*(pPoint++) = pPalTbl + (( byData2 >> 4 ) & 3);
		case 4:
			*(pPoint++) = pPalTbl + (( byData1 >> 2 ) & 3);
		case 5:
			*(pPoint++) = pPalTbl + (( byData2 >> 2 ) & 3);
		case 6:
			*(pPoint++) = pPalTbl + (byData1 & 3);
		case 7:
			*(pPoint++) = pPalTbl + (byData2 & 3);
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

		nesaddr = (characterBank * 0x400) + (characterIndex << 4) + nYBit;
		switch (nesaddr) {
			case 0x1FD8 ... 0x1FDF:
			case 0x1FE8 ... 0x1FEF:
				Mapper_9_PPU_Latch_FDFE(nesaddr);
				break;
			default:
				nesaddr += 8;
				switch (nesaddr) {
					case 0x1FD8 ... 0x1FDF:
					case 0x1FE8 ... 0x1FEF:		
						Mapper_9_PPU_Latch_FDFE(nesaddr);
						break;
				}
				break;
		}

		pbyBGData = PPUBANK[characterBank] + (characterIndex << 4) + (nYBit);
		byData1 = ( ( pbyBGData[ 0 ] >> 1 ) & 0x55 ) | ( pbyBGData[ 8 ] & 0xAA );
		byData2 = ( pbyBGData[ 0 ] & 0x55 ) | ( ( pbyBGData[ 8 ] << 1 ) & 0xAA );
		pPalTbl = (( (pAlBase[nX >> 2] >> ( ( nX & 2 ) + nY4 ) ) & 3 ) << 2 );

		*(pPoint++) = pPalTbl + (( byData1 >> 6 ) & 3);
		*(pPoint++) = pPalTbl + (( byData2 >> 6 ) & 3);
		*(pPoint++) = pPalTbl + (( byData1 >> 4 ) & 3);
		*(pPoint++) = pPalTbl + (( byData2 >> 4 ) & 3);
		*(pPoint++) = pPalTbl + (( byData1 >> 2 ) & 3);
		*(pPoint++) = pPalTbl + (( byData2 >> 2 ) & 3);
		*(pPoint++) = pPalTbl + (byData1 & 3);
		*(pPoint++) = pPalTbl + (byData2 & 3);

		nX++;

		// are we crossing a name table boundary?
		if (!(nX & 0x001F)) {
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

	nesaddr = (characterBank * 0x400) + (characterIndex << 4) + nYBit;
	switch (nesaddr) {
		case 0x1FD8 ... 0x1FDF:
		case 0x1FE8 ... 0x1FEF:
			Mapper_9_PPU_Latch_FDFE(nesaddr);
			break;
		default:
			nesaddr += 8;
			switch (nesaddr) {
				case 0x1FD8 ... 0x1FDF:
				case 0x1FE8 ... 0x1FEF:		
					Mapper_9_PPU_Latch_FDFE(nesaddr);
					break;
			}
			break;
	}

	pbyBGData = PPUBANK[characterBank] + (characterIndex << 4) + (nYBit);
	byData1 = ( ( pbyBGData[ 0 ] >> 1 ) & 0x55 ) | ( pbyBGData[ 8 ] & 0xAA );
	byData2 = ( pbyBGData[ 0 ] & 0x55 ) | ( ( pbyBGData[ 8 ] << 1 ) & 0xAA );
	pPalTbl = (( (pAlBase[nX >> 2] >> ( ( nX & 2 ) + nY4 ) ) & 3 ) << 2 );	

	switch (ppuinfo.PPU_Scr_H_Bit) {
		case 1:
			*(pPoint++) = pPalTbl + (( byData1 >> 6 ) & 3);
			break;
		case 2:
			*(pPoint++) = pPalTbl + (( byData1 >> 6 ) & 3);
			*(pPoint++) = pPalTbl + (( byData2 >> 6 ) & 3);
			break;
		case 3:
			*(pPoint++) = pPalTbl + (( byData1 >> 6 ) & 3);
			*(pPoint++) = pPalTbl + (( byData2 >> 6 ) & 3);
			*(pPoint++) = pPalTbl + (( byData1 >> 4 ) & 3);		
			break;
		case 4:
			*(pPoint++) = pPalTbl + (( byData1 >> 6 ) & 3);
			*(pPoint++) = pPalTbl + (( byData2 >> 6 ) & 3);
			*(pPoint++) = pPalTbl + (( byData1 >> 4 ) & 3);
			*(pPoint++) = pPalTbl + (( byData2 >> 4 ) & 3);		
			break;
		case 5:
			*(pPoint++) = pPalTbl + (( byData1 >> 6 ) & 3);
			*(pPoint++) = pPalTbl + (( byData2 >> 6 ) & 3);
			*(pPoint++) = pPalTbl + (( byData1 >> 4 ) & 3);
			*(pPoint++) = pPalTbl + (( byData2 >> 4 ) & 3);
			*(pPoint++) = pPalTbl + (( byData1 >> 2 ) & 3);		
			break;
		case 6:
			*(pPoint++) = pPalTbl + (( byData1 >> 6 ) & 3);
			*(pPoint++) = pPalTbl + (( byData2 >> 6 ) & 3);
			*(pPoint++) = pPalTbl + (( byData1 >> 4 ) & 3);
			*(pPoint++) = pPalTbl + (( byData2 >> 4 ) & 3);
			*(pPoint++) = pPalTbl + (( byData1 >> 2 ) & 3);
			*(pPoint++) = pPalTbl + (( byData2 >> 2 ) & 3);		
			break;
		case 7:
			*(pPoint++) = pPalTbl + (( byData1 >> 6 ) & 3);
			*(pPoint++) = pPalTbl + (( byData2 >> 6 ) & 3);
			*(pPoint++) = pPalTbl + (( byData1 >> 4 ) & 3);
			*(pPoint++) = pPalTbl + (( byData2 >> 4 ) & 3);
			*(pPoint++) = pPalTbl + (( byData1 >> 2 ) & 3);
			*(pPoint++) = pPalTbl + (( byData2 >> 2 ) & 3);
			*(pPoint++) = pPalTbl + (byData1 & 3);
			break;
	}

	if (!(PPU_R1 & 0x02)) {
		pPoint -= 256;
		memset(pPoint, 0, 8);
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