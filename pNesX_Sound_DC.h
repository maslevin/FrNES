/*===================================================================*/
/*                                                                   */
/*  pNesX_Sound_DC.h :  APU functions for NES sound emulation        */
/*                                                                   */
/*  2001/12/22  ReGex   FrNES 0.60 Source Release                    */
/*                      Based on Matthew Conte's Nofrendo sound core */
/*                                                                   */
/*===================================================================*/

#ifndef _PNESX_SOUND_DC_H_
#define _PNESX_SOUND_DC_H_

#include "pNesX.h"

#define  APU_WRA0       0x4000
#define  APU_WRA1       0x4001
#define  APU_WRA2       0x4002
#define  APU_WRA3       0x4003
#define  APU_WRB0       0x4004
#define  APU_WRB1       0x4005
#define  APU_WRB2       0x4006
#define  APU_WRB3       0x4007
#define  APU_WRC0       0x4008
#define  APU_WRC2       0x400A
#define  APU_WRC3       0x400B
#define  APU_WRD0       0x400C
#define  APU_WRD2       0x400E
#define  APU_WRD3       0x400F
#define  APU_WRE0       0x4010
#define  APU_WRE1       0x4011
#define  APU_WRE2       0x4012
#define  APU_WRE3       0x4013

#define  APU_SMASK      0x4015

/* length of generated noise */
#define  APU_NOISE_32K  0x7FFF
#define  APU_NOISE_93   93

#define  APU_BASEFREQ   1789772.7272727272727272

/* channel structures */
/* As much data as possible is precalculated,
** to keep the sample processing as lean as possible
*/
 
typedef struct rectangle_s
{
   uint8 regs[4];

   int enabled;
   
   float accum;
   int32 freq;
   int32 output_vol;
   int fixed_envelope;
   int holdnote;
   uint8 volume;

   int32 sweep_phase;
   int32 sweep_delay;
   int sweep_on;
   uint8 sweep_shifts;
   uint8 sweep_length;
   int sweep_inc;

   /* this may not be necessary in the future */
   int32 freq_limit;
   int32 env_phase;
   int32 env_delay;
   uint8 env_vol;

   int vbl_length;
   uint8 adder;
   int duty_flip;
} rectangle_t;

typedef struct triangle_s
{
   uint8 regs[3];

   int enabled;

   float accum;
   int32 freq;
   int32 output_vol;

   uint8 adder;

   int holdnote;
   int counter_started;
   /* quasi-hack */
   int write_latency;

   int vbl_length;
   int linear_length;
} triangle_t;


typedef struct noise_s
{
   uint8 regs[3];

   int enabled;

   float accum;
   int32 freq;
   int32 output_vol;

   int32 env_phase;
   int32 env_delay;
   uint8 env_vol;
   int fixed_envelope;
   int holdnote;

   uint8 volume;

   int vbl_length;
   int short_sample;
   int cur_pos;
//   uint8 xor_tap;
} noise_t;

typedef struct dmc_s
{
   uint8 regs[4];

   /* bodge for timestamp queue */
   int enabled;
   
   float accum;
   int32 freq;
   int32 output_vol;

   uint32 address;
   uint32 cached_addr;
   int dma_length;
   int cached_dmalength;
   uint8 cur_byte;

   int looping;
   int irq_gen;
   int irq_occurred;

} dmc_t;

enum
{
   APU_FILTER_NONE,
   APU_FILTER_LOWPASS,
   APU_FILTER_WEIGHTED
};

typedef struct
{
   uint32 min_range, max_range;
   uint8 (*read_func)(uint32 address);
} apu_memread;

typedef struct
{
   uint32 min_range, max_range;
   void (*write_func)(uint32 address, uint8 value);
} apu_memwrite;

typedef struct apu_s
{
   rectangle_t rectangle[2];
   triangle_t triangle;
   noise_t noise;
   dmc_t dmc;
   uint8 enable_reg;

   void *buffer; /* pointer to output buffer */
   int num_samples;

   uint8 mix_enable;
   int filter_type;

   double base_freq;
   float cycle_rate;

   int sample_rate;
   int sample_bits;
   int refresh_rate;

   void (*process)(void *buffer, int num_samples);
   void (*irq_callback)(void);
   uint8 (*irqclear_callback)(void);
} apu_t;

typedef struct
{
	unsigned char bydata;
	uint16	reg;
	uint16  sample_write;
} Timestamp_Tag;

/* Function prototypes */
extern void apu_process(void* buffer, int num_samples);
extern void apu_init();
extern void apu_build_luts();
extern uint8 apu_read(uint32 address);
extern void apu_write(uint32 address, uint8 value);
extern void apu_reset();
extern void apu_queue(int16 reg, unsigned char bydata);

extern unsigned char APU_Reg[];

#endif /* _PNESX_SOUND_DC_H_ */
