/*===================================================================*/
/*                                                                   */
/*  pNesX_Sound_APU.h : APU functions for NES sound emulation        */
/*                                                                   */
/*  2001/12/22  ReGex   FrNES 0.60 Source Release                    */
/*                      Based on Matthew Conte's Nofrendo sound core */
/*                                                                   */
/*===================================================================*/

#include "pNesX_Sound_APU.h"
#include "pNesX_Sound_DC.h"

/* look up table madness */
// Changed memory locations to fit within aica memory
int32 decay_lut[16]; 
int32 vbl_lut[32];
int trilength_lut[128];
apu_t apu;

Timestamp_Tag queue[2048];
int num_queues;
int pNesX_Scanline_Since_Audio;

extern int8 noise_short_lut[];

/* ratios of pos/neg pulse for rectangle waves */
static const int duty_flip[4] = { 2, 4, 8, 12 };

/* frequency limit of rectangle channels */
static const int freq_limit[8] =
{
   0x3FF, 0x555, 0x666, 0x71C, 0x787, 0x7C1, 0x7E0, 0x7F0
};

/* noise frequency lookup table */
static const int noise_freq[16] =
{
     4,    8,   16,   32,   64,   96,  128,  160,
   202,  254,  380,  508,  762, 1016, 2034, 4068
};

/* DMC transfer freqs */
const int dmc_clocks[16] =
{
   428, 380, 340, 320, 286, 254, 226, 214,
   190, 160, 142, 128, 106,  85,  72,  54
};

void apu_process(void* buffer, int num_samples)
{
	register int32 accum1;
//	int32 accum2;
//	uint32* buf32;
	int16* buf16;
	int16 prev_sample = 0;
	int16 next_sample;

	int count;

	//next write is the sample on which the next write will occur
	int16 next_write;
	int queue_index;

	if (0 != buffer)
	{
//		buf32 = (uint32*) buffer;
		buf16 = (int16*) buffer;

		//If there was a write during the last section
		if (num_queues)
		{
			queue_index = 0;
			num_queues--;
			next_write = num_samples - (int16) (num_samples * ((float) (queue[queue_index].sample_write) / 524.0f));
		}
		else
			next_write = -1;
		
		count = num_samples;

		while (count--)
		{
			accum1 = 0;

			if (apu.mix_enable & 0x01)
				accum1 += apu_rectangle_0();
			if (apu.mix_enable & 0x02)
				accum1 += apu_rectangle_1();
			if (apu.mix_enable & 0x04)
				accum1 += apu_triangle();
			if (apu.mix_enable & 0x08)
				accum1 += apu_noise();
			if (apu.mix_enable & 0x10)
				accum1 += apu_dmc();

//			if (apu_rectangle_0() + apu_rectangle_1() + apu_triangle() + apu_noise() + apu_dmc();

			next_sample = accum1;

			/* do clipping */
			accum1 += prev_sample;
			accum1 >>= 1;

			prev_sample = next_sample;

			if (accum1 > 0x7FFF)
				accum1 = 0x7FFF;
			else if (accum1 < -0x8000)
				accum1 = -0x8000;

			*buf16++ = accum1;

			while (next_write >= count)
			{
				apu_write(queue[queue_index].reg, queue[queue_index].bydata);
				num_queues--;

				if (num_queues >= 0)
				{
					queue_index++;
					next_write = num_samples - (int16) (num_samples * ((float) (queue[queue_index].sample_write) / 524.0f));
				}
				else
				{
					next_write = -1;
				}
			}
		}
	}
	num_queues = 0;
}

void apu_write(uint32 address, uint8 value)
{  
   int chan;

   switch (address)
   {
   /* rectangles */
   case APU_WRA0:
   case APU_WRB0:
      chan = (address & 4) >> 2;
      apu.rectangle[chan].regs[0] = value;
      apu.rectangle[chan].volume = value & 0x0F;
      apu.rectangle[chan].env_delay = decay_lut[value & 0x0F];
      apu.rectangle[chan].holdnote = (value & 0x20) ? 1 : 0;
      apu.rectangle[chan].fixed_envelope = (value & 0x10) ? 1 : 0;
      apu.rectangle[chan].duty_flip = duty_flip[value >> 6];
      break;

   case APU_WRA1:
   case APU_WRB1:
      chan = (address & 4) >> 2;
      apu.rectangle[chan].regs[1] = value;
      apu.rectangle[chan].sweep_on = (value & 0x80) ? 1 : 0;
      apu.rectangle[chan].sweep_shifts = value & 7;
      apu.rectangle[chan].sweep_delay = decay_lut[(value >> 4) & 7];
      apu.rectangle[chan].sweep_inc = (value & 0x08) ? 1 : 0;
      apu.rectangle[chan].freq_limit = freq_limit[value & 7];
      break;

   case APU_WRA2:
   case APU_WRB2:
      chan = (address & 4) >> 2;
      apu.rectangle[chan].regs[2] = value;
      apu.rectangle[chan].freq = (apu.rectangle[chan].freq & ~0xFF) | value;
      break;

   case APU_WRA3:
   case APU_WRB3:
      chan = (address & 4) >> 2;
      apu.rectangle[chan].regs[3] = value;
      apu.rectangle[chan].vbl_length = vbl_lut[value >> 3];
      apu.rectangle[chan].env_vol = 0;
      apu.rectangle[chan].freq = ((value & 7) << 8) | (apu.rectangle[chan].freq & 0xFF);
      apu.rectangle[chan].adder = 0;
      break;

   /* triangle */
   case APU_WRC0:
      apu.triangle.regs[0] = value;
      apu.triangle.holdnote = (value & 0x80) ? 1 : 0;

      if (0 == apu.triangle.counter_started && apu.triangle.vbl_length)
         apu.triangle.linear_length = trilength_lut[value & 0x7F];

      break;

   case APU_WRC2:
      apu.triangle.regs[1] = value;
      apu.triangle.freq = (((apu.triangle.regs[2] & 7) << 8) + value) + 1;
      break;

   case APU_WRC3:

      apu.triangle.regs[2] = value;
  
      /* this is somewhat of a hack.  there appears to be some latency on 
      ** the Real Thing between when trireg0 is written to and when the 
      ** linear length counter actually begins its countdown.  we want to 
      ** prevent the case where the program writes to the freq regs first, 
      ** then to reg 0, and the counter accidentally starts running because 
      ** of the sound queue's timestamp processing.
      **
      ** set latency to a couple hundred cycles -- should be plenty of time 
      ** for the 6502 code to do a couple of table dereferences and load up 
      ** the other triregs
      */
      apu.triangle.write_latency = (int) (228 / apu.cycle_rate);
      apu.triangle.freq = (((value & 7) << 8) + apu.triangle.regs[1]) + 1;
      apu.triangle.vbl_length = vbl_lut[value >> 3];
      apu.triangle.counter_started = 0;
      apu.triangle.linear_length = trilength_lut[apu.triangle.regs[0] & 0x7F];
      break;

   /* noise */
   case APU_WRD0:
      apu.noise.regs[0] = value;
      apu.noise.env_delay = decay_lut[value & 0x0F];
      apu.noise.holdnote = (value & 0x20) ? 1 : 0;
      apu.noise.fixed_envelope = (value & 0x10) ? 1 : 0;
      apu.noise.volume = value & 0x0F;
      break;

   case APU_WRD2:
      apu.noise.regs[1] = value;
      apu.noise.freq = noise_freq[value & 0x0F];
//	  apu.noise.xor_tap = (value & 0x80) ? 0x40 : 0x02;
      /* detect transition from long->short sample */
      if ((value & 0x80) && (0 == apu.noise.short_sample))
      {
         /* recalculate short noise buffer */
         shift_register15(noise_short_lut, APU_NOISE_93);
         apu.noise.cur_pos = 0;
      }
      apu.noise.short_sample = (value & 0x80) ? 1 : 0;
      break;

   case APU_WRD3:
      apu.noise.regs[2] = value;
      apu.noise.vbl_length = vbl_lut[value >> 3];
      apu.noise.env_vol = 0; /* reset envelope */
      break;

   /* DMC */
   case APU_WRE0:
      apu.dmc.regs[0] = value;
      apu.dmc.freq = dmc_clocks[value & 0x0F];
      apu.dmc.looping = (value & 0x40) ? 1 : 0;

      if (value & 0x80)
      {
         apu.dmc.irq_gen = 1;
      }
      else
      {
         apu.dmc.irq_gen = 0;
         apu.dmc.irq_occurred = 0;
      }
      break;

   case APU_WRE1: /* 7-bit DAC */
      /* add the _delta_ between written value and
      ** current output level of the volume reg
      */
      value &= 0x7F; /* bit 7 ignored */
      apu.dmc.output_vol += ((value - apu.dmc.regs[1]) << 8);
      apu.dmc.regs[1] = value;
      break;

   case APU_WRE2:
      apu.dmc.regs[2] = value;
      apu.dmc.cached_addr = 0xC000 + (uint16) (value << 6);
      break;

   case APU_WRE3:
      apu.dmc.regs[3] = value;
      apu.dmc.cached_dmalength = ((value << 4) + 1) << 3;
      break;

   case APU_SMASK:
      /* bodge for timestamp queue */
      apu.dmc.enabled = (value & 0x10) ? 1 : 0;
      apu.enable_reg = value;

      for (chan = 0; chan < 2; chan++)
      {
         if (value & (1 << chan))
         {
            apu.rectangle[chan].enabled = 1;
         }
         else
         {
            apu.rectangle[chan].enabled = 0;
            apu.rectangle[chan].vbl_length = 0;
         }
      }

      if (value & 0x04)
      {
         apu.triangle.enabled = 1;
      }
      else
      {
         apu.triangle.enabled = 0;
         apu.triangle.vbl_length = 0;
         apu.triangle.linear_length = 0;
         apu.triangle.counter_started = 0;
         apu.triangle.write_latency = 0;
      }

      if (value & 0x08)
      {
         apu.noise.enabled = 1;
      }
      else
      {
         apu.noise.enabled = 0;
         apu.noise.vbl_length = 0;
      }

      if (value & 0x10)
      {
         if (0 == apu.dmc.dma_length)
            apu_dmcreload();
      }
      else
      {
         apu.dmc.dma_length = 0;
      }

      apu.dmc.irq_occurred = 0;
      break;

      /* unused, but they get hit in some mem-clear loops */
   case 0x4009:
   case 0x400D:
      break;
   
   default:
      break;
   }
}

/* Read from $4000-$4017 */
uint8 apu_read(uint32 address)
{
   uint8 value;

   switch (address)
   {
   case APU_SMASK:
      value = 0;
      /* Return 1 in 0-5 bit pos if a channel is playing */
      if (apu.rectangle[0].enabled && apu.rectangle[0].vbl_length)
         value |= 0x01;
      if (apu.rectangle[1].enabled && apu.rectangle[1].vbl_length)
         value |= 0x02;
      if (apu.triangle.enabled && apu.triangle.vbl_length)
         value |= 0x04;
      if (apu.noise.enabled && apu.noise.vbl_length)
         value |= 0x08;

      /* bodge for timestamp queue */
      if (apu.dmc.enabled)
         value |= 0x10;

      if (apu.dmc.irq_occurred)
         value |= 0x80;

      if (apu.irqclear_callback)
         value |= apu.irqclear_callback();

      break;

   default:
      value = (address >> 8); /* heavy capacitance on data bus */
      break;
   }

   return value;
}

void apu_queue(int16 reg, unsigned char bydata)
{
	queue[num_queues].bydata = bydata;
	queue[num_queues].reg = reg;
	queue[num_queues].sample_write = pNesX_Scanline_Since_Audio;
	num_queues++;
}


void apu_reset(void)
{
   uint32 address;

   /* initialize all channel members */
   for (address = 0x4000; address <= 0x4013; address++)
      apu_write(address, 0);

   apu_write(0x4015, 0);
}