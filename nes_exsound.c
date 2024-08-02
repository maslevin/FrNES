/*
** ExSound by TAKEDA, toshiya
**
** original: nezp0922
*/

#include <kos.h>
#include <stdlib.h>
#include <math.h>

#include "nes_apu.h"

// ----------------------------------------------------------------------------
// common

// ex_chip	=  0; none
//		=  1; VRC6
//		=  2; VRC7
//		=  4; FDS
//		=  8; MMC5
//		= 16; N106
//		= 32; FME7
//		= 64; J106 (reserved)

//#define SAMPLE_RATE 44100
#define SAMPLE_RATE apu->sample_rate
#define NES_BASECYCLES (21477270)
#define M_PI 3.14159265358979323846

#include "sound_tables/tbl_param.h"

static const uint32 lineartbl[] = {
#include "sound_tables/tbl/lineartable.h"
};
static const uint32 logtbl[] = {
#include "sound_tables/tbl/logtable.h"
};

uint32 LinearToLog(int32 l) {
	return (l < 0) ? (lineartbl[-l] + 1) : lineartbl[l];
}

int32 LogToLinear(uint32 l, uint32 sft) {
	int32 ret;
	uint32 ofs;
	sft += (l >> 1) >> LOG_BITS;
	if (sft >= LOG_LIN_BITS) return 0;
	ofs = (l >> 1) & ((1 << LOG_BITS) - 1);
	ret = logtbl[ofs] >> sft;
	
	return (l & 1) ? -ret : ret;
}

void LogTableInitialize() {
	if (sizeof(lineartbl) != sizeof(uint32) * lineartable_len) {
		printf("WARNING: Failed assertion lineartbl is not of expected size\n");
	}
	if (sizeof(logtbl) != sizeof(uint32) * logtable_len) {
		printf("WARNING: Failed assertion logtbl is not of expected size\n");
	}
}

uint32 DivFix(uint32 p1, uint32 p2, uint32 fix) {
	uint32 ret;
	ret = p1 / p2;
	p1  = p1 % p2;/* p1 = p1 - p2 * ret; */
	while (fix--) {
		p1 += p1;
		ret += ret;
		if (p1 >= p2)
		{
			p1 -= p2;
			ret++;
		}
	}
	return ret;
}

/****************************************************************************
** Konami VRC6 ExSound by TAKEDA, toshiya, nezp0922
**
** original: s_vrc6.c in nezp0922
*****************************************************************************/
int32 VRC6SoundSquareRender(VRC6_SQUARE *ch) {
    uint32 output;
    if (ch->update) {
        if (ch->update & (2 | 4)) {
            ch->spd = (((ch->regs[2] & 0x0F) << 8) + ch->regs[1] + 1) << 18;
        }
        ch->update = 0;
    }
    
    if (!ch->spd) return 0;
    
    ch->cycles -= ch->cps;
    while (ch->cycles < 0) {
        ch->cycles += ch->spd;
        ch->adr++;
    }
    ch->adr &= 0xF;
    
    if (!(ch->regs[2] & 0x80)) return 0;
    
    output = LinearToLog(ch->regs[0] & 0x0F) + apu->vrc6s.mastervolume;
    if (!(ch->regs[0] & 0x80) && (ch->adr < ((ch->regs[0] >> 4) + 1))) {
#if 1
        return 0;	/* and array gate */
#else
        output++;	/* negative gate */
#endif
    }
    return LogToLinear(output, LOG_LIN_BITS - LIN_BITS - 16 - 1);
}

int32 VRC6SoundSawRender(VRC6_SAW *ch) {
    uint32 output;

    if (ch->update) {
        if (ch->update & (2 | 4)) {
            ch->spd = (((ch->regs[2] & 0x0F) << 8) + ch->regs[1] + 1) << 18;
        }
        ch->update = 0;
    }

    if (!ch->spd) return 0;

    ch->cycles -= ch->cps;
    while (ch->cycles < 0) {
        ch->cycles += ch->spd;
        ch->output += (ch->regs[0] & 0x3F);
        if (7 == ++ch->adr) {
            ch->adr = 0;
            ch->output = 0;
        }
    }

    if (!(ch->regs[2] & 0x80)) return 0;

    output = LinearToLog((ch->output >> 3) & 0x1F) + apu->vrc6s.mastervolume;
    return LogToLinear(output, LOG_LIN_BITS - LIN_BITS - 16 - 1);
}

int32 VRC6SoundRender() {
    int32 accum = 0;
    accum += VRC6SoundSquareRender(&apu->vrc6s.square[0]);
    accum += VRC6SoundSquareRender(&apu->vrc6s.square[1]);
    accum += VRC6SoundSawRender(&apu->vrc6s.saw);
    return accum >> 8;
}

void VRC6SoundVolume(uint32 volume) {
	apu->vrc6s.mastervolume = (volume << (LOG_BITS - 8)) << 1;
}

void VRC6SoundWrite9000(uint32 address, uint8 value) {
	apu->vrc6s.square[0].regs[address & 3] = value;
	apu->vrc6s.square[0].update |= 1 << (address & 3); 
}

void VRC6SoundWriteA000(uint32 address, uint8 value) {
	apu->vrc6s.square[1].regs[address & 3] = value;
	apu->vrc6s.square[1].update |= 1 << (address & 3); 
}

void VRC6SoundWriteB000(uint32 address, uint8 value) {
	apu->vrc6s.saw.regs[address & 3] = value;
	apu->vrc6s.saw.update |= 1 << (address & 3); 
}

void VRC6SoundWrite(uint32 address, uint8 value) {
	switch(address & 0xF000) {
		case 0x9000:
			VRC6SoundWrite9000(address, value);
			break;
		case 0xA000:
			VRC6SoundWriteA000(address, value);
			break;
		case 0xB000:
			VRC6SoundWriteB000(address, value);
			break;
	}
}

void VRC6SoundSquareReset(VRC6_SQUARE *ch) {
	ch->cps = DivFix(NES_BASECYCLES, 12 * SAMPLE_RATE, 18);
}

void VRC6SoundSawReset(VRC6_SAW *ch) {
	ch->cps = DivFix(NES_BASECYCLES, 24 * SAMPLE_RATE, 18);
}

void VRC6SoundReset(void) {
	memset(&apu->vrc6s, 0, sizeof(VRC6SOUND));
	VRC6SoundSquareReset(&apu->vrc6s.square[0]);
	VRC6SoundSquareReset(&apu->vrc6s.square[1]);
	VRC6SoundSawReset(&apu->vrc6s.saw);
}

/****************************************************************************
** Konami VRC7 ExSound by TAKEDA, toshiya, nezp0922
**
** original: s_vrc7.c in nezp0922
*****************************************************************************/

#define OPLL_CH_MAX 6/* 9 */

/* PG_BITS + PG_SHIFT < 32 */
#define PG_BITS 10
#define AM_BITS 8
#define PM_BITS 8

#define AM_DEPTH 4.8  /* 1.0 */		/* dB */
#define PM_DEPTH 14.0 /* 7.0 */		/* cent */
#define PM_SHIFT 16

#define PG_SHIFT (20 - PG_BITS)
#define EG_SHIFT 7/*8*/
#define LOG_KEYOFF (32 << LOG_BITS)
#define EG_KEYOFF (LOG_KEYOFF << EG_SHIFT)

typedef enum {
	EGM_OFF,
	EGM_A,
	EGM_D,
	EGM_S,
	EGM_SS,
	EGM_R
} OPLL_EGMODE;

#define OPLL_UPDATE_FREQH		(1 << 0)
#define OPLL_UPDATE_FREQL		(1 << 1)
#define OPLL_UPDATE_TONE		(1 << 2)

static uint32 amtbl[1 << AM_BITS];
static uint32 pmtbl[1 << PM_BITS];
static uint32 sintbl[1 << PG_BITS];
static uint32 sintbld[1 << PG_BITS];

static uint8 multbl[16] = {
	1,1*2,2*2,3*2,4*2,5*2,6*2,7*2,8*2,9*2,10*2,10*2,12*2,12*2,15*2,15*2,
};
static uint8 fbtbl[8] = {
	31,6,5,4,3,2,1,0,
};
static uint8 oplltone[][8] = {
	{  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },
#if 1
	/* VRC7ED2(00/06/26) vrc7tone.bin */
	/*  AM1  AM2  KS1  KS2  AD1  AD2  SR1  SR2 */
	{  0x53,0x02,0x10,0x07,0xF3,0xF0,0x48,0x1E },
	{  0x03,0x51,0x5E,0x07,0xF1,0xF1,0x0E,0x0C },
	{  0x01,0x51,0x4F,0x04,0xD8,0xE0,0x0F,0x0F },
	{  0x01,0x41,0x1A,0x01,0xA0,0xC0,0xFF,0xFF },
	{  0x02,0x01,0x18,0x07,0xD2,0xA1,0xA7,0xE8 },
	{  0x34,0x21,0xD0,0x07,0x44,0x84,0xF4,0xEE },
	{  0x21,0x61,0x17,0x07,0x71,0x81,0xCE,0x9B },
	{  0x33,0x31,0x58,0x06,0xF0,0xF0,0x0E,0x0E },
	{  0x07,0x01,0x2F,0x00,0xE0,0xE0,0xFF,0xFF },
	{  0x37,0xA1,0x1B,0x07,0xF1,0xF1,0xF4,0xEE },
	{  0x07,0x81,0x24,0x07,0xFF,0xFF,0xF4,0xFF },
	{  0x61,0x81,0x08,0x72,0xF0,0xF0,0xBD,0xAC },
	{  0x01,0x02,0xD6,0x07,0xF8,0xE4,0xFF,0xCF },
	{  0x21,0x61,0x00,0x00,0xF3,0xE2,0xB6,0xD9 },
	{  0x11,0x11,0xD4,0x07,0xFF,0xFF,0xFF,0xFF },
#endif
	{  0x13,0x11,0x25,0x00,0xd7,0xb7,0xf4,0xf4 }, /* 16 Rhythm 1: */
	{  0x13,0x11,0x25,0x00,0xd7,0xb7,0xf4,0xf4 }, /* 17 Rhythm 2: */
	{  0x13,0x11,0x25,0x00,0xd7,0xb7,0xf4,0xf4 }, /* 18 Rhythm 3: */
};

inline static uint32 PGCALC(OPLL_OP *pop) {
	if (pop->pg_vib) {
		pop->vib_cycles -= 1 << PM_SHIFT;
		while (pop->vib_cycles < 0) {
			pop->vib_cycles += apu->ym2413s.lfo[1].output;
			pop->pg_phase += pop->pg_spd;
		}
	} else {
		pop->pg_phase += pop->pg_spd;
	}
	return (pop->input + (pop->pg_phase >> PG_SHIFT)) & ((1 << PG_BITS) - 1);
}

inline static void OPKEY(OPLL_OP *pop, uint8 key) {
	if (key) {
		pop->eg_mode = EGM_A;
		pop->eg_phase = 63 << (EG_SHIFT + LOG_BITS - 3);
	} else {
		if (pop->eg_mode == EGM_OFF) return;
		pop->eg_mode = EGM_R;
	}
}

inline static uint32 EGCALC(OPLL_OP *pop) {
	uint32 egout = pop->tl << (LOG_BITS - 3);
	if (pop->eg_am) egout += apu->ym2413s.lfo[0].output;
	switch (pop->eg_mode) {
		case EGM_A:
			{
				uint32 attack;
				attack = 1 + (pop->eg_phase >> pop->eg_arr);
				if (pop->eg_phase > attack) {
					pop->eg_phase -= attack;
					break;
				}
			}
			pop->eg_phase = 0;
			pop->eg_mode = EGM_D;
		case EGM_D:
			if (pop->eg_phase < pop->eg_sl) {
				pop->eg_phase += pop->eg_drr;
				break;
			}
			if (pop->eg_type) {
				pop->eg_mode = EGM_SS;
				break;
			}
			pop->eg_mode = EGM_S;
			break;
		case EGM_S:
			if (pop->eg_phase < EG_KEYOFF) {
				pop->eg_phase += pop->eg_rrr;
			} else {
				pop->eg_mode = EGM_OFF;
			}
			break;
		case EGM_SS:
			break;
		case EGM_R:
			if (pop->eg_phase < EG_KEYOFF) {
				if (pop->su_type) {
					pop->eg_phase += (5 << 2);
				} else if (pop->eg_type) {
					pop->eg_phase += pop->eg_rrr;
                } else {
					pop->eg_phase += (13 << 2);
                }
				break;
			}
			pop->eg_mode = EGM_OFF;
			break;
		case EGM_OFF:
			return EG_KEYOFF;
	}
	return ((pop->eg_phase >> EG_SHIFT) + egout) << 1;
}

static uint32 OPCALC(OPLL_OP *pop) {
	return pop->sintblp[PGCALC(pop)] + EGCALC(pop);
}

inline static void EGSET(OPLL_CH *pch) {
	pch->op[0].eg_am = pch->tone[0] & 0x80;
	pch->op[1].eg_am = pch->tone[1] & 0x80;
	pch->op[0].tl = pch->tone[2] & 0x3F;
	pch->op[0].sintblp = (pch->tone[3] & 0x08) ? sintbld : sintbl;
	pch->op[1].sintblp = (pch->tone[3] & 0x10) ? sintbld : sintbl;
	pch->op[0].eg_ks = pch->tone[2] >> 6;
	pch->op[1].eg_ks = pch->tone[3] >> 6;
	pch->op[0].eg_type = pch->tone[0] & 0x20;
	pch->op[1].eg_type = pch->tone[1] & 0x20;
	pch->op[0].eg_ar = (pch->tone[4] & 0xF0) >> 2;
	pch->op[1].eg_ar = (pch->tone[5] & 0xF0) >> 2;
	pch->op[0].eg_dr = (pch->tone[4] & 0x0F) << 2;
	pch->op[1].eg_dr = (pch->tone[5] & 0x0F) << 2;
	pch->op[0].eg_rr = (pch->tone[6] & 0x0F) << 2;
	pch->op[1].eg_rr = (pch->tone[7] & 0x0F) << 2;
	pch->op[0].eg_sl = (pch->tone[6] & 0xF0) << (EG_SHIFT + LOG_BITS - 3 - 2);
	pch->op[1].eg_sl = (pch->tone[7] & 0xF0) << (EG_SHIFT + LOG_BITS - 3 - 2);
}

inline static void EGSET2(OPLL_OP *pop) {
	pop->eg_arr = (15 - (pop->eg_ar >> 2));
	pop->eg_drr = pop->eg_dr;
	pop->eg_rrr = pop->eg_rr;
}

inline static void EGSET2KSR(OPLL_OP *pop, uint8 freqh) {
	uint8 ksr = (freqh & 0x0F) >> (3 - pop->eg_ks);
	if (pop->eg_ar) {
		uint8 arr = pop->eg_ar + ksr;
		if (arr > 63) arr = 63;
		pop->eg_arr = (15 - (arr >> 2));
	} else {
		pop->eg_arr = 15;
	}
	if (pop->eg_dr) {
		pop->eg_drr = pop->eg_dr + ksr;
		if (pop->eg_drr > 63) pop->eg_drr = 63;
	} else {
		pop->eg_drr = 0;
	}
	if (pop->eg_rr) {
		pop->eg_rrr = pop->eg_rr + ksr;
		if (pop->eg_rrr > 63) pop->eg_rrr = 63;
	} else {
		pop->eg_rrr = 0;
	}
}

inline static void PGSET(OPLL_CH *pch) {
	uint32 fnum, block;
	fnum = ((pch->freqh << 8) + pch->freql) & 0x1FF;
	block = (pch->freqh >> 1) & 7;
	pch->op[0].pg_spd = (multbl[pch->tone[0] & 0xF] * fnum) << block;
	pch->op[1].pg_spd = (multbl[pch->tone[1] & 0xF] * fnum) << block;
}

inline static int32 CHCALC(OPLL_CH *pch) {
	if (pch->update) {
		if (pch->update & (OPLL_UPDATE_FREQL | OPLL_UPDATE_FREQH | OPLL_UPDATE_TONE)) {
			PGSET(pch);
		}
		if (pch->update & OPLL_UPDATE_TONE) {
			pch->fb = fbtbl[pch->tone[3] & 0x7] + (LOG_LIN_BITS - PG_BITS);
			pch->fbbuf[0] = pch->fbbuf[1] = 0;
			if (pch->fb > 31) pch->fb = 31;
			pch->op[0].pg_vib = pch->tone[0] & 0x40;
			pch->op[1].pg_vib = pch->tone[1] & 0x40;
			EGSET(pch);
		}
		if (pch->update & OPLL_UPDATE_FREQH) {
			if ((pch->key ^ pch->freqh) & 0x10) {
				pch->key ^= 0x10;
				OPKEY(&pch->op[0], (uint8)(pch->key & 0x10));
				OPKEY(&pch->op[1], (uint8)(pch->key & 0x10));
			}
			pch->op[0].su_type = pch->freqh & 0x20;
			pch->op[1].su_type = pch->freqh & 0x20;
		}
		if (pch->update & (OPLL_UPDATE_FREQH | OPLL_UPDATE_TONE)) {
			int op;
			for (op = 0; op < 2; op++) {
				if (pch->tone[op] & 0x10)
					EGSET2KSR(&pch->op[op], pch->freqh);
				else
					EGSET2(&pch->op[op]);
			}
		}
		pch->update = 0;
	}
	pch->cycles -= pch->cps;
	while (pch->cycles < 0) {
		uint32 opout;
		pch->cycles += 1 << 18;
#if 0
		pch->op[0].input = pch->fbbuf[0] + pch->fbbuf[1];
		opout = OPCALC(&pch->op[0]);
		pch->fbbuf[1] = pch->fbbuf[0];
		pch->fbbuf[0] = LogToLinear(opout, pch->fb + 1);
		pch->op[1].input = LogToLinear(opout, (LOG_LIN_BITS - PG_BITS - 1));
#else
		pch->op[0].input = pch->fbbuf[0];
		opout = OPCALC(&pch->op[0]);
		pch->fbbuf[0] = LogToLinear(opout, pch->fb);
		pch->op[1].input = LogToLinear(opout, (LOG_LIN_BITS - PG_BITS - 1));
#endif
		pch->output = OPCALC(&pch->op[1]);
	}
	return LogToLinear(pch->output + apu->ym2413s.mastervolume, (LOG_LIN_BITS - 21));
}

inline static int32 LFOCALC(OPLL_LFO *pch) {
	pch->cycles -= pch->cps;
	while (pch->cycles < 0) {
		pch->cycles += pch->spd;
		pch->adr++;
	}
	pch->adr &= pch->adrmask;
	pch->output = pch->table[pch->adr];
	return pch->output;
}

int32 OPLLSoundRender(void) {
	int32 accum = 0;
	uint8 ch;
#if OPLL_CH_MAX == 9
	uint8 chmax = (apu->ym2413s.rhythmc & 0x20) ? 6 : 9;
#else
	const uint8 chmax = 6;	/* VRCVII */
#endif
	for (ch = 0; ch < 2; ch++) LFOCALC(&apu->ym2413s.lfo[ch]);
	for (ch = 0; ch < chmax; ch++) {
		accum += CHCALC(&apu->ym2413s.ch[ch]);
	}
	return accum >> 8;
}

void OPLLSoundVolume(uint32 volume) {
	apu->ym2413s.mastervolume = (volume << (LOG_BITS - 8)) << 1;
}

void OPLLSoundWriteAddr(uint32 address, uint8 value) {
	apu->ym2413s.adr = value;
}

void OPLLSoundWriteData(uint32 address, uint8 value) {
	int ch = apu->ym2413s.adr & 0xF;
	switch (apu->ym2413s.adr & 0xF8) {
		case 0x00:
			apu->ym2413s.usertone[apu->ym2413s.adr] = value;
			apu->ym2413s.toneupdate = ~0;
			break;
#if OPLL_CH_MAX == 9
		case 0x08:
			if (ch != 0xE) break;
			apu->ym2413s.rhythmc = value;
			/* rhythmmode */
			break;
#endif
		case 0x10:
		case 0x18:
			if (ch >= OPLL_CH_MAX) break;
			apu->ym2413s.ch[ch].freql = value;
			apu->ym2413s.ch[ch].update |= OPLL_UPDATE_FREQL;
			break;
		case 0x20:
		case 0x28:
			if (ch >= OPLL_CH_MAX) break;
			apu->ym2413s.ch[ch].freqh = value;
			apu->ym2413s.ch[ch].update |= OPLL_UPDATE_FREQH;
			break;
		case 0x30:
		case 0x38:
			if (ch >= OPLL_CH_MAX) break;
#if OPLL_CH_MAX == 9
			if ((apu->ym2413s.rhythmc & 0x20) && (ch > 5)) {
				if (ch != 6)
					apu->ym2413s.ch[ch].op[0].tl = ((value >> 4) & 0xF) << 2;
				apu->ym2413s.ch[ch].op[1].tl = (value & 0xF) << 2;
			}
			else
#endif
			{
				if (apu->ym2413s.ch[ch].toneno != (value >> 4)) {
					if (value >> 4) {
						apu->ym2413s.ch[ch].toneno = value >> 4;
						memcpy(apu->ym2413s.ch[ch].tone, oplltone[value >> 4], 8);
						apu->ym2413s.ch[ch].update |= OPLL_UPDATE_TONE;
					} else if ((apu->ym2413s.ch[ch].toneno != 0x10) || (apu->ym2413s.toneupdate & (1 << ch))) {
						apu->ym2413s.toneupdate &= ~(1 << ch);
						apu->ym2413s.ch[ch].toneno = 0x10;
						memcpy(apu->ym2413s.ch[ch].tone, apu->ym2413s.usertone, 8);
						apu->ym2413s.ch[ch].update |= OPLL_UPDATE_TONE;
					}
				}
				apu->ym2413s.ch[ch].op[1].tl = (value & 0xF) << 2;
			}
			break;
	}
}

void OPLLSoundWrite(uint32 address, uint8 value) {
	if (address == 0x9010) {
		OPLLSoundWriteAddr(address, value);
	} else if(address == 0x9030) {
		OPLLSoundWriteData(address, value);
	}
}

void OPLLSoundReset(void) {
	uint32 i, w;
	double a;
	uint32 cps;
	memset(&apu->ym2413s, 0, sizeof(OPLLSOUND));
	cps = DivFix(NES_BASECYCLES, 6 * 72 * SAMPLE_RATE, 18);
	for (i = 0; i < OPLL_CH_MAX; i++) {
		apu->ym2413s.ch[i].cps = cps;
		apu->ym2413s.ch[i].toneno = 0x10;
		apu->ym2413s.ch[i].fb = 31;
		apu->ym2413s.ch[i].output = 0 + (LOG_KEYOFF << 1);
		apu->ym2413s.ch[i].op[0].sintblp = sintbl;
		apu->ym2413s.ch[i].op[1].sintblp = sintbl;
	}

	w = 1 << (PG_BITS - 2);
	sintbl[w * 0] = 0 + (LOG_KEYOFF << 1);
	sintbl[w * 1] = 0;
	sintbl[w * 2] = 1 + (LOG_KEYOFF << 1);
	sintbl[w * 3] = 1;
	sintbld[w * 0] = 0 + (LOG_KEYOFF << 1);
	sintbld[w * 1] = 0;
	sintbld[w * 2] = 0 + (LOG_KEYOFF << 1);
	sintbld[w * 3] = 0 + (LOG_KEYOFF << 1);
	for (i = 1; i < w; i++) {
		uint32 ua;
		a = sin((2 * M_PI * i) / (double)(w << 2)) * (1 << LOG_LIN_BITS);
		ua = (uint32)((LOG_LIN_BITS - (log(a) / log(2))) * (1 << LOG_BITS));
		sintbl[w * 0 + i] = sintbl[w * 2 - i] = 0 + (ua << 1);
		sintbl[w * 2 + i] = sintbl[w * 4 - i] = 1 + (ua << 1);
		sintbld[w * 0 + i] = sintbld[w * 2 - i] = 0 + (ua << 1);
		sintbld[w * 2 + i] = sintbld[w * 4 - i] = 0 + (LOG_KEYOFF << 1);
	}
	/* AM(tremoro) */
	w = 1 << AM_BITS;
	for (i = 0; i < w; i++) {
		a = (1 + sin((2 * M_PI * i) / w)) * ((1 << LOG_BITS) * AM_DEPTH / 12);
		amtbl[i] = (uint32)a;
	}
	apu->ym2413s.lfo[0].cps = cps;
	apu->ym2413s.lfo[0].spd = DivFix(NES_BASECYCLES, (uint32)((6 * 72 * (1 << AM_BITS)) * 3.7), 18);
	apu->ym2413s.lfo[0].adrmask = w - 1;
	apu->ym2413s.lfo[0].table = amtbl;
	apu->ym2413s.lfo[0].output = apu->ym2413s.lfo[0].table[0];
	/* VIB */
	w = 1 << PM_BITS;
	for (i = 0; i < w; i++) {
		a = pow(2, PM_DEPTH * sin((2 * M_PI * i) / w) / 1200.0) * (1 << PM_SHIFT);
		pmtbl[i] = (uint32)a;
	}
	apu->ym2413s.lfo[1].cps = cps;
	apu->ym2413s.lfo[1].spd = DivFix(NES_BASECYCLES, (uint32)((6 * 72 * (1 << PM_BITS)) * 6.4), 18);
	apu->ym2413s.lfo[1].adrmask = w - 1;
	apu->ym2413s.lfo[1].table = pmtbl;
	apu->ym2413s.lfo[1].output = apu->ym2413s.lfo[1].table[0];
}

void VRC7SetTone(uint8 *p) {
	int i, j;
	for (i = 1; i < 16; i++) {
		for (j = 0; j < 8; j++) {
			oplltone[i][j] = *p++;
		}
	}
}

/****************************************************************************
** Konami FDS ExSound by TAKEDA, toshiya, nezp0922, stun
**
** original: s_fds.c in nezp0922
*****************************************************************************/
int32 FDSSoundRender(void) {
	FDS_FMOP *op0, *op1;
	FDSSOUND *fds;

	fds = &(apu->fdssound);
	op0 = fds->op;
	op1 = fds->op + 1;

	if (op0->timer >= 0)
		--(op0->timer);

	if (op0->timer == 0) {
		op1->sweep = op0->last_spd;
		op0->envmode = 1;
	}

	if (op0->envmode == 1) {
		op1->spd = op1->sweep;
	} else if (op0->envmode & 0x80) {
		op0->envphase++;
		if (!(op0->envmode & 0x40)) {
			if (op0->envphase == op0->envspd) {
				op0->envphase = 0;
				if (op1->spd < op1->sweep) {
					op1->spd += op0->volume;
					if (op1->spd > op1->sweep)
						op1->spd = op1->sweep;
				}
			}
		} else {
			if (op0->envphase == op0->envspd) {
				op0->envphase = 0;
				if (op1->spd > op1->sweep / 2) {
					op1->spd -= op0->volume; 
					if (op1->spd < op1->sweep)
						op1->spd = op1->sweep;
				}
			}
		}
	}

	{
		uint32 vol; 

		op0->envout = LinearToLog(0);

		if (op1->envmode && fds->fade) {
			op1->envphase -= fds->cps >> (11 - 1);
			if (op1->envmode & 0x40) {
				while (op1->envphase < 0)
				{
					op1->envphase += op1->envspd;
					op1->volume += (op1->volume < 0x1f);
				}
			} else {
				while (op1->envphase < 0)
				{
					op1->envphase += op1->envspd;
					op1->volume -= (op1->volume > 0x00);
				}
			}
		}
		vol = op1->volume;
		op1->envout = LinearToLog(vol) + fds->mastervolume;
	}

	{
		fds->cycles -= fds->cps;
		fds->output = 0;

		if (fds->cycles < 0) {
			int cycles = - fds->cycles;
			int count;
			
			if ((cycles & 0x7ff) == 0) {
				count = cycles >> 23;
			} else {
				count = (cycles >> 23) + 1;
			}
			
			fds->cycles += (count << 23); 
			
			if (op0->spd && op0->enable) {
				op0->phase += op0->spd * count;
				if (!(op1->spd) || !(op1->enable)) {
					fds->output = 
						LogToLinear(op0->envout + op0->wave[(op0->phase >> (23 - 1)) & 0x3f], 
							op0->outlvl);
				}
			}
			if (op1->spd && op1->enable) {
				op1->phase += op1->spd * count;
				fds->output = 
					LogToLinear(op1->envout + op1->wave[(op1->phase >> (23 - 1)) & 0x3f], 
							op1->outlvl);
			}
		}
	}

	if (fds->mute) return 0;
	return fds->output >> 8;
}

void FDSSoundVolume(uint32 volume) {
	apu->fdssound.mastervolume = (volume << (LOG_BITS - 8)) << 1;
}

void FDSSoundWrite(uint32 address, uint8 value) {
	if (0x4040 <= address && address <= 0x407F) {
		apu->fdssound.op[1].wave[address - 0x4040] = LinearToLog(((int32)value & 0x3f) - 0x20);
	} else if (0x4080 <= address && address <= 0x408F) {
		int ch = (address < 0x4084);
		FDS_FMOP *pop = &apu->fdssound.op[ch];
		apu->fdssound.reg[address - 0x4080] = value;
		switch (address & 15) {
			case 0:
				if (value & 0x80) {
					pop->volume = (value & 0x3f);
					pop->envmode = 0;
				} else {
					pop->envspd = ((value & 0x3f) + 1) << 23;
					pop->envmode = 0x80 | value;
				}
				break;
			case 4:
				if (value & 0x80) {
					int32 a=apu->fdssound.op[1].spd;
					int32 b=apu->fdssound.op[1].sweep;
					pop->timer=(0x3f-(value & 0x3f)) << 10;
					if(pop->timer==0) pop->timer=1;
					pop->last_spd=a*(0x3f-(value & 0x3f))/0x3f+
								  b*(value & 0x3f)/0x3f;
				} else if(apu->fdssound.op[1].sweep) {
					pop->envspd = (value & 0x3f) << 5;
					if((value & 0x3f)==0) pop->envspd=1;
					pop->envphase = 0;
					pop->envmode = 0x80 | (value & 0x40) ;
					pop->volume=abs(apu->fdssound.op[1].sweep - apu->fdssound.op[1].spd);
					pop->volume/=pop->envspd;
					if((value & 0x3f)==0) pop->envmode=1;
				}
				apu->fdssound.waveaddr = 0;
				break;
			case 1:
				if ((value & 0x7f) < 0x60) {
					apu->fdssound.op[0].sweep = value & 0x7f;
				} else {
					apu->fdssound.op[0].sweep = ((int32)value & 0x7f) - 0x80;
				}
				break;
			case 5:
				if (!value) break;
				if ((value & 0x7f) < 0x60) {
					apu->fdssound.op[1].sweep = (int32)apu->fdssound.op[1].spd+
						((apu->fdssound.op[1].spd * (value & 0x7f))>>5);
				} else {
					apu->fdssound.op[1].sweep = (int32)apu->fdssound.op[1].spd-
						(((apu->fdssound.op[1].spd) * (((int32)value & 0x7f) - 0x80)) >> 5);
				}
				break;
			case 2:
				pop->spd &= 0x00000F00 << 7;
				pop->spd |= (value & 0xFF) << 7;
				apu->fdssound.op[0].envmode = 0;
				apu->fdssound.op[0].timer=0;
				break;
			case 6:
				pop->spd &= 0x00000F00 << 7;
				pop->spd |= (value & 0xFF) << 7;
				pop->envmode = 0;
				break;
			case 3:
				pop->spd &= 0x000000FF << 7;
				pop->spd |= (value & 0x0F) << (7 + 8);
				pop->enable = !(value & 0x80);
				apu->fdssound.op[0].envmode = 0;
				apu->fdssound.op[0].timer=0;
				break;
			case 7:
				pop->spd &= 0x000000FF << 7;
				pop->spd |= (value & 0x0F) << (7 + 8);
				pop->enable = !(value & 0x80);
				apu->fdssound.waveaddr = 0;
				break;
			case 8: {
				static int8 lfotbl[8] = { 0,1,2,3,-4,-3,-2,-1 };
				uint32 v = LinearToLog(lfotbl[value & 7]);
				apu->fdssound.op[0].wave[apu->fdssound.waveaddr++] = v;
				apu->fdssound.op[0].wave[apu->fdssound.waveaddr++] = v;
				if (apu->fdssound.waveaddr == 0x40) {
					apu->fdssound.waveaddr = 0;
				}
			} break;
			case 9:
				apu->fdssound.op[0].outlvl = LOG_LIN_BITS - LIN_BITS - LIN_BITS - 10 - (value & 3);
				if(value & 0x80) apu->fdssound.mute=1;
				else             apu->fdssound.mute=0;
				break;
			case 10:
				apu->fdssound.op[1].outlvl = LOG_LIN_BITS - LIN_BITS - LIN_BITS - 10 - (value & 3);
				if(value & 0x80) apu->fdssound.fade=1;
				else             apu->fdssound.fade=0;
				break;
		}
	}
}

void FDSSoundWriteCurrent(uint32 address, uint8 value) {
	if (0x4080 <= address && address <= 0x408F) {
		apu->fdssound.reg_cur[address - 0x4080] = value;
	}
}

uint8 FDSSoundRead(uint32 address) {
	if (0x4090 <= address && address <= 0x409F) {
		return apu->fdssound.reg_cur[address - 0x4090];
	}
	return 0;
}

void FDSSoundReset(void) {
	int i;
	FDS_FMOP *pop;

	memset(&apu->fdssound, 0, sizeof(FDSSOUND));
	apu->fdssound.cps = DivFix(NES_BASECYCLES, 12 * (1 << 1) * SAMPLE_RATE, 23);
	for (pop = &apu->fdssound.op[0]; pop < &apu->fdssound.op[2]; pop++) {
		pop->enable = 1;
	}
	apu->fdssound.op[0].outlvl = LOG_LIN_BITS - LIN_BITS - LIN_BITS - 10;
	apu->fdssound.op[1].outlvl = LOG_LIN_BITS - LIN_BITS - LIN_BITS - 10;

	for (i = 0; i < 0x40; i++) {
		apu->fdssound.op[1].wave[i] = LinearToLog((i < 0x20) ? 0x1f : -0x20);
	}
}


/****************************************************************************
** Sunsoft FME7 ExSound by TAKEDA, toshiya
**
** original: s_fds.c in nezp0922
*****************************************************************************/

static int8 env_pulse[] = {
	0x1F,+02,0x1E,+02,0x1D,+02,0x1C,+02,0x1B,+02,0x1A,+02,0x19,+02,0x18,+02,
	0x17,+02,0x16,+02,0x15,+02,0x14,+02,0x13,+02,0x12,+02,0x11,+02,0x10,+02,
	0x0F,+02,0x0E,+02,0x0D,+02,0x0C,+02,0x0B,+02,0x0A,+02,0x09,+02,0x08,+02,
	0x07,+02,0x06,+02,0x05,+02,0x04,+02,0x03,+02,0x02,+02,0x01,+02,0x00,+00,
};
static int8 env_pulse_hold[] = {
	0x1F,+02,0x1E,+02,0x1D,+02,0x1C,+02,0x1B,+02,0x1A,+02,0x19,+02,0x18,+02,
	0x17,+02,0x16,+02,0x15,+02,0x14,+02,0x13,+02,0x12,+02,0x11,+02,0x10,+02,
	0x0F,+02,0x0E,+02,0x0D,+02,0x0C,+02,0x0B,+02,0x0A,+02,0x09,+02,0x08,+02,
	0x07,+02,0x06,+02,0x05,+02,0x04,+02,0x03,+02,0x02,+02,0x01,+02,0x00,+02,
	0x1F,+00,
};
static int8 env_saw[] = {
	0x1F,+02,0x1E,+02,0x1D,+02,0x1C,+02,0x1B,+02,0x1A,+02,0x19,+02,0x18,+02,
	0x17,+02,0x16,+02,0x15,+02,0x14,+02,0x13,+02,0x12,+02,0x11,+02,0x10,+02,
	0x0F,+02,0x0E,+02,0x0D,+02,0x0C,+02,0x0B,+02,0x0A,+02,0x09,+02,0x08,+02,
	0x07,+02,0x06,+02,0x05,+02,0x04,+02,0x03,+02,0x02,+02,0x01,+02,0x00,-62,
};
static int8 env_tri[] = {
	0x1F,+02,0x1E,+02,0x1D,+02,0x1C,+02,0x1B,+02,0x1A,+02,0x19,+02,0x18,+02,
	0x17,+02,0x16,+02,0x15,+02,0x14,+02,0x13,+02,0x12,+02,0x11,+02,0x10,+02,
	0x0F,+02,0x0E,+02,0x0D,+02,0x0C,+02,0x0B,+02,0x0A,+02,0x09,+02,0x08,+02,
	0x07,+02,0x06,+02,0x05,+02,0x04,+02,0x03,+02,0x02,+02,0x01,+02,0x00,+02,
	0x00,+02,0x01,+02,0x02,+02,0x03,+02,0x04,+02,0x05,+02,0x06,+02,0x07,+02,
	0x08,+02,0x09,+02,0x0A,+02,0x0B,+02,0x0C,+02,0x0D,+02,0x0E,+02,0x0F,+02,
	0x10,+02,0x11,+02,0x12,+02,0x13,+02,0x14,+02,0x15,+02,0x16,+02,0x17,+02,
	0x18,+02,0x19,+02,0x1A,+02,0x1B,+02,0x1C,+02,0x1D,+02,0x1E,+02,0x1F,-126,
};
static int8 env_xpulse[] = {
	0x00,+02,0x01,+02,0x02,+02,0x03,+02,0x04,+02,0x05,+02,0x06,+02,0x07,+02,
	0x08,+02,0x09,+02,0x0A,+02,0x0B,+02,0x0C,+02,0x0D,+02,0x0E,+02,0x0F,+02,
	0x10,+02,0x11,+02,0x12,+02,0x13,+02,0x14,+02,0x15,+02,0x16,+02,0x17,+02,
	0x18,+02,0x19,+02,0x1A,+02,0x1B,+02,0x1C,+02,0x1D,+02,0x1E,+02,0x1F,+00,
};
static int8 env_xpulse_hold[] = {
	0x00,+02,0x01,+02,0x02,+02,0x03,+02,0x04,+02,0x05,+02,0x06,+02,0x07,+02,
	0x08,+02,0x09,+02,0x0A,+02,0x0B,+02,0x0C,+02,0x0D,+02,0x0E,+02,0x0F,+02,
	0x10,+02,0x11,+02,0x12,+02,0x13,+02,0x14,+02,0x15,+02,0x16,+02,0x17,+02,
	0x18,+02,0x19,+02,0x1A,+02,0x1B,+02,0x1C,+02,0x1D,+02,0x1E,+02,0x1F,+02,
	0x00,+00,
};
static int8 env_xsaw[] = {
	0x00,+02,0x01,+02,0x02,+02,0x03,+02,0x04,+02,0x05,+02,0x06,+02,0x07,+02,
	0x08,+02,0x09,+02,0x0A,+02,0x0B,+02,0x0C,+02,0x0D,+02,0x0E,+02,0x0F,+02,
	0x10,+02,0x11,+02,0x12,+02,0x13,+02,0x14,+02,0x15,+02,0x16,+02,0x17,+02,
	0x18,+02,0x19,+02,0x1A,+02,0x1B,+02,0x1C,+02,0x1D,+02,0x1E,+02,0x1F,-62,
};
static int8 env_xtri[] = {
	0x00,+02,0x01,+02,0x02,+02,0x03,+02,0x04,+02,0x05,+02,0x06,+02,0x07,+02,
	0x08,+02,0x09,+02,0x0A,+02,0x0B,+02,0x0C,+02,0x0D,+02,0x0E,+02,0x0F,+02,
	0x10,+02,0x11,+02,0x12,+02,0x13,+02,0x14,+02,0x15,+02,0x16,+02,0x17,+02,
	0x18,+02,0x19,+02,0x1A,+02,0x1B,+02,0x1C,+02,0x1D,+02,0x1E,+02,0x1F,+02,
	0x1F,+02,0x1E,+02,0x1D,+02,0x1C,+02,0x1B,+02,0x1A,+02,0x19,+02,0x18,+02,
	0x17,+02,0x16,+02,0x15,+02,0x14,+02,0x13,+02,0x12,+02,0x11,+02,0x10,+02,
	0x0F,+02,0x0E,+02,0x0D,+02,0x0C,+02,0x0B,+02,0x0A,+02,0x09,+02,0x08,+02,
	0x07,+02,0x06,+02,0x05,+02,0x04,+02,0x03,+02,0x02,+02,0x01,+02,0x00,-126,
};

static int8 *(env_table[16]) = {
	env_pulse,	env_pulse,	env_pulse,	env_pulse,
	env_xpulse,	env_xpulse,	env_xpulse,	env_xpulse,
	env_saw,	env_pulse,	env_tri,	env_pulse_hold,
	env_xsaw,	env_xpulse,	env_xtri,	env_xpulse_hold,
};

uint32 PSGSoundNoise(PSG_NOISE *ch) {
	if (ch->update) {
		if (ch->update & 1) {
			ch->spd = ((ch->regs[0] & 0x1F) + 1) << 18;
		}
		ch->update = 0;
	}

	if (ch->spd) {
		ch->cycles -= ch->cps;
		while (ch->cycles < 0) {
			ch->cycles += ch->spd;
			if ((ch->noiserng + 1) & 2) ch->noiseout = ~ch->noiseout;
			if (ch->noiserng & 1) ch->noiserng ^= 0x28000;
			ch->noiserng >>= 1;
		}
	}
	return ch->noiseout;
}

uint32 PSGSoundEnvelope(PSG_ENVELOPE *ch) {
	if (ch->update) {
		if (ch->update & (1 | 2)) {
			ch->spd = ((ch->regs[1] << 8) + ch->regs[0] + 1) << 18;
		}
		if (ch->update & 4) {
			ch->adr = env_table[ch->regs[2] & 0xF];
		}
		ch->update = 0;
	}

	if (ch->spd) {
		ch->cycles -= ch->cps;
		while (ch->cycles < 0) {
			ch->cycles += ch->spd;
			ch->adr += ch->adr[1];
		}
		if (ch->adr[0] & 0x1F) {
			ch->envout = (0x1F - (ch->adr[0] & 0x1F)) << (LOG_BITS - 2 + 1);
			ch->envout += apu->psg.mastervolume;
			ch->envout = LogToLinear(ch->envout, LOG_LIN_BITS - 21);
		} else {
			ch->envout = 0;
		}
	}
	return ch->envout;
}

int32 PSGSoundSquare(PSG_SQUARE *ch) {
	int32 output = 0;
	if (ch->update) {
		if (ch->update & (1 | 2)) {
			ch->spd = (((ch->regs[1] & 0x0F) << 8) + ch->regs[0] + 1) << 18;
		}
		ch->update = 0;
	}

	if (!ch->spd) return 0;

	ch->cycles -= ch->cps;
	while (ch->cycles < 0) {
		ch->cycles += ch->spd;
		ch->adr++;
	}

	if (ch->key) {
		uint32 volume;
		if (ch->regs[2] & 0x10) {
			volume = apu->psg.envelope.envout;
		} else if (ch->regs[2] & 0xF) {
			volume = (0xF - (ch->regs[2] & 0xF)) << (LOG_BITS - 1 + 1);
			volume += apu->psg.mastervolume;
			volume = LogToLinear(volume, LOG_LIN_BITS - 21);
		} else {
			volume = 0;
		}
		if (ch->key & 1) {
			if (ch->adr & 1) {
				output += volume;
			} else {
				output -= volume;
			}
		}
		if (ch->key & 2) {
			if (apu->psg.noise.noiseout) {
				output += volume;
			} else {
				output -= volume;
			}
		}
	}
	return output;
}

int32 PSGSoundRender(void) {
	int32 accum = 0;
	PSGSoundNoise(&apu->psg.noise);
	PSGSoundEnvelope(&apu->psg.envelope);
	accum += PSGSoundSquare(&apu->psg.square[0]);
	accum += PSGSoundSquare(&apu->psg.square[1]);
	accum += PSGSoundSquare(&apu->psg.square[2]);
	return accum >> 7;
}

void PSGSoundVolume(uint32 volume) {
	apu->psg.mastervolume = (volume << (LOG_BITS - 8)) << 1;
}

void PSGSoundWriteAddr(uint32 address, uint8 value) {
	apu->psg.adr = value;
}

void PSGSoundWriteData(uint32 address, uint8 value) {
	switch (apu->psg.adr) {
		case 0x0: case 0x1:
		case 0x2: case 0x3:
		case 0x4: case 0x5:
			apu->psg.square[apu->psg.adr >> 1].regs[apu->psg.adr & 1] = value;
			apu->psg.square[apu->psg.adr >> 1].update |= 1 << (apu->psg.adr & 1);
			break;
		case 0x6:
			apu->psg.noise.regs[0] = value;
			apu->psg.noise.update |= 1 << 0;
			break;
		case 0x7: {
			uint8 ch;
			for (ch = 0; ch < 3; ch++) {
				apu->psg.square[ch].key = 0;
				if (!(value & (1 << ch))) apu->psg.square[ch].key |= 1;
				if (!(value & (8 << ch))) apu->psg.square[ch].key |= 2;
			}
		} break;
		case 0x8: case 0x9: case 0xA:
			apu->psg.square[apu->psg.adr & 3].regs[2] = value;
			apu->psg.square[apu->psg.adr & 3].update |= 1 << 2;
			break;
		case 0xB: case 0xC: case 0xD:
			apu->psg.envelope.regs[apu->psg.adr - 0xB] = value;
			apu->psg.envelope.update |= 1 << (apu->psg.adr - 0xB);
			break;
	}
}

void PSGSoundWrite(uint32 address, uint8 value) {
	if (address == 0xC000) {
		PSGSoundWriteAddr(address, value);
	} else if (address == 0xE000) {
		PSGSoundWriteData(address, value);
	}
}

void PSGSoundSquareReset(PSG_SQUARE *ch) {
	ch->cps = DivFix(NES_BASECYCLES, 6 * 8 * 4 * SAMPLE_RATE, 18);
}

void PSGSoundNoiseReset(PSG_NOISE *ch) {
	ch->cps = DivFix(NES_BASECYCLES, 6 * 8 * 4 * SAMPLE_RATE, 18);
	ch->noiserng = 1;
	ch->noiseout = 0xFF;
}

void PSGSoundEnvelopeReset(PSG_ENVELOPE *ch) {
	ch->cps = DivFix(NES_BASECYCLES, 6 * 8 * 4 * SAMPLE_RATE, 18);
	ch->adr = env_table[0];
}

void PSGSoundReset(void) {
	memset(&apu->psg, 0, sizeof(PSGSOUND));
	PSGSoundNoiseReset(&apu->psg.noise);
	PSGSoundEnvelopeReset(&apu->psg.envelope);
	PSGSoundSquareReset(&apu->psg.square[0]);
	PSGSoundSquareReset(&apu->psg.square[1]);
	PSGSoundSquareReset(&apu->psg.square[2]);
}

/****************************************************************************
** Nintendo MMC5 ExSound by TAKEDA, toshiya, nezp0922
**
** original: s_apu.c in nezp0922
*****************************************************************************/

#define V(x) (x*64/60)
static uint32 mmc5_vbl_length[32] = {
	V(0x05), V(0x7F), V(0x0A), V(0x01), V(0x14), V(0x02), V(0x28), V(0x03),
	V(0x50), V(0x04), V(0x1E), V(0x05), V(0x07), V(0x06), V(0x0E), V(0x07),
	V(0x06), V(0x08), V(0x0C), V(0x09), V(0x18), V(0x0A), V(0x30), V(0x0B),
	V(0x60), V(0x0C), V(0x24), V(0x0D), V(0x08), V(0x0E), V(0x10), V(0x0F),
};
#undef V

#define V(x) ((x) << 19)
static const uint32 mmc5_spd_limit[8] = {
	V(0x3FF), V(0x555), V(0x666), V(0x71C), 
	V(0x787), V(0x7C1), V(0x7E0), V(0x7F0),
};
#undef V

int32 MMC5SoundSquareRender(MMC5_SQUARE *ch) {
	uint32 output;
	if (ch->update) {
		if (ch->update & 1) {
			ch->duty = (ch->regs[0] >> 4) & 0x0C;
			if (ch->duty == 0) ch->duty = 2;
			ch->envspd = ((ch->regs[0] & 0x0F) + 1) << (19 + 7);
		}
		if (ch->update & 2) {
			ch->sweepspd = (((ch->regs[1] >> 4) & 0x07) + 1) << (19 + 8);
		}
		if (ch->update & (4 | 8)) {
			ch->spd = (((ch->regs[3] & 7) << 8) + ch->regs[2] + 1) << 19;
		}
		if ((ch->update & 8) && (ch->key & 1)) {
			ch->key &= ~2;
			ch->length = (mmc5_vbl_length[ch->regs[3] >> 3] * ch->freq) >> 6;
			ch->envadr = 0;
		}
		ch->update = 0;
	}

	if (ch->key == 0) return 0;

	ch->envphase -= ch->cps >> (13 - 7);
	if (ch->regs[0] & 0x20) {
		while (ch->envphase < 0) {
			ch->envphase += ch->envspd;
			ch->envadr++;
		}
		ch->envadr &= 0x0F;
	} else {
		while (ch->envphase < 0) {
			ch->envphase += ch->envspd;
			ch->envadr += (ch->envadr < 15);
		}
	}

	if (ch->length) {
		if (!(ch->regs[0] & 0x20)) ch->length--;
	} else {
		ch->key |= 2;
	}

	if ((ch->regs[1] & 0x80)) {
		if (ch->regs[1] & 7) {
			ch->sweepphase -= ch->cps >> (14 - 8);
			if (ch->regs[1] & 8) {
				while (ch->sweepphase < 0) {
					ch->sweepphase += ch->sweepspd;
					ch->spd -= ch->spd >> (ch->regs[1] & 7);
				}
			} else {
				while (ch->sweepphase < 0) {
					ch->sweepphase += ch->sweepspd;
					ch->spd += ch->spd >> (ch->regs[1] & 7);
				}
			}
		}
	}

	if (ch->spd < (4 << 19)) return 0;
	if (!(ch->regs[1] & 8)) {
		if (ch->spd > mmc5_spd_limit[ch->regs[1] & 7]) return 0;
	}

	ch->cycles -= ch->cps;
	while (ch->cycles < 0) {
		ch->cycles += ch->spd;
		ch->adr++;
	}
	ch->adr &= 0x0F;

	if (ch->key & 2) {
		if (ch->release < (31 << (LOG_BITS + 1)))
			ch->release += 3 << (LOG_BITS - 8 + 1);
	} else {
		ch->release = 0;
	}

	if (ch->regs[0] & 0x10) /* fixed volume */
		output = ch->regs[0] & 0x0F;
	else
		output = 15 - ch->envadr;

	output = LinearToLog(output) + ch->mastervolume + ch->release;
	output += (ch->adr < ch->duty);
	return LogToLinear(output, LOG_LIN_BITS - LIN_BITS - 16);
}

void MMC5SoundSquareReset(MMC5_SQUARE *ch) {
	memset(ch, 0, sizeof(MMC5_SQUARE));
	ch->freq = SAMPLE_RATE;
	ch->cps = DivFix(NES_BASECYCLES, 12 * ch->freq, 19);
}

int32 MMC5SoundRender(void) {
	int32 accum = 0;
	accum += MMC5SoundSquareRender(&apu->mmc5.square[0]);
	accum += MMC5SoundSquareRender(&apu->mmc5.square[1]);
	return accum >> 8;
}

void MMC5SoundVolume(uint32 volume) {
	volume = (volume << (LOG_BITS - 8)) << 1;
	apu->mmc5.square[0].mastervolume = volume;
	apu->mmc5.square[1].mastervolume = volume;
}

void MMC5SoundWrite(uint32 address, uint8 value) {
	if (0x5000 <= address && address <= 0x5015) {
		switch (address) {
			case 0x5000: case 0x5002: case 0x5003:
			case 0x5004: case 0x5006: case 0x5007: {
				int ch = address >= 0x5004;
				int port = address & 3;
				apu->mmc5.square[ch].regs[port] = value;
				apu->mmc5.square[ch].update |= 1 << port; 
			}
			break;
			case 0x5011:
				apu->mmc5.da.output = ((int32)value) - 0x80;
				break;
			case 0x5010:
				apu->mmc5.da.key = (value & 0x01);
				break;
			case 0x5015:
				if (value & 1) {
					apu->mmc5.square[0].key = 1;
				} else {
					apu->mmc5.square[0].key = 0;
					apu->mmc5.square[0].length = 0;
				}
				if (value & 2) {
					apu->mmc5.square[1].key = 1;
				} else {
					apu->mmc5.square[1].key = 0;
					apu->mmc5.square[1].length = 0;
				}
				break;
		}
	}
}

void MMC5SoundDaReset(MMC5_DA *ch) {
	memset(ch, 0, sizeof(MMC5_DA));
}

void MMC5SoundReset(void) {
	MMC5SoundSquareReset(&apu->mmc5.square[0]);
	MMC5SoundSquareReset(&apu->mmc5.square[1]);
	MMC5SoundDaReset(&apu->mmc5.da);

	MMC5SoundWrite(0x5000, 0x00);
	MMC5SoundWrite(0x5002, 0x00);
	MMC5SoundWrite(0x5003, 0x00);
	MMC5SoundWrite(0x5004, 0x00);
	MMC5SoundWrite(0x5006, 0x00);
	MMC5SoundWrite(0x5007, 0x00);
	MMC5SoundWrite(0x5010, 0x00);
	MMC5SoundWrite(0x5011, 0x00);
}

/****************************************************************************
** Namco 106 ExSound by TAKEDA, toshiya, nezp0922
**
** original: s_apu.c in nezp0922
*****************************************************************************/

int32 N106SoundRender(void) {
	N106_WM *chp;
	int32 accum = 0;
	uint32 cyclesspd = apu->n106s.chinuse << 20;
	uint8 chinuse = apu->n106s.chinuse;

	chp = apu->n106s.ch + 8 - apu->n106s.chinuse;
	while (chinuse--) {
		chp->cycles -= apu->n106s.cps;
		while (chp->cycles < 0) {
			chp->cycles += cyclesspd;
			chp->phase += chp->spd;
		}
		while (chp->phase >= chp->tlen)
			chp->phase -= chp->tlen;

		accum += LogToLinear(apu->n106s.tone[((chp->phase >> 18) + chp->tadr) & 0xff] + chp->logvol + apu->n106s.mastervolume, LOG_LIN_BITS - LIN_BITS - LIN_BITS - 10);

		++chp;
	}
	return accum >> 8;
}

void N106SoundVolume(uint32 volume) {
	apu->n106s.mastervolume = (volume << (LOG_BITS - 8)) << 1;
}

uint8 N106SoundReadData(uint32 address) {
	uint8 ret = apu->n106s.data[apu->n106s.address];
	if (apu->n106s.addressauto)
		apu->n106s.address = (apu->n106s.address + 1) & 0x7f;
	return ret;
}

uint8 N106SoundRead(uint32 address) {
	if (address == 0x4800) {
		return N106SoundReadData(address);
	} else {
		return 0x00;
	}
}

void N106SoundWriteAddr(uint32 address, uint8 value) {
	apu->n106s.address     = value & 0x7f;
	apu->n106s.addressauto = (value & 0x80) ? 1 : 0;
}

void N106SoundWriteData(uint32 address, uint8 value) {
	apu->n106s.data[apu->n106s.address] = value;
	apu->n106s.tone[apu->n106s.address * 2]     = LinearToLog(((int)(value & 0xf) << 2) - 0x20);
	apu->n106s.tone[apu->n106s.address * 2 + 1] = LinearToLog(((int)(value >>  4) << 2) - 0x20);
	if (apu->n106s.address >= 0x40) {
		N106_WM *chp = &apu->n106s.ch[(apu->n106s.address - 0x40) >> 3];
		switch (apu->n106s.address & 7)
		{
			case 0:
				chp->spd = (chp->spd & 0xffff00) | value;
				break;
			case 2:
				chp->spd = (chp->spd & 0xff00ff) | (value << 8);
				break;
			case 4:
				chp->spd = (chp->spd & 0x00ffff) | ((value & 0x3) << 16);
				{
					uint32 tlen;
					
					tlen = (0x20 - (value & 0x1c)) << 18;
					if (chp->tlen != tlen) {
						chp->tlen = tlen;
						chp->phase = 0;
					}
				}
				break;
			case 6:
				chp->tadr = value & 0xff;
				break;
			case 7:
				if (chp == &apu->n106s.ch[7])
					apu->n106s.chinuse = 1 + ((value >> 4) & 0x07);

				chp->logvol = LinearToLog((value & 0x0f) << 2);

				break;
		}
	}
	if (apu->n106s.addressauto) {
		apu->n106s.address = (apu->n106s.address + 1) & 0x7f;
	}
}

void N106SoundWrite(uint32 address, uint8 value) {
	if (address & 0x10000) {
		/* dummy read */
		N106SoundRead(address & 0xffff);
	} else {
		if (address == 0x4800)
			N106SoundWriteData(address, value);
		else if (address == 0xf800)
			N106SoundWriteAddr(address, value);
	}
}

void N106SoundReset(void) {
	int i;

	apu_t *apu;
	apu = apu_getcontext ();

	memset (&apu->n106s, 0, sizeof(N106SOUND));
	for (i = 0; i < 8; i++) {
		apu->n106s.ch[i].tlen = 0x10 << 18;
		apu->n106s.ch[i].logvol = LinearToLog(0);
	}

	apu->n106s.addressauto = 1;
	apu->n106s.chinuse = 8;
	apu->n106s.cps = DivFix(NES_BASECYCLES, 45 * SAMPLE_RATE, 20);
}

