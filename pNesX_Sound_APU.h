/*===================================================================*/
/*                                                                   */
/*  pNesX_Sound_APU.h : APU functions for NES sound emulation        */
/*                                                                   */
/*  2001/12/22  ReGex   FrNES 0.60 Source Release                    */
/*                      Slightly reorganized from pNesX version      */
/*                                                                   */
/*===================================================================*/

#ifndef PNESX_SOUND_DC
#define PNESX_SOUND_DC

#include "nes_apu.h"

#define APU_SAMPLE_RATE 22050
#define CYCLE_RATE 60
#define APU_SAMPLE_BITS 16

extern int audio_initialize();
extern void audio_reset();
extern void audio_shutdown();
extern uint8 audio_read(uint32 addr);
extern void audio_write(uint32 addr, uint8 value);
extern void audio_do_frame(uint16* sample_buffer, uint32 num_samples);

#endif