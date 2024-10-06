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

uint8* scanline_buffer = (uint8*)(void*)0x7dfff000;
uint8* sprite_buffer = (uint8*)(void*)0x7e000f00;

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
	void* texture_address;

	//texture_address is the Texture the frame currently being rendered will be displayed in
	texture_address = &(WorkFrame -> texture[ppuinfo.PPU_Scanline * 256]);

	if ( !(PPU_R1 & 0x18) ) {
		memset4(scanline_buffer, PPURAM[0x3F00] << 24 | PPURAM[0x3F00] << 16 | PPURAM[0x3F00] << 8 | PPURAM[0x3F00], 256);
	} else {
		if (MapperNo == 9) {
			pNesX_Map9DrawLine_BG_C(scanline_buffer);
			pNesX_Map9DrawLine_Spr_C(scanline_buffer, sprite_buffer);
		} else {
			pNesX_DrawLine_BG_C(scanline_buffer);
			pNesX_DrawLine_Spr_C(scanline_buffer, sprite_buffer);
		}
	}

	//Move the scanline buffer to the PVR texture
	pvr_txr_load(scanline_buffer, texture_address, 256);
}
