/*===================================================================*/
/*                                                                   */
/*  pNesX.c   : NES Emulator for PSX                                 */
/*                                                                   */
/*  1999/11/03  Racoon  New preparation                              */
/*  2001/12/22  ReGex   FrNES 0.60 Source Release                    */
/*                      Slightly reorganized from pNesX version      */
/*                                                                   */
/*===================================================================*/

#ifndef PNESX_H_INCLUDED
#define PNESX_H_INCLUDED

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/

//#include "pNesX_Types.h"
#include <kos.h>

/*-------------------------------------------------------------------*/
/*  NES resources                                                    */
/*-------------------------------------------------------------------*/

#define PPURAM_SIZE  0x4000
#define SPRRAM_SIZE  256

/*-------------------------------------------------------------------*/
/*  PPU resources                                                    */
/*-------------------------------------------------------------------*/
typedef struct __attribute__ ((packed, aligned(4))) PPU_Info_s {
	uint16 PPU_Addr;
	uint32 PPU_Scr_H_Bit;
	uint32 PPU_Scanline;
	uint32 PPU_SP_Height;
	uint32 PPU_R0;
} PPU_Info_t;

extern PPU_Info_t ppuinfo;

/* PPU RAM */
extern unsigned char PPURAM[];

/* PPU BANK ( 1Kb * 16 ) */
extern unsigned char *PPUBANK[];

#define FRAMEBUFFER_WIDTH 256
#define FRAMEBUFFER_HEIGHT 256

//Here we define the size of a framebuffer texture we want
#define FRAMEBUFFER_PIXELS (FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT)

//The codebook for a VQ texture is always 2048 bytes
#define CODEBOOK_SIZE 2048

typedef struct {
	unsigned char codebook[CODEBOOK_SIZE];
	unsigned char texture[FRAMEBUFFER_PIXELS];
} VQ_Texture;

/* Display Buffer */
#define NUM_PVR_FRAMES 3

extern VQ_Texture* WorkFrame;
extern uint16 WorkFrameIdx;
extern VQ_Texture* WorkFrames[NUM_PVR_FRAMES];
extern uint8 codebook[CODEBOOK_SIZE];

#define NAME_TABLE0  8
#define NAME_TABLE1  9
#define NAME_TABLE2  10
#define NAME_TABLE3  11

#define NAME_TABLE_V_MASK 2
#define NAME_TABLE_H_MASK 1

/* Sprite RAM */
extern unsigned char SPRRAM[];

#define SPR_Y    0
#define SPR_CHR  1
#define SPR_ATTR 2
#define SPR_X    3
#define SPR_ATTR_COLOR  0x3
#define SPR_ATTR_V_FLIP 0x80
#define SPR_ATTR_H_FLIP 0x40
#define SPR_ATTR_PRI    0x20

/* PPU Register */
extern unsigned char PPU_R1;
extern unsigned char PPU_R2;
extern unsigned char PPU_R3;
extern unsigned char PPU_R7;

extern unsigned char PPU_Scr_V;
extern unsigned char PPU_Scr_V_Next;
extern unsigned char PPU_Scr_V_Byte_Next;
extern unsigned char PPU_Scr_V_Bit_Next;

extern unsigned char PPU_Scr_H;
extern unsigned char PPU_Scr_H_Next;
extern unsigned char PPU_Scr_H_Byte_Next;
extern unsigned char PPU_Scr_H_Bit_Next;

extern unsigned char PPU_Latch_Flag;
extern uint16 PPU_Addr;
extern uint16 PPU_Increment;
extern uint16 PPU_Temp;
extern unsigned char PPU_2007_Buffer;

extern int SpriteJustHit;
#define SPRITE_HIT_SENTINEL 255

#define MIRRORING_HORIZONTAL 0
#define MIRRORING_VERTICAL 1
#define MIRRORING_FOUR_SCREEN 2
#define MIRRORING_SINGLE_SCREEN_LOW 3
#define MIRRORING_SINGLE_SCREEN_HIGH 4

#define R0_NMI_VB      0x80
#define R0_NMI_SP      0x40
#define R0_SP_SIZE     0x20
#define R0_BG_ADDR     0x10
#define R0_SP_ADDR     0x08
#define R0_INC_ADDR    0x04
#define R0_NAME_ADDR   0x03

#define R1_BACKCOLOR   0xe0
#define R1_SHOW_SP     0x10
#define R1_SHOW_SCR    0x08
#define R1_CLIP_SP     0x04
#define R1_CLIP_BG     0x02
#define R1_MONOCHROME  0x01

#define R2_IN_VBLANK   0x80
#define R2_HIT_SP      0x40
#define R2_MAX_SP      0x20
#define R2_WRITE_FLAG  0x10

/* NES display size */
#define NES_DISP_WIDTH      256
#define NES_DISP_HEIGHT     224

#ifdef DEBUG
#define MAX_EXIT_COUNT 1
#else
#define MAX_EXIT_COUNT 50
#endif

/*-------------------------------------------------------------------*/
/*  Display and Others resouces                                      */
/*-------------------------------------------------------------------*/
extern float frames_per_second;

/* Frame Skip */
extern uint16 FrameSkip;
extern uint16 FrameCnt;

/*-------------------------------------------------------------------*/
/*  APU and Pad resources                                            */
/*-------------------------------------------------------------------*/

extern unsigned char APU_Reg[];

extern uint32 PAD1_Latch;
extern uint32 PAD2_Latch;
extern uint32 ExitCount;
extern uint32 PAD1_Bit;
extern uint32 PAD2_Bit;

#define PAD_SYS_QUIT   1
#define PAD_SYS_OK     2
#define PAD_SYS_CANCEL 4
#define PAD_SYS_UP     8
#define PAD_SYS_DOWN   0x10
#define PAD_SYS_LEFT   0x20
#define PAD_SYS_RIGHT  0x40

#define PAD_PUSH(a,b)  ( ( (a) & (b) ) != 0 )

/*-------------------------------------------------------------------*/
/*  Function prototypes                                              */
/*-------------------------------------------------------------------*/

/* Initialize pNesX */
void pNesX_Init();

/* Completion treatment */
void pNesX_Fin();

/* Load a cassette */
int pNesX_Load( const char *filepath, uint32 filename );

/* Reset pNesX */
int pNesX_Reset();

/* Initialize PPU */
void pNesX_SetupPPU();

/* Set up a Mirroring of Name Table */
void pNesX_Mirroring( int nType );
void pNesX_Mirroring_Manual( int bank1, int bank2, int bank3, int bank4);

/* The main loop of pNesX */ 
void pNesX_Main();

/* The loop of emulation */
void pNesX_Cycle();

/* A function in H-Sync */
int pNesX_HSync();
void pNesX_VSync();

void pNesX_DoSpu();

#endif /* !PNESX_H_INCLUDED */

