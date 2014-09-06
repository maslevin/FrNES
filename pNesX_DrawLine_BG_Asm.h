/*===================================================================*/
/*                                                                   */
/*  pNesX_DrawLine_BG_Asm.h : Interface For Assembly Language        */
/*                            Background and Sprite Renderers        */
/*                                                                   */
/*  2001/12/22  ReGex   FrNES 0.60 Source Release                    */
/*                      Slightly reorganized from pNesX version      */
/*                                                                   */
/*===================================================================*/

#ifndef PNESX_DRAWLINE_BG_ASM
#define PNESX_DRAWLINE_BG_ASM

#include <kos.h>

void pNesX_DrawLine_BG(PPU_Info* ppuinfo, void* PPUBANK, void* RenderBG, void* Palette);
unsigned char pNesX_DrawLine_Spr(PPU_Info* ppuinfo, void* SPRRAM, void* CHARRAM, void* RenderSPR);

#endif