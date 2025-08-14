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
#include "pNesX_DrawLine_Spr_C.h"
#include "profile.h"
#include "Mapper.h"

#define __ALIGN32__		__attribute__ ((aligned (32)))
__ALIGN32__ unsigned char Scanline_Buffer[256];

extern uint16 PPU_Temp;

__attribute__ ((hot)) void pNesX_StartFrame() {
	if ((PPU_R1 & 0x10) || (PPU_R1 & 0x08))
		ppuinfo.PPU_Addr = PPU_Temp;
}

/*===================================================================*/
/*                                                                   */
/*              pNesX_NewDrawLine() : Render a scanline              */
/*                                                                   */
/*===================================================================*/
__attribute__ ((hot)) void pNesX_DrawLine() {
	//texture_address is the Texture the frame currently being rendered will be displayed in
	uintptr_t texture_address = (((uintptr_t)&WorkFrame -> texture & 0xffffff) | PVR_TA_TEX_MEM) + (ppuinfo.PPU_Scanline << 8);	

	if ( !( PPU_R1 & R1_SHOW_SCR ) ) {
		sq_set(texture_address, PPURAM[0x3F00], 256);
//		memset4(Scanline_Buffer, (PPURAM[0x3F00] << 24) | (PPURAM[0x3F00] << 16) | (PPURAM[0x3F00] << 8) | PPURAM[0x3F00], 256);
	} else {
		if (MapperNo == 9) {
			pNesX_Map9DrawLine_BG_C(Scanline_Buffer);
			pNesX_Map9DrawLine_Spr_C(Scanline_Buffer);
		} else {
			pNesX_DrawLine_BG_C(Scanline_Buffer);
			pNesX_DrawLine_Spr_C(Scanline_Buffer);
		}
		sq_cpy(texture_address, Scanline_Buffer, 256);
	}

	//Move the scanline buffer to the PVR texture
	//sq_cpy(texture_address, Scanline_Buffer, 256);
//	pvr_txr_load(Scanline_Buffer, texture_address, 256);
}
