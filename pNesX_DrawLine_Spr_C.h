/*===================================================================*/
/*                                                                   */
/*  pNesX_DrawLine_Spr_C.h : Interface for C Sprite Renderer         */
/*                                                                   */
/*  2001/12/22  ReGex   FrNES 0.60 Source Release                    */
/*                                                                   */
/*===================================================================*/

#ifndef PNESX_DRAWLINE_SPR_C
#define PNESX_DRAWLINE_SPR_C

#include <kos.h>

void pNesX_DrawLine_Spr_C(volatile unsigned char* scanline_buffer, volatile unsigned char* sprite_buffer);

#endif