/*===================================================================*/
/*                                                                   */
/*  pNesX_PPU_DC.c          : PPU Renderer Function                  */
/*                                                                   */
/*  2001/12/22  ReGex   FrNES 0.60 Source Release                    */
/*                      Slightly reorganized from pNesX version      */
/*                                                                   */
/*===================================================================*/

#include <kos.h>
#include "macros.h"

#include "pNesX.h"
#include "pNesX_PPU_DC.h"
#include "pNesX_System_DC.h"
#include "pNesX_DrawLine_BG_C_Map9.h"
#include "pNesX_DrawLine_BG_C.h"
//#include "pNesX_DrawLine_BG_Asm.h"
#include "pNesX_DrawLine_Spr_C.h"
#include "pNesX_Utils.h"

extern VQ_Texture* WorkFrame;
extern unsigned char* Scanline_Buffer;
extern PPU_Info ppuinfo;

unsigned char pSprBuf[264];

unsigned char* Scanline_Buffer = NULL;
extern int SpriteJustHit;
//extern uint16 PPU_Addr;
extern uint16 PPU_Temp;

void pNesX_StartFrame()
{
	if ((PPU_R1 & 0x10) || (PPU_R1 & 0x08))
		ppuinfo.PPU_Addr = PPU_Temp;
}

/*===================================================================*/
/*                                                                   */
/*              pNesX_NewDrawLine() : Render a scanline              */
/*                                                                   */
/*===================================================================*/
void pNesX_DrawLine()
{
	void* texture_address;
	unsigned char* pPoint;
	int nSprCnt;
	int index;

	//texture_address is the Texture the frame currently being rendered will be displayed in
	texture_address = &(WorkFrame -> texture[ppuinfo.PPU_Scanline * 256]);
	
	if (Scanline_Buffer == NULL) {
		Scanline_Buffer = memalign(32, 256);
	}
	pPoint = Scanline_Buffer;

	if ( !( PPU_R1 & R1_SHOW_SCR ) )
	{
		memset(Scanline_Buffer, 0, 256);
		pvr_txr_load(Scanline_Buffer, texture_address, 256);
		return;
	}


	if (MapperNo == 9)
	{
		if (FrameCnt == 0)
		{
			pNesX_Map9DrawLine_BG_C(pPoint);
			nSprCnt = pNesX_Map9DrawLine_Spr_C(pSprBuf);
		}
		else
		{
			pNesX_Map9Simulate_BG_C();
			nSprCnt = pNesX_Map9Simulate_Spr_C();
		}
	}
	else
	{
		pNesX_DrawLine_BG_C(pPoint);
		nSprCnt = pNesX_DrawLine_Spr_C();

		/* // ASM VERSION
		pNesX_DrawLine_BG(&ppuinfo, PPUBANK, Scanline_Buffer, PalTable);
		nSprCnt = pNesX_DrawLine_Spr(&ppuinfo, SPRRAM, ChrBuf, pSprBuf);
		*/
	}

	if (nSprCnt)
	{
		//Copy the data into the scanline buffer
		for (index = 0; index < 256; index++)
		{
			if (pSprBuf[index])
				pPoint[index] = (pSprBuf[index] & 0xf) + 0x10;
		}
	}

	//Make Sure there's only 8 Sprites on a line
	if ( nSprCnt == 8 )
		PPU_R2 |= R2_MAX_SP;  // Set a flag of maximum sprites on scanline

	pvr_txr_load(pPoint, texture_address, 256);
}
