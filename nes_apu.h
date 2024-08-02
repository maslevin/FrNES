/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of version 2 of the GNU Library General 
** Public License as published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
** Library General Public License for more details.  To obtain a 
** copy of the GNU Library General Public License, write to the Free 
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
**
**
** nes_apu.h
**
** NES APU emulation header file
** nes_apu.h,v 1.5 2002/03/24 15:32:03 tekezo Exp
*/

#ifndef _NES_APU_H_
#define _NES_APU_H_

#include <kos.h>

/* define this for realtime generated noise */
#define  REALTIME_NOISE

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

/*#define  APU_BASEFREQ   1789772.7272727272727272 */
#define  APU_BASEFREQ   1789772.5

/* to/from 16.16 fixed point */
#define  APU_TO_FIXED(x)    ((x) << 16)
#define  APU_FROM_FIXED(x)  ((x) >> 16)

#define boolean int
#define true 1
#define false 0

/* ============================================================ */
/* APU Sound struct */

/* channel structures */
/* As much data as possible is precalculated,
** to keep the sample processing as lean as possible
*/
 
typedef struct rectangle_s {
    uint8 regs[4];

    boolean enabled;

    int32 phaseacc;
    int32 freq;
    int32 output_vol;
    boolean fixed_envelope;
    boolean holdnote;
    uint8 volume;

    int32 sweep_phase;
    int32 sweep_delay;
    boolean sweep_on;
    uint8 sweep_shifts;
    uint8 sweep_length;
    boolean sweep_inc;

    /* this may not be necessary in the future */
    int32 freq_limit;

    /* rectangle 0 uses a complement addition for sweep
    ** increases, while rectangle 1 uses subtraction
    */
    boolean sweep_complement;

    int32 env_phase;
    int32 env_delay;
    uint8 env_vol;

    int vbl_length;
    uint8 adder;
    int duty_flip;

    /* for sync read $4105 */
    boolean enabled_cur;
    boolean holdnote_cur;
    int vbl_length_cur;
} rectangle_t;

typedef struct triangle_s {
    uint8 regs[3];

    boolean enabled;

    int32 freq;
    int32 phaseacc;
    int32 output_vol;

    uint8 adder;

    boolean holdnote;
    boolean counter_started;
    /* quasi-hack */
    int write_latency;

    int vbl_length;
    int linear_length;

    /* for sync read $4105 */
    boolean enabled_cur;
    boolean holdnote_cur;
    boolean counter_started_cur;
    int vbl_length_cur;

#ifdef APU_YANO
    /* less compatibility, clearer sound if enabled */
    boolean ideal_triangle;
#endif
} triangle_t;

typedef struct noise_s {
    uint8 regs[3];

    boolean enabled;

    int32 freq;
    int32 phaseacc;
    int32 output_vol;

    int32 env_phase;
    int32 env_delay;
    uint8 env_vol;
    boolean fixed_envelope;
    boolean holdnote;

    uint8 volume;

    int vbl_length;

#ifdef REALTIME_NOISE
    uint8 xor_tap;
#else
    boolean short_sample;
    int cur_pos;
#endif /* REALTIME_NOISE */

    /* for sync read $4105 */
    boolean enabled_cur;
    boolean holdnote_cur;
    int vbl_length_cur;
} noise_t;

typedef struct dmc_s {
    uint8 regs[4];

    /* bodge for timestamp queue */
    boolean enabled;
    
    int32 freq;
    int32 phaseacc;
    int32 output_vol;

    uint32 address;
    uint32 cached_addr;
    int dma_length;
    int cached_dmalength;
    uint8 cur_byte;

    boolean looping;
    boolean irq_gen;
    boolean irq_occurred;

    /* for sync read $4105 and DPCM IRQ */
    int32 freq_cur;
    int32 phaseacc_cur;
    int dma_length_cur;
    int cached_dmalength_cur;
    boolean enabled_cur;
    boolean looping_cur;
    boolean irq_gen_cur;
    boolean irq_occurred_cur;
} dmc_t;

typedef struct apusound_s {
    rectangle_t rectangle[2];
    triangle_t triangle;
    noise_t noise;
    dmc_t dmc;
} APUSOUND;

/* ============================================================ */
/* ex-sound */
typedef enum {
    NES_APU_EXSOUND_NONE,
    NES_APU_EXSOUND_VRC6,
    NES_APU_EXSOUND_VRC7,
    NES_APU_EXSOUND_FDS,
    NES_APU_EXSOUND_MMC5,
    NES_APU_EXSOUND_N106,
    NES_APU_EXSOUND_FME7,
} nes_apu_exsound_type_t;


typedef struct {
    int32 (*render_func)();
    void (*write_func)(uint32 address, uint8 value);
} nes_apu_exsound_t;

/* VRC6 Sound struct */

typedef struct {
    uint32 cps;
    int32 cycles;
    uint32 spd;
    uint8 regs[3];
    uint8 update;
    uint8 adr;
} VRC6_SQUARE;

typedef struct {
    uint32 cps;
    int32 cycles;
    uint32 spd;
    uint32 output;
    uint8 regs[3];
    uint8 update;
    uint8 adr;
} VRC6_SAW;

typedef struct {
    VRC6_SQUARE square[2];
    VRC6_SAW saw;
    uint32 mastervolume;
} VRC6SOUND;

/* ------------------------------------------------------------ */
/* VRC7 Sound struct */

typedef struct {
    uint32	pg_phase;
    uint32	pg_spd;
    int32	vib_cycles;
    uint32	input;
    uint32	eg_phase;
    uint32	eg_sl;
    uint32	eg_arr;
    uint32	eg_drr;
    uint32	eg_rrr;
    uint8	pg_vib;
    uint32	*sintblp;
    uint8	tl;
    uint8	eg_mode;
    uint8	eg_type;
    uint8	su_type;
    uint8	eg_ar;
    uint8	eg_dr;
    uint8	eg_rr;
    uint8	eg_ks;
    uint8	eg_am;
} OPLL_OP;

typedef struct {
    uint32	cps;
    uint32	spd;
    int32	cycles;
    uint32	adr;
    uint32	adrmask;
    uint32	*table;
    uint32	output;
} OPLL_LFO;

typedef struct {
    uint32 cps;
    int32 cycles;
    uint32 fbbuf[2];
    uint32 output;
    OPLL_OP op[2];
    uint32 mastervolume;
    uint8 tone[8];
    uint8 key;
    uint8 toneno;
    uint8 freql;
    uint8 freqh;
    uint8 fb;
    uint8 update;
} OPLL_CH;

typedef struct {
    OPLL_CH ch[6];
    OPLL_LFO lfo[2];
    uint32 mastervolume;
    uint8 usertone[8];
    uint8 adr;
    uint8 rhythmc;
    uint8 toneupdate;
} OPLLSOUND;

/* ------------------------------------------------------------ */
/* FDS Sound struct */

typedef struct {
    uint32 wave[0x40];
    uint32 envspd;
    int32 envphase;
    uint32 envout;
    uint32 outlvl;

    uint32 phase;
    uint32 spd;
    uint32 volume;
    int32 sweep;

    uint8 enable;
    uint8 envmode;

    int32 timer;
    uint32 last_spd;
} FDS_FMOP;

typedef struct FDSSOUND {
    uint32 cps;
    int32 cycles;
    uint32 mastervolume;
    int32 output;
    int32 fade;

    FDS_FMOP op[2];

    uint32 waveaddr;
    uint8 mute;
    uint8 reg[0x10];
    uint8 reg_cur[0x10];
} FDSSOUND;

/* ------------------------------------------------------------ */
/* MMC5 Sound struct */

typedef struct {
    uint32 cps;
    int32 cycles;
    int32 sweepphase;
    int32 envphase;

    uint32 spd;
    uint32 envspd;
    uint32 sweepspd;

    uint32 length;
    uint32 freq;
    uint32 mastervolume;
    uint32 release;

    uint8 regs[4];
    uint8 update;
    uint8 key;
    uint8 adr;
    uint8 envadr;
    uint8 duty;
} MMC5_SQUARE;

typedef struct {
    int32 output;
    uint8 key;
} MMC5_DA;

typedef struct {
    MMC5_SQUARE square[2];
    MMC5_DA da;
} MMC5SOUND;

/* ------------------------------------------------------------ */
/* N106 Sound struct */

typedef struct {
    uint32 logvol;
    int32 cycles;
    uint32 spd;
    uint32 phase;
    uint32 tlen;
    uint8 tadr;
} N106_WM;

typedef struct {
    uint32 cps;
    uint32 mastervolume;

    N106_WM ch[8];

    uint8 addressauto;
    uint8 address;
    uint8 chinuse;

    uint32 tone[0x100];	/* TONE DATA */
    uint8 data[0x80];
} N106SOUND;

/* ------------------------------------------------------------ */
/* FME7 Sound struct */

typedef struct {
    uint32 cps;
    int32 cycles;
    uint32 spd;
    uint8 regs[3];
    uint8 update;
    uint8 adr;
    uint8 key;
} PSG_SQUARE;

typedef struct {
    uint32 cps;
    int32 cycles;
    uint32 spd;
    uint32 noiserng;
    uint8 regs[1];
    uint8 update;
    uint8 noiseout;
} PSG_NOISE;

typedef struct {
    uint32 cps;
    int32 cycles;
    uint32 spd;
    uint32 envout;
    int8 *adr;
    uint8 regs[3];
    uint8 update;
} PSG_ENVELOPE;

typedef struct {
    PSG_SQUARE square[3];
    PSG_ENVELOPE envelope;
    PSG_NOISE noise;
    uint32 mastervolume;
    uint32 adr;
} PSGSOUND;

/* ------------------------------------------------------------ */
/* APU Sound struct */

enum {
    APU_FILTER_NONE,
    APU_FILTER_LOWPASS,
    APU_FILTER_WEIGHTED
};

#ifdef APU_YANO
enum {
    APUMODE_IDEAL_TRIANGLE,
    APUMODE_SMOOTH_ENVELOPE,
    APUMODE_SMOOTH_SWEEP
};
#endif /* APU_YANO */

#if 0
typedef struct {
    uint32 min_range, max_range;
    uint8 (*read_func)(uint32 address);
} apu_memread;

typedef struct {
    uint32 min_range, max_range;
    void (*write_func)(uint32 address, uint8 value);
} apu_memwrite;
#else
typedef uint8 (*apu_memread)(uint32 address);
typedef void (*apu_memwrite)(uint32 address, uint8 value);
#endif

/* APU queue structure */
#define  APUQUEUE_SIZE  8192
#define  APUQUEUE_MASK  (APUQUEUE_SIZE - 1)

/* apu ring buffer member */
typedef struct apudata_s {
    uint32 timestamp, address;
    uint8 value;
} apudata_t;

typedef struct apu_s {
    APUSOUND apus;
    VRC6SOUND vrc6s;
    OPLLSOUND ym2413s;
    FDSSOUND fdssound;
    MMC5SOUND mmc5;
    N106SOUND n106s;
    PSGSOUND psg;
    
    uint8 enable_reg;
    uint8 enable_reg_cur;
    
    apudata_t queue[APUQUEUE_SIZE];
    int q_head, q_tail;
    
    /* for ExSound */
    apudata_t ex_queue[APUQUEUE_SIZE];
    int ex_q_head, ex_q_tail;
    int exsound_enable;
    nes_apu_exsound_type_t exsound_type;
    nes_apu_exsound_t exsound;
    
    uint32 elapsed_cycles;
    
    int32 cycle_rate;
    int sample_rate;
    int sample_bits;
    int refresh_rate;
} apu_t;

extern apu_t *apu;

/* Function prototypes */
apu_t *apu_getcontext(void);
void apu_setcontext(apu_t *src_apu);

apu_t *apu_create(int sample_rate, int refresh_rate, int frag_size,
                         int sample_bits);
void apu_destroy(apu_t **apu);
void apu_setparams(int sample_rate, int refresh_rate, int sample_bits);
void apu_set_exsound(nes_apu_exsound_type_t ex_type);
void apu_set_exsound_enable(int enable);

void apu_process(void *buffer, int num_samples);
void apu_reset(void);

#ifdef APU_YANO
extern void apu_setmode(int item, int mode);
#endif /* APU_YANO */

uint8 apu_read(uint32 address);
void apu_write(uint32 address, uint8 value);

uint8 ex_read(uint32 address);
void ex_write(uint32 address, uint8 value);

void apu_write_cur(uint32 address, uint8 value);
void sync_apu_register();
boolean sync_dmc_register(uint32 cpu_cycles);

#endif /* _NES_APU_H_ */
