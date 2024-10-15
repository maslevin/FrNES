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
#include "cpu.h"
#include "ppu.h"

#include "input_recorder.h"

#include "pNesX.h"
#include "Mapper.h"
#include "pNesX_System.h"
#include "profile.h"
#include "pNesX_System_DC.h"
#include "pNesX_Sound_APU.h"
#include "GUI_SystemPage.h"
#include "aica_fw.h"

/*-------------------------------------------------------------------*/
/*  NES resources                                                    */
/*-------------------------------------------------------------------*/

#define __ALIGN32__		__attribute__ ((aligned (32)))

extern unsigned char HALT;

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

/*-------------------------------------------------------------------*/
/*  APU and Pad resources                                            */
/*-------------------------------------------------------------------*/

//32 - byte aligned sound output buffer
#define SAMPLE_BUFFER_LENGTH 2940
__ALIGN32__ uint16 sample_buffer[SAMPLE_BUFFER_LENGTH];

#define SAMPLES_PER_FRAME 736

/* APU Register */
unsigned char APU_Reg[ 0x18 ];

uint32 num_bytes = 5880;
uint32 firmware_size = 3292;

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
	// Initialize Sound AICA program and ring buffers, and apu emulator
	// Initialize pNesX
	/*
	if (options.opt_SoundEnabled) {
		// Start Sound Emu
		DC_SoundInit();
		timer_spin_sleep(40);
		*start = 0xFFFFFFFF;
	}
	*/

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
	if (!ReadRom( filepath, filesize ))
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

	/*-------------------------------------------------------------------*/
	/*  Initialize resources                                             */
	/*-------------------------------------------------------------------*/
	// Reset frame skip and frame count
	FrameSkip = options.opt_FrameSkip;
	FrameCnt = 0;

	// Reset APU register
	memset( APU_Reg, 0, sizeof APU_Reg );

	// Reset joypad
	PAD1_Latch = PAD2_Latch = ExitCount = 0;
	PAD1_Bit = PAD2_Bit = 0;

	/*-------------------------------------------------------------------*/
	/*  Initialize PPU                                                   */
	/*-------------------------------------------------------------------*/
	WorkFrameIdx = 0;
	WorkFrame = WorkFrames[0];

	// TODO: figure out when to set up four screen from ROM header smartly
	if (NesHeader.byInfo1 & 0x01) {
		ppu_set_mirroring(PPU_MIRRORING_VERTICAL);
	} else {
		ppu_set_mirroring(PPU_MIRRORING_HORIZONTAL);
	}

	// reset CPU and PPU
	ppu_reset();

	mapper -> init();
	power();

	// Successful
	return 0;
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

	resetProfiling(PMCR_PIPELINE_FREEZE_BY_ICACHE_MISS_MODE, MAX_PROFILING_FUNCTIONS);
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

	maple_device_t* my_controller;
	cont_state_t* my_state = NULL;		

	// Main loop
	while ( 1 ) {
		if ( ExitCount > MAX_EXIT_COUNT )
			break;  // Quit

		last_frame_timestamp = frame_timestamp;

		run_frame();
		numEmulationFrames++;

		printf("Ran Frame [%li]\n", numEmulationFrames);

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

		//Grab data from controller 0
		my_controller = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
		if (my_controller != NULL) {
			my_state = (cont_state_t*)maple_dev_status(my_controller);
			if (my_state -> buttons & CONT_START) {
				break;
			}
		}

/*
		if (HALT) {
			printf ("ERROR: System Halt - exiting emulation\n");
			break;
		}
*/
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

void new_frame() {
	pNesX_LoadFrame();

	// Switching of the buffer
	WorkFrameIdx++;
	WorkFrameIdx%=NUM_PVR_FRAMES;
	WorkFrame = WorkFrames[WorkFrameIdx];
}