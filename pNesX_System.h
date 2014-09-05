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
int pNesX_ReadRom( const char *pszFileName );

/* Release a memory for ROM */
void pNesX_ReleaseRom();

/* Transfer the contents of work frame on the screen */
void pNesX_LoadFrame();

/* Get a joypad state */
void pNesX_PadState( uint32 *pdwPad1, uint32 *pdwPad2, uint32 *ExitCount );

/* memcpy */
uint32 *pNesX_MemoryCopy( uint32 *dest, uint32 *src, int count );

/* memset */
void *pNesX_MemorySet( void *dest, int c, int count );

/* Print debug message */
void pNesX_DebugPrint( char *pszMsg );

#endif /* !PNESX_SYSTEM_H_INCLUDED */