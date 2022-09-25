/*===================================================================*/
/*                                                                   */
/*  pNesX_Sound_APU.c : APU functions for NES sound emulation        */
/*                                                                   */
/*  2001/12/22  ReGex   FrNES 0.60 Source Release                    */
/*                      Based on Matthew Conte's Nofrendo sound core */
/*                                                                   */
/*===================================================================*/

#include "pNesX_Sound_APU.h"
#include "macros.h"

int audio_initialize() {
   apu = apu_create(APU_SAMPLE_RATE, CYCLE_RATE, 0, APU_SAMPLE_BITS);
   apu_set_exsound(NES_APU_EXSOUND_NONE);
   if (!apu) return 0;
   audio_reset();
   return 1;
}

void audio_reset() {
   apu_reset();
}

void audio_shutdown() {
   if (apu != NULL) {
      apu_destroy(&apu);
      apu = NULL;
   }
}

uint8 audio_read(uint32 addr) {
   return apu_read(addr);
}

void audio_write(uint32 addr, uint8 value) {
   apu_write(addr, value);
   apu_write_cur(addr, value);
}

void audio_do_frame(uint16* sample_buffer, uint32 num_samples) {
   apu_process(sample_buffer, num_samples);
}

int audio_sync_dmc_registers(uint32 cycles) {
   return sync_dmc_register(cycles);
}

void audio_sync_apu_registers() {
   sync_apu_register();
}