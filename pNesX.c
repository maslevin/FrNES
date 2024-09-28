/*===================================================================*/
/*                                                                   */
/*  pNesX.c   : NES Emulator for PSX                                 */
/*                                                                   */
/*  1999/11/03  Racoon  New preparation                              */
/*  2001/12/22  ReGex   FrNES 0.60 Source Release                    */
/*                      Slightly reorganized from pNesX version      */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/

#include <kos.h>
#include "macros.h"

#include "cartridge.h"
#include "input_recorder.h"

#include "pNesX.h"
#include "Mapper.h"
#include "pNesX_System.h"
#include "profile.h"
#include "pNesX_System_DC.h"
#include "pNesX_Sound_APU.h"
#include "pNesX_PPU_DC.h"
#include "GUI_SystemPage.h"
#include "K6502.h"
#include "aica_fw.h"

/*-------------------------------------------------------------------*/
/*  NES resources                                                    */
/*-------------------------------------------------------------------*/

/* RAM */
unsigned char RAM[ RAM_SIZE ];

/* SRAM */
unsigned char SRAM[ SRAM_SIZE ];

/* ROM BANK ( 8Kb * 4 ) */
unsigned char *ROMBANK_LO; // 0x6000
unsigned char *ROMBANK0;
unsigned char *ROMBANK1;
unsigned char *ROMBANK2;
unsigned char *ROMBANK3;

/*-------------------------------------------------------------------*/
/*  PPU resources                                                    */
/*-------------------------------------------------------------------*/
#define __ALIGN32__		__attribute__ ((aligned (32)))

/* PPU RAM */
unsigned char PPURAM[ PPURAM_SIZE ];

extern unsigned char HALT;

/* PPU BANK ( 1Kb * 16 ) */
unsigned char *PPUBANK[ 16 ];

/* Sprite RAM */
unsigned char SPRRAM[ SPRRAM_SIZE ];

/* PPU Info Struct */
PPU_Info_t ppuinfo;

/* PPU Register */
unsigned char PPU_R1;
unsigned char PPU_R2;
unsigned char PPU_R3;
unsigned char PPU_R7;

/* Flag for PPU Address and Scroll Latch */
unsigned char PPU_Latch_Flag;

/* PPU Address */
uint16 PPU_Temp;

/* The increase value of the PPU Address */
uint16 PPU_Increment;

/* Current Scanline */
uint16 PPU_Scanline;

//32 - byte aligned sound output buffer
#define SAMPLE_BUFFER_LENGTH 2940
__ALIGN32__ uint16 sample_buffer[SAMPLE_BUFFER_LENGTH];

#define SAMPLES_PER_FRAME 736

/* Sprite #0 Scanline Hit Position */
int SpriteJustHit;

/* Flag every odd cycle */
bool odd_cycle;

/*-------------------------------------------------------------------*/
/*  Display and Others resouces                                      */
/*-------------------------------------------------------------------*/
float frames_per_second;

/* Frame Skip */
uint16 FrameSkip;
uint16 FrameCnt;

/* AutoFrameskip */
int32 Auto_Frames;

/* Display Buffer */
VQ_Texture* WorkFrame;
uint16 WorkFrameIdx;
VQ_Texture* WorkFrames[];
__ALIGN32__ uint8 codebook[CODEBOOK_SIZE];

/* Table for Mirroring */
unsigned char PPU_MirrorTable[][ 4 ] = {
  { NAME_TABLE0, NAME_TABLE0, NAME_TABLE1, NAME_TABLE1 },
  { NAME_TABLE0, NAME_TABLE1, NAME_TABLE0, NAME_TABLE1 },
  { NAME_TABLE0, NAME_TABLE1, NAME_TABLE2, NAME_TABLE3 },
  { NAME_TABLE0, NAME_TABLE0, NAME_TABLE0, NAME_TABLE0 },
  { NAME_TABLE1, NAME_TABLE1, NAME_TABLE1, NAME_TABLE1 }
};

/*-------------------------------------------------------------------*/
/*  APU and Pad resources                                            */
/*-------------------------------------------------------------------*/

/* APU Register */
unsigned char APU_Reg[ 0x18 ];

uint32 num_bytes = 5880;
uint32 firmware_size = 3292;

//void* sample_buffer = 0x7C002000;
static volatile unsigned long *position  = (volatile unsigned long*)(0x10014 + 0xa0800000);
static volatile unsigned long *start  = (volatile unsigned long*)(0x10000 + 0xa0800000);

uint16 num_writes;
uint16 on_frame;

uint32 sound_address;
uint32 current_address;
uint32 last_pos;
uint32 this_pos;
uint32 last_write;
uint32 last_buffer;

/* Pad data */
uint32 PAD1_Latch;
uint32 PAD2_Latch;
uint32 ExitCount;
uint32 PAD1_Bit;
uint32 PAD2_Bit;

uint32 PollCount;
uint32 PollSkip;

void DC_SoundInit() {
	sound_address = 0;
	current_address = 0x11000;
	last_pos = 0;
	this_pos = 0;
	last_write = 0;
	last_buffer = 0;

	memset(sample_buffer, 0, num_bytes);

	spu_memload(current_address, (uint8 *)sample_buffer, num_bytes);

	*position = 0;

	spu_disable();
	spu_memload(0, aica_fw, 3292);
	spu_enable();

	audio_initialize();
}

void pNesX_DoSpu() {
	startProfiling(4);
	uint32 this_buffer;

	this_pos = *position;

	if (this_pos < 736)
		this_buffer = 0;
	else
		this_buffer = 1;

	if (last_buffer != this_buffer) {
		audio_do_frame(sample_buffer, 736);

		if (this_buffer == 0)
			spu_memload(0x115C0, (uint8 *)sample_buffer, 1472);
		else
			spu_memload(0x11000, (uint8 *)sample_buffer, 1472);

		last_buffer = this_buffer;
	}
	endProfiling(4);
}

/*===================================================================*/
/*                                                                   */
/*                pNesX_Init() : Initialize pNesX                    */
/*                                                                   */
/*===================================================================*/
void pNesX_Init() {
	// Initialize 6502
	K6502_Init();

	// Initialize Sound AICA program and ring buffers, and apu emulator
	// Initialize pNesX
	if (options.opt_SoundEnabled) {
		// Start Sound Emu
		DC_SoundInit();
		timer_spin_sleep(40);
		*start = 0xFFFFFFFF;
	}

	K6502_Set_Int_Wiring( 1, 1 ); 

	// Call mapper initialization function - important that this comes before mapper
	// init, for expansion audio
	mapper -> init();

	// Reset CPU and prepare to run program
	K6502_Reset();

	// Calculate how the output screen should appear based on clipping and stretching parameters
	calculateOutputScreenGeometry();
}

/*===================================================================*/
/*                                                                   */
/*                pNesX_Fin() : Completion treatment                 */
/*                                                                   */
/*===================================================================*/
void pNesX_Fin() {
	if (options.opt_SoundEnabled) {
		spu_shutdown();
	}

	audio_shutdown();

#ifdef DEBUG
	EndTracing();
#endif
}


/*===================================================================*/
/*                                                                   */
/*                  pNesX_Load() : Load a cassette                   */
/*  Parameters                                                       */
/*    const char *pszFileName                                        */
/*      File name of ROM image                                       */
/*                                                                   */
/*  Return values                                                    */
/*     0 : It was finished normally.                                 */
/*    -1 : An error occurred.                                        */
/*                                                                   */
/*  Remarks                                                          */
/*    Read a ROM image in the memory.                                */
/*    Reset pNesX.                                                   */
/*                                                                   */
/*===================================================================*/
int pNesX_Load( const char *filepath, uint32 filesize ) {
	if ( !ReadRom( filepath, filesize ) < 0 )
		return -1;

	if ( pNesX_Reset() < 0 )
		return -1;

	return 0;
}

/*===================================================================*/
/*                                                                   */
/*                 pNesX_Reset() : Reset pNesX                       */
/*                                                                   */
/*  Reset pNesX                                                      */
/*                                                                   */
/*  Return values                                                    */
/*     0 : Normally                                                  */
/*    -1 : Non support mapper                                        */
/*                                                                   */
/*  Remarks                                                          */
/*    Initialize Resources, PPU and Mapper.                          */
/*    Reset CPU.                                                     */
/*                                                                   */
/*===================================================================*/
int pNesX_Reset() {
	// TODO : make ROM loading and resetting separated
	int nIdx;

	// Get information on the ROM
	if (NesHeader.byInfo1 & 0x08) {
		ROM_Mirroring = MIRRORING_FOUR_SCREEN;
	} else if (NesHeader.byInfo1 & 0x01) {
		ROM_Mirroring = MIRRORING_VERTICAL;
	} else {
		ROM_Mirroring = MIRRORING_HORIZONTAL;
	}

	ROM_SRAM = NesHeader.byInfo1 & 2;
	ROM_Trainer = NesHeader.byInfo1 & 4;
	ROM_FourScr = NesHeader.byInfo1 & 8;

	/*-------------------------------------------------------------------*/
	/*  Initialize resources                                             */
	/*-------------------------------------------------------------------*/

	// Clear RAM
	memset( RAM, 0, sizeof RAM );

	// Clear PPU RAM
	memset( PPURAM, 0, PPURAM_SIZE );

	// Clear SPR RAM
	memset( SPRRAM, 0, SPRRAM_SIZE );

	// Clear SRAM
	memset( SRAM, 0, sizeof SRAM );

	// Reset frame skip and frame count
	FrameSkip = options.opt_FrameSkip;
	FrameCnt = 0;

	PollSkip = 6;
	PollCount = 0;

	// Reset APU register
	memset( APU_Reg, 0, sizeof APU_Reg );

	// Reset joypad
	PAD1_Latch = PAD2_Latch = ExitCount = 0;
	PAD1_Bit = PAD2_Bit = 0;

	/*-------------------------------------------------------------------*/
	/*  Initialize PPU                                                   */
	/*-------------------------------------------------------------------*/
	pNesX_SetupPPU();
	WorkFrameIdx = 0;
	WorkFrame = WorkFrames[0];

	/*-------------------------------------------------------------------*/
	/*  Initialize Mapper                                                */
	/*-------------------------------------------------------------------*/

	// Get Mapper Table Index
	printf("Looking for Support for Mapper [%i]\n", MapperNo);
	for ( nIdx = 0; Mappers[ nIdx ].nMapperNo != -1; ++nIdx ) {
		if ( Mappers[ nIdx ].nMapperNo == MapperNo )
			break;
	}

	if (( Mappers[ nIdx ].nMapperNo == -1 ) || (Mappers[nIdx].mapper == NULL)) {
		// Non support mapper
		return -1;
	} else {
		printf("Mapper [%i] Supported\n", Mappers[ nIdx ].nMapperNo);
		mapper = Mappers[nIdx].mapper;
	}

	// Successful
	return 0;
}

/*===================================================================*/
/*                                                                   */
/*                pNesX_SetupPPU() : Initialize PPU                  */
/*                                                                   */
/*===================================================================*/
void pNesX_SetupPPU() {
	int nPage;

	// Clear PPU and Sprite Memory
	memset( PPURAM, 0, sizeof PPURAM );
	memset( SPRRAM, 0, sizeof SPRRAM );

	// Reset PPU Register
	ppuinfo.PPU_R0 = PPU_R1 = PPU_R2 = PPU_R3 = PPU_R7 = 0;

	// Reset latch flag
	PPU_Latch_Flag = 0;

	// Reset PPU address
	ppuinfo.PPU_Addr = 0;
	PPU_Temp = 0;

	// Reset scanline
	ppuinfo.PPU_Scanline = 0;

	// Reset hit position of sprite #0 
	// SpriteHitPos = -1;

	// Reset information on PPU_R0
	PPU_Increment = 1;
	ppuinfo.PPU_SP_Height = 8;

	// Reset PPU banks
	// This isn't accurate because the NES doesn't actually have 16kB of PPU memory
	// I will refactor this to be more accurate with a more accurate memory map
	for ( nPage = 0; nPage < 8; ++nPage )
		PPUBANK[ nPage ] = &PPURAM[ nPage * 0x400 ];
	PPUBANK[ 8 ] = &PPURAM[ 8 * 0x400 ];
	PPUBANK[ 9 ] = &PPURAM[ 9 * 0x400 ];
	PPUBANK[ 10 ] = &PPURAM[ 10 * 0x400 ];
	PPUBANK[ 11 ] = &PPURAM[ 11 * 0x400 ];
	PPUBANK[ 12 ] = &PPURAM[ 12 * 0x400 ];
	PPUBANK[ 13 ] = &PPURAM[ 13 * 0x400 ];
	PPUBANK[ 14 ] = &PPURAM[ 14 * 0x400 ];
	PPUBANK[ 15 ] = &PPURAM[ 15 * 0x400 ];

	/* Mirroring of Name Table */
	pNesX_Mirroring( ROM_Mirroring );
}

/*===================================================================*/
/*                                                                   */
/*       pNesX_Mirroring() : Set up a Mirroring of Name Table        */
/*                                                                   */
/*                                                                   */
/*  Parameters                                                       */
/*    int nType          (Read)                                      */
/*      Mirroring Type                                               */
/*        0 : Horizontal                                             */
/*        1 : Vertical                                               */
/*        2 : Four Screen                                            */
/*        3 : One Screen 0x2000                                      */
/*        4 : One Screen 0x2400                                      */
/*                                                                   */
/*===================================================================*/
void pNesX_Mirroring( int nType ) {
	PPUBANK[ NAME_TABLE0 ] = &PPURAM[ PPU_MirrorTable[ nType ][ 0 ] * 0x400 ];
	PPUBANK[ NAME_TABLE1 ] = &PPURAM[ PPU_MirrorTable[ nType ][ 1 ] * 0x400 ];
	PPUBANK[ NAME_TABLE2 ] = &PPURAM[ PPU_MirrorTable[ nType ][ 2 ] * 0x400 ];
	PPUBANK[ NAME_TABLE3 ] = &PPURAM[ PPU_MirrorTable[ nType ][ 3 ] * 0x400 ];
}

void pNesX_Mirroring_Manual (int bank1, int bank2, int bank3, int bank4) {
	PPUBANK[ NAME_TABLE0 ] = &PPURAM[ (NAME_TABLE0 + bank1) * 0x400 ];
	PPUBANK[ NAME_TABLE1 ] = &PPURAM[ (NAME_TABLE0 + bank2) * 0x400 ];
	PPUBANK[ NAME_TABLE2 ] = &PPURAM[ (NAME_TABLE0 + bank3) * 0x400 ];
	PPUBANK[ NAME_TABLE3 ] = &PPURAM[ (NAME_TABLE0 + bank4) * 0x400 ];
}

#define NUM_FPS_SAMPLES 60

#define MAX_PROFILING_FUNCTIONS 6

/*===================================================================*/
/*                                                                   */
/*              pNesX_Main() : The main loop of pNesX                */
/*                                                                   */
/*===================================================================*/
__attribute__ ((hot)) void pNesX_Main() {
	pNesX_Init();

	resetProfiling(MAX_PROFILING_FUNCTIONS);
	setProfilingFunctionName(0, "K6502_Step");
	setProfilingFunctionName(1, "handle_dmc_synchronization");
	setProfilingFunctionName(2, "pNesX_DrawLine_BG_C");
	setProfilingFunctionName(3, "pNesX_DrawLine_Spr_C");
	setProfilingFunctionName(4, "pNesX_DoSpu");
	setProfilingFunctionName(5, "audio_sync_apu_registers");

	frames_per_second = 0;

	struct timespec ts;
	uint64 last_frame_timestamp = 0;	
	uint64 frame_timestamp = 0;
	double last_frames_per_second[NUM_FPS_SAMPLES];
	uint32 last_frames_per_second_index = 0;	
	if (options.opt_ShowFrameRate) {
		clock_gettime(CLOCK_MONOTONIC, &ts);		
		frame_timestamp = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
	}

	odd_cycle = false;

	// Main loop
	while ( 1 ) {
		if ( ExitCount > MAX_EXIT_COUNT )
			break;  // Quit

		last_frame_timestamp = frame_timestamp;

		pNesX_Cycle();
		odd_cycle = !odd_cycle;
		numEmulationFrames++;

		if (options.opt_ShowFrameRate) {
			clock_gettime(CLOCK_MONOTONIC, &ts);
			frame_timestamp = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
			last_frames_per_second[last_frames_per_second_index] = 1e9 / (double)(frame_timestamp - last_frame_timestamp);
			last_frames_per_second_index++;

			if (last_frames_per_second_index % NUM_FPS_SAMPLES == 0) {
				double averageFramesPerSecond = 0.0f;
				for (uint32 fpsIndex = 0; fpsIndex < NUM_FPS_SAMPLES; fpsIndex++) {
					averageFramesPerSecond += last_frames_per_second[fpsIndex];
				}
				frames_per_second = averageFramesPerSecond / (float)NUM_FPS_SAMPLES;
				last_frames_per_second_index = 0;
			}
		}

		if (HALT) {
			printf ("ERROR: System Halt - exiting emulation\n");
			break;
		}
/*
		if ( *opt_AutoFrameSkip ) {
			if ( Auto_Frames > 0) {
				FrameCnt = 1;
			} else {
				FrameCnt = 0;
			}
		}
*/		
	}

	printf("Exiting Emulator Main Loop\n");

	pNesX_Fin();
	printProfilingReport();
}

#define CYCLES_PER_LINE 113
#define HSYNC_CYCLES 28

__attribute__ ((hot)) void handle_dmc_synchronization(uint32 cycles) {
	startProfiling(1);
	if (audio_sync_dmc_registers(cycles)) {
		K6502_DoIRQ();
	}
	endProfiling(1);
}

/*===================================================================*/
/*                                                                   */
/*              pNesX_Cycle() : The loop of emulation                */
/*                                                                   */
/*===================================================================*/
__attribute__ ((hot)) void pNesX_Cycle() {
	SpriteJustHit = SPRITE_HIT_SENTINEL;

	//Set the PPU adress to the buffered value
	pNesX_StartFrame();

	// Dummy scanline -1 or 261;
	K6502_Step(1);
	PPU_R2 &= ~(R2_IN_VBLANK | R2_HIT_SP);
	if (odd_cycle) {	
		K6502_Step(CYCLES_PER_LINE);	
		handle_dmc_synchronization(CYCLES_PER_LINE + 1);
	} else {
		K6502_Step(CYCLES_PER_LINE - 1);	
		handle_dmc_synchronization(CYCLES_PER_LINE);
	}

	// Scanline 0-239
	for (ppuinfo.PPU_Scanline = 0; ppuinfo.PPU_Scanline <= 260; ppuinfo.PPU_Scanline++) {
		uint16 cpu_cycles_to_emulate = CYCLES_PER_LINE;
		uint16 hsync_cycles = HSYNC_CYCLES;
		if ((ppuinfo.PPU_Scanline + 1) % 3 == 0) {
			cpu_cycles_to_emulate += 2;
			hsync_cycles += 1;
		}

		switch (ppuinfo.PPU_Scanline) {
			case 0 ... 239: {
				pNesX_DrawLine();

				if (SpriteJustHit == ppuinfo.PPU_Scanline) {
					// Set the sprite hit flag
					PPU_R2 |= R2_HIT_SP;

					// NMI is required if there is necessity
					if ( ( ppuinfo.PPU_R0 & R0_NMI_SP ) && ( PPU_R1 & R1_SHOW_SP ) )
						NMI_REQ;
				}

				K6502_Step(hsync_cycles);
				mapper -> hsync();				
				K6502_Step(cpu_cycles_to_emulate - hsync_cycles);

				handle_dmc_synchronization(cpu_cycles_to_emulate);

				if ((PPU_R1 & 0x10) || (PPU_R1 & 0x08)) {
					ppuinfo.PPU_Addr = (ppuinfo.PPU_Addr & 0xFBE0) | (PPU_Temp & 0x041F);

					if ((ppuinfo.PPU_Addr & 0x7000) == 0x7000) {  /* is subtile y offset == 7? */
						ppuinfo.PPU_Addr &= 0x8FFF; /* subtile y offset = 0 */

						if ((ppuinfo.PPU_Addr & 0x03E0) == 0x03A0) { /* name_tab line == 29? */
							ppuinfo.PPU_Addr ^= 0x0800;  /* switch v nametables (bit 11) */
							ppuinfo.PPU_Addr &= 0xFC1F;  /* name_tab line = 0 */
						} else {
							if ((ppuinfo.PPU_Addr & 0x03E0) == 0x03E0) { /* line == 31? */
								ppuinfo.PPU_Addr &= 0xFC1F;  /* name_tab line = 0 */
							} else {
								ppuinfo.PPU_Addr += 0x0020;
							}
						}
					} else {
						ppuinfo.PPU_Addr += 0x1000; /* next subtile y offset */
					}
				}
			} break;

			case 240: {
				pNesX_LoadFrame();

				// Switching of the buffer
				WorkFrameIdx++;
				WorkFrameIdx%=NUM_PVR_FRAMES;
				WorkFrame = WorkFrames[WorkFrameIdx];

				K6502_Step(cpu_cycles_to_emulate);
				handle_dmc_synchronization(cpu_cycles_to_emulate);
				mapper -> hsync();
			} break;

			case 241: {
				K6502_Step(5);
				pNesX_VSync();
				mapper -> vsync();
				K6502_Step(cpu_cycles_to_emulate - 5);
				handle_dmc_synchronization(cpu_cycles_to_emulate);
				mapper -> hsync();
			} break;

			case 242 ... 260 : {
				K6502_Step(cpu_cycles_to_emulate);
				handle_dmc_synchronization(cpu_cycles_to_emulate);
				mapper -> hsync();
			} break;
		}
	}

	if (options.opt_SoundEnabled) {
		pNesX_DoSpu();
	}
	audio_sync_apu_registers();
}

__attribute__ ((hot)) void pNesX_VSync() {
	// Set a V-Blank flag
	PPU_R2 |= R2_IN_VBLANK;

	pNesX_PadState( &PAD1_Latch, &PAD2_Latch, &ExitCount );

	// NMI on V-Blank
	if ( ppuinfo.PPU_R0 & R0_NMI_VB )
		NMI_REQ;
}