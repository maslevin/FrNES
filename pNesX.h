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

#include <kos.h>

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

/* The main loop of pNesX */ 
void pNesX_Main();

/* The loop of emulation */
void pNesX_Cycle();

/* A function in H-Sync */
int pNesX_HSync();
void pNesX_VSync();

void pNesX_DoSpu();

void new_frame();

#endif /* !PNESX_H_INCLUDED */

