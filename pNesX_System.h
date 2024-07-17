/*===================================================================*/
/*                                                                   */
/*  pNesX_System.h : The function which depends on a system          */
/*                                                                   */
/*  1999/11/03  Racoon  New preparation                              */
/*                                                                   */
/*===================================================================*/

#ifndef PNESX_SYSTEM_H_INCLUDED
#define PNESX_SYSTEM_H_INCLUDED

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/

#include <kos.h>
/*-------------------------------------------------------------------*/
/*  Palette data                                                     */
/*-------------------------------------------------------------------*/
extern uint16 NesPalette[];

/*-------------------------------------------------------------------*/
/*  Function prototypes                                              */
/*-------------------------------------------------------------------*/

/* Menu screen */
int pNesX_Menu();

/* Read ROM image file */
int pNesX_ReadRom( const char *filepath, uint32 filesize );

/* Release a memory for ROM */
void pNesX_ReleaseRom();

/* Transfer the contents of work frame on the screen */
void pNesX_LoadFrame();

/* Get a joypad state */
void pNesX_PadState( uint32 *pdwPad1, uint32 *pdwPad2, uint32 *ExitCount );

/* memcpy with wrapping offset */
uint32* pNesX_MemoryCopy_Offset( uint32* dest, uint32* src, int count, uint32 offset);

#endif /* !PNESX_SYSTEM_H_INCLUDED */