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

#include "K6502_rw.h"

#define  APU_VOLUME_DECAY(x)  ((x) -= ((x) >> 7))

/* the following seem to be the correct (empirically determined)
** relative volumes between the sound channels
*/
#define  APU_RECTANGLE_OUTPUT(channel) (apu.rectangle[channel].output_vol)
#define  APU_TRIANGLE_OUTPUT           (apu.triangle.output_vol + (apu.triangle.output_vol >> 2))
#define  APU_NOISE_OUTPUT              ((apu.noise.output_vol + apu.noise.output_vol + apu.noise.output_vol) >> 2)
#define  APU_DMC_OUTPUT                ((apu.dmc.output_vol + apu.dmc.output_vol + apu.dmc.output_vol) >> 2)

int8 noise_long_lut[APU_NOISE_32K];
int8 noise_short_lut[APU_NOISE_93];

/* vblank length table used for rectangles, triangle, noise */
static const uint8 vbl_length[32] =
{
    5, 127,
   10,   1,
   19,   2,
   40,   3,
   80,   4,
   30,   5,
    7,   6,
   13,   7,
    6,   8,
   12,   9,
   24,  10,
   48,  11,
   96,  12,
   36,  13,
    8,  14,
   16,  15
};

/* emulation of the 15-bit shift register the
** NES uses to generate pseudo-random series
** for the white noise channel
*/
void shift_register15(int8* buf, int count)
{
   static int sreg = 0x4000;
   int bit0, bit1, bit6, bit14;

   if (count == APU_NOISE_93)
   {
	   while (count--)
	   {
		  bit0 = sreg & 1;
		  bit6 = (sreg & 0x40) >> 6;
		  bit14 = (bit0 ^ bit6);
		  sreg >>= 1;
		  sreg |= (bit14 << 14);
		  *buf++ = bit0 ^ 1;
	   }
   }
   else
   {
	   while (count--)
	   {
		  bit0 = sreg & 1;
		  bit1 = (sreg & 2) >> 1;
		  bit14 = (bit0 ^ bit1);
		  sreg >>= 1;
		  sreg |= (bit14 << 14);
		  *buf++ = bit0 ^ 1;
	   }
   }
}

void apu_build_luts(int num_samples)
{
   int i;

   /* lut used for enveloping and frequency sweeps */
   for (i = 0; i < 16; i++)
      decay_lut[i] = num_samples * (i + 1);

   /* used for note length, based on vblanks and size of audio buffer */
   for (i = 0; i < 32; i++)
      vbl_lut[i] = vbl_length[i] * num_samples;

   /* triangle wave channel's linear length table */
   for (i = 0; i < 128; i++)
      trilength_lut[i] = (int) (0.25 * (i * num_samples));

   /* generate noise samples */
   shift_register15((int8*)noise_long_lut, APU_NOISE_32K);
   shift_register15((int8*)noise_short_lut, APU_NOISE_93);
}

/* RECTANGLE WAVE
** ==============
** reg0: 0-3=volume, 4=envelope, 5=hold, 6-7=duty cycle
** reg1: 0-2=sweep shifts, 3=sweep inc/dec, 4-6=sweep length, 7=sweep on
** reg2: 8 bits of freq
** reg3: 0-2=high freq, 7-4=vbl length counter
*/
int32 apu_rectangle_0(void) 
{
   int32 output; 

   APU_VOLUME_DECAY(apu.rectangle[0].output_vol); 

   if ((0 == apu.rectangle[0].enabled) || (0 == apu.rectangle[0].vbl_length)) 
      return APU_RECTANGLE_OUTPUT(0); 

   /* vbl length counter */ 
   if (apu.rectangle[0].holdnote == 0)
      apu.rectangle[0].vbl_length--; 

   /* envelope decay at a rate of (env_delay + 1) / 240 secs */ 
   apu.rectangle[0].env_phase -= 4; /* 240/60 */ 
   while (apu.rectangle[0].env_phase < 0) 
   { 
      apu.rectangle[0].env_phase += apu.rectangle[0].env_delay; 

      if (apu.rectangle[0].holdnote) 
         apu.rectangle[0].env_vol = (apu.rectangle[0].env_vol + 1) & 0x0F; 
      else if (apu.rectangle[0].env_vol < 0x0F) 
         apu.rectangle[0].env_vol++; 
   } 

   /* TODO: find true relation of freq_limit to register values */ 
   if ((apu.rectangle[0].freq < 8) || ((0 == apu.rectangle[0].sweep_inc) && (apu.rectangle[0].freq > apu.rectangle[0].freq_limit)))
      return APU_RECTANGLE_OUTPUT(0); 

   /* frequency sweeping at a rate of (sweep_delay + 1) / 120 secs */ 
   if (apu.rectangle[0].sweep_on && apu.rectangle[0].sweep_shifts) 
   { 
      apu.rectangle[0].sweep_phase -= 2; /* 120/60 */ 
      while (apu.rectangle[0].sweep_phase < 0) 
      { 
         apu.rectangle[0].sweep_phase += apu.rectangle[0].sweep_delay; 

         if (apu.rectangle[0].sweep_inc) /* ramp up */ 
         { 
            apu.rectangle[0].freq += ~(apu.rectangle[0].freq >> apu.rectangle[0].sweep_shifts); 
         } 
         else /* ramp down */ 
         { 
            apu.rectangle[0].freq += (apu.rectangle[0].freq >> apu.rectangle[0].sweep_shifts); 
         } 
      } 
   } 

   apu.rectangle[0].accum -= apu.cycle_rate; 
   if (apu.rectangle[0].accum >= 0) 
      return APU_RECTANGLE_OUTPUT(0); 

   while (apu.rectangle[0].accum < 0)
   {
      apu.rectangle[0].accum += (apu.rectangle[0].freq + 1);
      apu.rectangle[0].adder = (apu.rectangle[0].adder + 1) & 0x0F;
   }

   if (apu.rectangle[0].fixed_envelope)
      output = apu.rectangle[0].volume << 8; /* fixed volume */
   else
      output = (apu.rectangle[0].env_vol ^ 0x0F) << 8;

   if (0 == apu.rectangle[0].adder)
      apu.rectangle[0].output_vol = output;
   else if (apu.rectangle[0].adder == apu.rectangle[0].duty_flip)
      apu.rectangle[0].output_vol = -output;

   return APU_RECTANGLE_OUTPUT(0);
}


int32 apu_rectangle_1(void) 
{
   int32 output; 

   APU_VOLUME_DECAY(apu.rectangle[1].output_vol); 

   if ((0 == apu.rectangle[1].enabled) || (0 == apu.rectangle[1].vbl_length)) 
      return APU_RECTANGLE_OUTPUT(1); 

   /* vbl length counter */ 
   if (apu.rectangle[1].holdnote == 0)
      apu.rectangle[1].vbl_length--; 

   /* envelope decay at a rate of (env_delay + 1) / 240 secs */ 
   apu.rectangle[1].env_phase -= 4; /* 240/60 */ 
   while (apu.rectangle[1].env_phase < 0) 
   { 
      apu.rectangle[1].env_phase += apu.rectangle[1].env_delay; 

      if (apu.rectangle[1].holdnote) 
         apu.rectangle[1].env_vol = (apu.rectangle[1].env_vol + 1) & 0x0F; 
      else if (apu.rectangle[1].env_vol < 0x0F) 
         apu.rectangle[1].env_vol++; 
   } 

   /* TODO: find true relation of freq_limit to register values */ 
   if ((apu.rectangle[1].freq < 8) || ((0 == apu.rectangle[1].sweep_inc) && (apu.rectangle[1].freq > apu.rectangle[1].freq_limit)))
      return APU_RECTANGLE_OUTPUT(1); 

   /* frequency sweeping at a rate of (sweep_delay + 1) / 120 secs */ 
   if (apu.rectangle[1].sweep_on && apu.rectangle[1].sweep_shifts) 
   { 
      apu.rectangle[1].sweep_phase -= 2; /* 120/60 */ 
      while (apu.rectangle[1].sweep_phase < 0) 
      { 
         apu.rectangle[1].sweep_phase += apu.rectangle[1].sweep_delay; 

         if (apu.rectangle[1].sweep_inc) /* ramp up */ 
         { 
            apu.rectangle[1].freq -= (apu.rectangle[1].freq >> apu.rectangle[1].sweep_shifts); 
         } 
         else /* ramp down */ 
         { 
            apu.rectangle[1].freq += (apu.rectangle[1].freq >> apu.rectangle[1].sweep_shifts); 
         } 
      } 
   } 

   apu.rectangle[1].accum -= apu.cycle_rate; 
   if (apu.rectangle[1].accum >= 0) 
      return APU_RECTANGLE_OUTPUT(1); 

   while (apu.rectangle[1].accum < 0)
   {
      apu.rectangle[1].accum += (apu.rectangle[1].freq + 1);
      apu.rectangle[1].adder = (apu.rectangle[1].adder + 1) & 0x0F;
   }

   if (apu.rectangle[1].fixed_envelope)
      output = apu.rectangle[1].volume << 8; /* fixed volume */
   else
      output = (apu.rectangle[1].env_vol ^ 0x0F) << 8;

   if (0 == apu.rectangle[1].adder)
      apu.rectangle[1].output_vol = output;
   else if (apu.rectangle[1].adder == apu.rectangle[1].duty_flip)
      apu.rectangle[1].output_vol = -output;

   return APU_RECTANGLE_OUTPUT(1);
}


/* TRIANGLE WAVE
** =============
** reg0: 7=holdnote, 6-0=linear length counter
** reg2: low 8 bits of frequency
** reg3: 7-3=length counter, 2-0=high 3 bits of frequency
*/
int32 apu_triangle(void)
{
   APU_VOLUME_DECAY(apu.triangle.output_vol);

   if (0 == apu.triangle.enabled || 0 == apu.triangle.vbl_length)
      return APU_TRIANGLE_OUTPUT;

   if (apu.triangle.counter_started)
   {
      if (apu.triangle.linear_length > 0)
         apu.triangle.linear_length--;
      if (apu.triangle.vbl_length && (0 == apu.triangle.holdnote))
         apu.triangle.vbl_length--;
   }
   else if (0 == apu.triangle.holdnote && apu.triangle.write_latency)
   {
      if (--apu.triangle.write_latency == 0)
         apu.triangle.counter_started = 1;
   }

   if (0 == apu.triangle.linear_length || apu.triangle.freq < 4) /* inaudible */
      return APU_TRIANGLE_OUTPUT;

   apu.triangle.accum -= apu.cycle_rate; 
   while (apu.triangle.accum < 0)
   {
      apu.triangle.accum += apu.triangle.freq;
      apu.triangle.adder = (apu.triangle.adder + 1) & 0x1F;

      if (apu.triangle.adder & 0x10)
         apu.triangle.output_vol -= (2 << 8);
      else
         apu.triangle.output_vol += (2 << 8);
   }

   return APU_TRIANGLE_OUTPUT;
}


/* WHITE NOISE CHANNEL
** ===================
** reg0: 0-3=volume, 4=envelope, 5=hold
** reg2: 7=small(93 byte) sample,3-0=freq lookup
** reg3: 7-4=vbl length counter
*/
/* TODO: AAAAAAAAAAAAAAAAAAAAAAAA!  #ifdef MADNESS! */
int32 apu_noise(void)
{
   int32 outvol;
   int32 noise_bit = 0;

   APU_VOLUME_DECAY(apu.noise.output_vol);

   if ((0 == apu.noise.enabled) || (0 == apu.noise.vbl_length))
      return APU_NOISE_OUTPUT;

   /* vbl length counter */
   if (0 == apu.noise.holdnote)
      apu.noise.vbl_length--;

   /* envelope decay at a rate of (env_delay + 1) / 240 secs */
   apu.noise.env_phase -= 4; /* 240/60 */
   while (apu.noise.env_phase < 0)
   {
      apu.noise.env_phase += apu.noise.env_delay;

      if (apu.noise.holdnote)
         apu.noise.env_vol = (apu.noise.env_vol + 1) & 0x0F;
      else if (apu.noise.env_vol < 0x0F)
         apu.noise.env_vol++;
   }

   apu.noise.accum -= apu.cycle_rate;
   if (apu.noise.accum >= 0)
      return APU_NOISE_OUTPUT;
   

   while (apu.noise.accum < 0)
   {
		apu.noise.accum += apu.noise.freq;
      apu.noise.cur_pos++;

      if (apu.noise.short_sample)
      {
         if (APU_NOISE_93 == apu.noise.cur_pos)
            apu.noise.cur_pos = 0;
      }
      else
      {
         if (APU_NOISE_32K == apu.noise.cur_pos)
            apu.noise.cur_pos = 0;
      }

/*
      apu.noise.accum += apu.noise.freq;

      noise_bit = shift_register15(apu.noise.xor_tap);*/
   }

   if (apu.noise.fixed_envelope)
      outvol = apu.noise.volume << 8; /* fixed volume */
   else
      outvol = (apu.noise.env_vol ^ 0x0F) << 8;

   if (apu.noise.short_sample)
      noise_bit = noise_short_lut[apu.noise.cur_pos];
   else
      noise_bit = noise_long_lut[apu.noise.cur_pos];

   if (noise_bit)
      apu.noise.output_vol = outvol;
   else
      apu.noise.output_vol = -outvol;

   return APU_NOISE_OUTPUT;
}


void apu_dmcreload(void)
{
   apu.dmc.address = apu.dmc.cached_addr;
   apu.dmc.dma_length = apu.dmc.cached_dmalength;
   apu.dmc.irq_occurred = 0;
}

/* DELTA MODULATION CHANNEL
** =========================
** reg0: 7=irq gen, 6=looping, 3-0=pointer to clock table
** reg1: output dc level, 6 bits unsigned
** reg2: 8 bits of 64-byte aligned address offset : $C000 + (value * 64)
** reg3: length, (value * 16) + 1
*/
int32 apu_dmc(void)
{
   int delta_bit;

   APU_VOLUME_DECAY(apu.dmc.output_vol);

   /* only process when channel is alive */
   if (apu.dmc.dma_length)
   {
      apu.dmc.accum -= apu.cycle_rate;
      
      while (apu.dmc.accum < 0)
      {
         apu.dmc.accum += apu.dmc.freq;
         
         delta_bit = (apu.dmc.dma_length & 7) ^ 7;
         
         if (7 == delta_bit)
         {
			apu.dmc.cur_byte = K6502_Read(apu.dmc.address);
//          apu.dmc.cur_byte = nes6502_getbyte(apu.dmc.address);
            
            /* steal a cycle from CPU*/
//          nes6502_burn(1);

            /* prevent wraparound */
            if (0xFFFF == apu.dmc.address)
               apu.dmc.address = 0x8000;
            else
               apu.dmc.address++;
         }

         if (--apu.dmc.dma_length == 0)
         {
            /* if loop bit set, we're cool to retrigger sample */
            if (apu.dmc.looping)
            {
               apu_dmcreload();
            }
            else
            {
               /* check to see if we should generate an irq */
               if (apu.dmc.irq_gen)
               {
                  apu.dmc.irq_occurred = 1;
                  if (apu.irq_callback)
                     apu.irq_callback();
               }

               /* bodge for timestamp queue */
               apu.dmc.enabled = 0;
               break;
            }
         }

         /* positive delta */
         if (apu.dmc.cur_byte & (1 << delta_bit))
         {
            if (apu.dmc.regs[1] < 0x7D)
            {
               apu.dmc.regs[1] += 2;
               apu.dmc.output_vol += (2 << 8);
            }
         }
         /* negative delta */
         else            
         {
            if (apu.dmc.regs[1] > 1)
            {
               apu.dmc.regs[1] -= 2;
               apu.dmc.output_vol -= (2 << 8);
            }
         }
      }
   }

   return APU_DMC_OUTPUT;
}

void apu_init()
{
	apu.sample_rate = 22050;
	apu.refresh_rate = 60;
	apu.sample_bits = 16;
	apu.num_samples = 736;
	apu.base_freq = APU_BASEFREQ;
	//Try throwing the DC's sound frequency in here
	apu.cycle_rate = (float) (apu.base_freq / (float) apu.sample_rate);
	apu.mix_enable = 0xFF;

	apu_build_luts(apu.num_samples);
	apu_reset();
}
