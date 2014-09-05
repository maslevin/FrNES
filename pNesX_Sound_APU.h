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

#include "pNesX_Sound_DC.h"

#define  APU_NOISE_32K  0x7FFF
#define  APU_NOISE_93   93

extern apu_t apu;
extern int32 decay_lut[];
extern int32 vbl_lut[];
extern int trilength_lut[128];

extern void apu_build_luts(int num_samples);
extern int32 apu_rectangle_0(void);
extern int32 apu_rectangle_1(void);
extern int32 apu_triangle(void);
extern int32 apu_noise(void);
extern void apu_dmcreload(void);
extern int32 apu_dmc(void);
extern void apu_init();
void shift_register15(int8* buf, int count);

#endif