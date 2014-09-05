#include <KOS.h>
#include "pNesX_PPU_DC.h"
#include "pNesX_System_DC.h"

extern int16* WorkFrame;
extern uint16 Scanline_Buffer[];
extern unsigned char* pSprBuf;
extern unsigned char pBGBuf[];
extern PPU_Info ppuinfo;
//unsigned char pSprBuf[264];

/*===================================================================*/
/*                                                                   */
/*              pNesX_NewDrawLine() : Render a scanline              */
/*                                                                   */
/*===================================================================*/
void pNesX_DrawLine()
{
/*
 *  Render a scanline
 *
 */

	int nX;
	int nY;
	int nY4;
	int nYBit;
	uint16 *pPalTbl;
	unsigned char *pAttrBase;
	uint16 *pPoint;
	int nNameTable;
	unsigned char *pbyNameTable;
	unsigned char *pbyChrData;
	unsigned char *pSPRRAM;
	int nAttr;
	int nSprCnt;
	int nIdx;
	int nSprData;
	int index;
	unsigned char bySprCol;
	unsigned char pSprBuf[ 264 ];
	unsigned char pBGBuf[ 264 ];
	//unsigned char pSprBuf[8];
	void* texture_address;

	//texture_address is the Texture the frame currently being rendered will be displayed in
	texture_address = &WorkFrame[ ppuinfo.PPU_Scanline * 256 ];
	pPoint = &Scanline_Buffer[0];

	//Index of the Name Table currently being used
	nNameTable = ppuinfo.PPU_NameTableBank;

	//Adjust the scanline to render for Vertical Scroll
	//Start VScroll Calc
	nY = ppuinfo.PPU_Scr_V_Byte + (ppuinfo.PPU_Scanline >> 3);
	nYBit = ppuinfo.PPU_Scr_V_Bit + (ppuinfo.PPU_Scanline & 7);

	if ( nYBit > 7 )
	{
		++nY;
		nYBit &= 7;
	}
	nYBit <<= 3;

	if ( nY > 29 )
	{
		// Next NameTable (An up-down direction)
		nNameTable ^= NAME_TABLE_V_MASK;
		nY -= 30;
	}
	nY4 = ( ( nY & 2 ) << 1 );

	//Start HScroll Calc... more Simple
	nX = ppuinfo.PPU_Scr_H_Byte;

	//----------------------------------------------------------------------------------------BG Rendering
	//Start of BG Rendering
	pbyNameTable = PPUBANK[ nNameTable ] + nY * 32 + nX;
	pbyChrData = ppuinfo.PPU_BG_Base + ( *pbyNameTable << 6 ) + nYBit;
	pAttrBase = PPUBANK[ nNameTable ] + 0x3c0 + ( nY / 4 ) * 8;

	pPalTbl =  &PalTable[ ( ( ( pAttrBase[ nX >> 2 ] >> ( ( nX & 2 ) + nY4 ) ) & 3 ) << 2 ) ];
	for ( nIdx = ppuinfo.PPU_Scr_H_Bit; nIdx < 8; ++nIdx )
	{
		if (pbyChrData[nIdx] == 0)
			*( pPoint++ ) = pPalTbl[ pbyChrData[ nIdx ] ] | 0x8000;
		else
			*( pPoint++ ) = pPalTbl[ pbyChrData[ nIdx ] ];
	}

	++nX;
	++pbyNameTable;

	//Traverse 32 Entries of the Name Table and Draw Upto 256 Characters
	//Render 1st BG Nametable
	for ( ; nX < 32; ++nX )
	{
		pbyChrData = ppuinfo.PPU_BG_Base + ( *pbyNameTable << 6 ) + nYBit;
		pPalTbl = &PalTable[ (((pAttrBase[ nX >> 2 ] >> ((nX & 2) + nY4)) & 3 ) << 2)];
		pPoint[ 0 ] = pPalTbl[ pbyChrData[ 0 ] ]; 
		pPoint[ 1 ] = pPalTbl[ pbyChrData[ 1 ] ];
		pPoint[ 2 ] = pPalTbl[ pbyChrData[ 2 ] ];
		pPoint[ 3 ] = pPalTbl[ pbyChrData[ 3 ] ];
		pPoint[ 4 ] = pPalTbl[ pbyChrData[ 4 ] ];
		pPoint[ 5 ] = pPalTbl[ pbyChrData[ 5 ] ];
		pPoint[ 6 ] = pPalTbl[ pbyChrData[ 6 ] ];
		pPoint[ 7 ] = pPalTbl[ pbyChrData[ 7 ] ];		
/*		((uint32*)pPoint)[ 0 ] = (pPalTbl[ pbyChrData[ 1 ] ] << 16) | pPalTbl[ pbyChrData[ 0 ] ]; 
		((uint32*)pPoint)[ 1 ] = (pPalTbl[ pbyChrData[ 3 ] ] << 16) | pPalTbl[ pbyChrData[ 2 ] ]; 
		((uint32*)pPoint)[ 2 ] = (pPalTbl[ pbyChrData[ 5 ] ] << 16) | pPalTbl[ pbyChrData[ 4 ] ]; 
		((uint32*)pPoint)[ 3 ] = (pPalTbl[ pbyChrData[ 7 ] ] << 16) | pPalTbl[ pbyChrData[ 6 ] ]; */
		pPoint += 8;

		++pbyNameTable;
	}

	// Get next nametable
	nNameTable ^= NAME_TABLE_H_MASK;
	pbyNameTable = PPUBANK[ nNameTable ] + nY * 32;
	pAttrBase = PPUBANK[ nNameTable ] + 0x3c0 + ( nY / 4 ) * 8;

	//Render left on the other side of the Screen Using the next nametable
	for ( nX = 0; nX < ppuinfo.PPU_Scr_H_Byte; ++nX )
	{
		pbyChrData = ppuinfo.PPU_BG_Base + ( *pbyNameTable << 6 ) + nYBit;
		pPalTbl = &PalTable[ ( ( ( pAttrBase[ nX >> 2 ] >> ( ( nX & 2 ) + nY4 ) ) & 3 ) << 2 ) ];

		pPoint[ 0 ] = pPalTbl[ pbyChrData[ 0 ] ]; 
		pPoint[ 1 ] = pPalTbl[ pbyChrData[ 1 ] ];
		pPoint[ 2 ] = pPalTbl[ pbyChrData[ 2 ] ];
		pPoint[ 3 ] = pPalTbl[ pbyChrData[ 3 ] ];
		pPoint[ 4 ] = pPalTbl[ pbyChrData[ 4 ] ];
		pPoint[ 5 ] = pPalTbl[ pbyChrData[ 5 ] ];
		pPoint[ 6 ] = pPalTbl[ pbyChrData[ 6 ] ];
		pPoint[ 7 ] = pPalTbl[ pbyChrData[ 7 ] ];
		pPoint += 8;

		++pbyNameTable;
	}

	//Render the extra part of a scanline due to hscroll
	pbyChrData = ppuinfo.PPU_BG_Base + ( *pbyNameTable << 6 ) + nYBit;
	pPalTbl = &PalTable[ ( ( ( pAttrBase[ nX >> 2 ] >> ( ( nX & 2 ) + nY4 ) ) & 3 ) << 2 ) ];
	for ( nIdx = 0; nIdx < ppuinfo.PPU_Scr_H_Bit; ++nIdx )
	{
		pPoint[ nIdx ] = pPalTbl[ pbyChrData[ nIdx ] ];
	}
	//--------------------------------------------------------------------------------------End BG Rendering

	// Reset Rendering Point
	pPoint = &Scanline_Buffer[0];
//    pPoint -= ( NES_DISP_WIDTH - ppuinfo.PPU_Scr_H_Bit );
  
  /*-------------------------------------------------------------------*/
  /*  Render a sprite                                                  */
  /*-------------------------------------------------------------------*/

  if ( PPU_R1 & R1_SHOW_SP )
  {
    // Reset Scanline Sprite Count
    PPU_R2 &= ~R2_MAX_SP;

    // Reset sprite buffer
//    pNesX_Uint32MemSet( pSprBuf, 0, NES_DISP_WIDTH + 7 );

    // Render a sprite to the sprite buffer
    nSprCnt = 0;
	for ( pSPRRAM = SPRRAM + ( 63 << 2 ); ((pSPRRAM >= SPRRAM) && (nSprCnt < 8)); pSPRRAM -= 4 )
	{
		nY = pSPRRAM[ SPR_Y ] + 1;
		if ( nY > ppuinfo.PPU_Scanline || nY + ppuinfo.PPU_SP_Height <= ppuinfo.PPU_Scanline )
			continue;  // Next sprite

		/*-------------------------------------------------------------------*/
		/*  A sprite in scanning line                                        */
		/*-------------------------------------------------------------------*/

		// Holizontal Sprite Count +1

		if (nSprCnt == 0)
//			pNesX_Buffer_Fill(pSprBuf, 0x00000000);
			pNesX_Uint32MemSet( pSprBuf, 0, 68);

		++nSprCnt;
      
		nAttr = pSPRRAM[ SPR_ATTR ];
		nYBit = ppuinfo.PPU_Scanline - nY;
		nYBit = ( nAttr & SPR_ATTR_V_FLIP ) ? ( ppuinfo.PPU_SP_Height - nYBit - 1 ) << 3 : nYBit << 3;

		if ( ppuinfo.PPU_R0 & R0_SP_SIZE )
		{
			// Sprite size 8x16
			if ( pSPRRAM[ SPR_CHR ] & 1 )
			{
				pbyChrData = ChrBuf + 256 * 64 + ( ( pSPRRAM[ SPR_CHR ] & 0xfe ) << 6 ) + nYBit;
			}
			else
			{
				pbyChrData = ChrBuf + ( ( pSPRRAM[ SPR_CHR ] & 0xfe ) << 6 ) + nYBit;
			}
		}
		else
		{
			// Sprite size 8x8
			pbyChrData = ppuinfo.PPU_SP_Base + ( pSPRRAM[ SPR_CHR ] << 6 ) + nYBit;
		}

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


	if (nSprCnt)
	{
		//Copy the data into the scanline buffer
		for (index = 0; index < 256; index++)
		{
//			nSprData = pSprBuf[ nX + index ];
			if (pSprBuf[index] && ((pSprBuf[index] & 0x80) || pPoint[index] & 0x8000) )
				pPoint[index] = PalTable[ (pSprBuf[index] & 0xf) + 0x10 ];
		}
	}
	//Make Sure there's only 8 Sprites on a line
	if ( nSprCnt == 8 )
		PPU_R2 |= R2_MAX_SP;  // Set a flag of maximum sprites on scanline
  }
  pNesX_Texture_Write(texture_address, pPoint);
}