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

#include "pNesX.h"
#include "pNesX_System.h"
#include "pNesX_Mapper.h"
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

/* ROM */
extern unsigned char *ROM;

/* ROM BANK ( 8Kb * 4 ) */
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
extern unsigned char pSprBuf[272];

extern unsigned char HALT;

/* Cartridge VROM */
extern unsigned char* VROM;
/* Cartridge VRAM */
extern unsigned char* VRAM;

/* PPU BANK ( 1Kb * 16 ) */
unsigned char *PPUBANK[ 16 ];

/* Sprite RAM */
unsigned char SPRRAM[ SPRRAM_SIZE ];

/* PPU Info Struct */
PPU_Info ppuinfo;

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
__ALIGN32__ uint16 sample_buffer[2940];

/* Scanline Table */
unsigned char PPU_ScanTable[ 263 ];

/* Sprite #0 Scanline Hit Position */
int SpriteHitPos;
int SpriteJustHit;

/*-------------------------------------------------------------------*/
/*  Display and Others resouces                                      */
/*-------------------------------------------------------------------*/
/* Frame Skip */
uint16 FrameSkip;
uint16 FrameCnt;

/* AutoFrameskip */
int32 Auto_Frames;

/* Display Buffer */
VQ_Texture* WorkFrame;
uint16 WorkFrameIdx;

/* Palette Table */
uint16 PalTable[ 32 ];

/* Table for Mirroring */
unsigned char PPU_MirrorTable[][ 4 ] =
{
//Original Mirroring Layout
/*  { NAME_TABLE0, NAME_TABLE0, NAME_TABLE1, NAME_TABLE1 },
  { NAME_TABLE0, NAME_TABLE1, NAME_TABLE0, NAME_TABLE1 },
  { NAME_TABLE1, NAME_TABLE1, NAME_TABLE1, NAME_TABLE1 },
  { NAME_TABLE0, NAME_TABLE0, NAME_TABLE0, NAME_TABLE0 }*/
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

/*-------------------------------------------------------------------*/
/*  Profiling Information ->  May 6th, 2001                          */
/*-------------------------------------------------------------------*/
//uint32 NES_CPU_Natural_Speed = 1789773;
//uint32 pNesX_Last_Clocks;
//uint32 pNesX_CPU_Clocks;
//uint32 NES_CPU_Natural_Frames = 60;
//uint32 pNesX_Last_Frames;
//uint32 pNesX_Frames;

/*-------------------------------------------------------------------*/
/*  Mapper Function                                                  */
/*-------------------------------------------------------------------*/

/* Initialize Mapper */
void (*MapperInit)();
/* Write to Mapper */
void (*MapperWrite)( uint16 wAddr, unsigned char byData );
/* Callback at VSync */
void (*MapperVSync)();
/* Callback at HSync */
void (*MapperHSync)();

/*-------------------------------------------------------------------*/
/*  ROM information                                                  */
/*-------------------------------------------------------------------*/

/* .nes File Header */
struct NesHeader_tag NesHeader;

/* Mapper Number */
unsigned char MapperNo;

/* Mirroring 0:Horizontal 1:Vertical */
unsigned char ROM_Mirroring;
/* It has SRAM */
unsigned char ROM_SRAM;
/* It has Trainer */
unsigned char ROM_Trainer;
/* Four screen VRAM  */
unsigned char ROM_FourScr;

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
}

/*===================================================================*/
/*                                                                   */
/*                pNesX_Init() : Initialize pNesX                    */
/*                                                                   */
/*===================================================================*/
void pNesX_Init()
{
/*
 *  Initialize pNesX
 *
 *  Remarks
 *    Initialize K6502 and Scanline Table.
 */
  int nIdx;

  // Initialize 6502
  K6502_Init();

  // Initialize Scanline Table
  // MS - I don't think this is required anymore, refactor it out
  for ( nIdx = 0; nIdx < 263; ++nIdx )
  {
    if ( nIdx < SCAN_ON_SCREEN_START )
      PPU_ScanTable[ nIdx ] = SCAN_TOP_OFF_SCREEN;
    else
    if ( nIdx < SCAN_BOTTOM_OFF_SCREEN_START )
      PPU_ScanTable[ nIdx ] = SCAN_ON_SCREEN;
    else
    if ( nIdx < SCAN_UNKNOWN_START )
      PPU_ScanTable[ nIdx ] = SCAN_BOTTOM_OFF_SCREEN;
    else
    if ( nIdx < SCAN_VBLANK_START )
      PPU_ScanTable[ nIdx ] = SCAN_UNKNOWN;
    else
      PPU_ScanTable[ nIdx ] = SCAN_VBLANK;
  }
}

/*===================================================================*/
/*                                                                   */
/*                pNesX_Fin() : Completion treatment                 */
/*                                                                   */
/*===================================================================*/
void pNesX_Fin()
{
/*
 *  Completion treatment
 *
 *  Remarks
 *    Release resources
 */

  // Release a memory for ROM
//  pNesX_ReleaseRom();
	audio_shutdown();
}


/*===================================================================*/
/*                                                                   */
/*                  pNesX_Load() : Load a cassette                   */
/*                                                                   */
/*===================================================================*/
int pNesX_Load( const char *filepath, uint32 filesize )
{
/*
 *  Load a cassette
 *
 *  Parameters
 *    const char *pszFileName            (Read)
 *      File name of ROM image
 *
 *  Return values
 *     0 : It was finished normally.
 *    -1 : An error occurred.
 *
 *  Remarks
 *    Read a ROM image in the memory. 
 *    Reset pNesX.
 */

  // Release a memory for ROM
//  pNesX_ReleaseRom();

  // Read a ROM image in the memory
  if ( pNesX_ReadRom( filepath, filesize ) < 0 )
    return -1;

  // Reset pNesX
  if ( pNesX_Reset() < 0 )
    return -1;

  // Successful
  return 0;
}

/*===================================================================*/
/*                                                                   */
/*                 pNesX_Reset() : Reset pNesX                       */
/*                                                                   */
/*===================================================================*/
int pNesX_Reset()
{
/*
 *  Reset pNesX
 *
 *  Return values
 *     0 : Normally
 *    -1 : Non support mapper
 *
 *  Remarks
 *    Initialize Resources, PPU and Mapper.
 *    Reset CPU.
 */

  int nIdx;

  /*-------------------------------------------------------------------*/
  /*  Get information on the cassette                                  */
  /*-------------------------------------------------------------------*/

  // Get Mapper Number
  MapperNo = NesHeader.byInfo1 >> 4;

  // Check bit counts of Mapper No.
  for ( nIdx = 4; nIdx < 8 && NesHeader.byReserve[ nIdx ] == 0; ++nIdx )
    ;

  if ( nIdx == 8 )
  {
    // Mapper Number is 8bits
    MapperNo |= ( NesHeader.byInfo2 & 0xf0 );
  }

  // Get information on the ROM
  if (NesHeader.byInfo1 & 0x08)
	  ROM_Mirroring = 2; // 4 screen mirroring
  else
  if (NesHeader.byInfo1 & 0x01)
	  ROM_Mirroring = 1; // Vertical mirroring
  else
      ROM_Mirroring = 0; // Horizontal mirroring

  ROM_SRAM = NesHeader.byInfo1 & 2;
  ROM_Trainer = NesHeader.byInfo1 & 4;
  ROM_FourScr = NesHeader.byInfo1 & 8;

  /*-------------------------------------------------------------------*/
  /*  Initialize resources                                             */
  /*-------------------------------------------------------------------*/

  // Clear RAM
  memset( RAM, 0, sizeof RAM );

  // Clear SRAM
  memset( SRAM, 0, sizeof SRAM );

  // Reset frame skip and frame count
  FrameSkip = *opt_FrameSkip;
  FrameCnt = 0;

  PollSkip = 6;
  PollCount = 0;

  // Reset palette table
  memset( PalTable, 0, sizeof PalTable );

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
  WorkFrame = PVR_NESScreen1_Offset;

  /*-------------------------------------------------------------------*/
  /*  Initialize Mapper                                                */
  /*-------------------------------------------------------------------*/

  // Get Mapper Table Index
  printf("Looking for Support for Mapper [%i]\n", MapperNo);
  for ( nIdx = 0; MapperTable[ nIdx ].nMapperNo != -1; ++nIdx )
  {
    if ( MapperTable[ nIdx ].nMapperNo == MapperNo )
      break;
  }

  if (( MapperTable[ nIdx ].nMapperNo == -1 ) || (MapperTable[nIdx].pMapperInit == NULL))
  {
    // Non support mapper
    return -1;
  } else {
	printf("Mapper [%i] Supported\n", MapperTable[ nIdx ].nMapperNo);
  }

  // Set up a mapper initialization function
  MapperTable[ nIdx ].pMapperInit();

  /*-------------------------------------------------------------------*/
  /*  Reset CPU                                                        */
  /*-------------------------------------------------------------------*/

  K6502_Reset();
/*
  pNesX_CPU_Clocks = 0;
  pNesX_Last_Clocks = 0;
  pNesX_Frames = 0;
  pNesX_Last_Frames = 0;*/

  // Successful
  return 0;
}

/*===================================================================*/
/*                                                                   */
/*                pNesX_SetupPPU() : Initialize PPU                  */
/*                                                                   */
/*===================================================================*/
void pNesX_SetupPPU()
{
/*
 *  Initialize PPU
 *
 */
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
  SpriteHitPos = -1;

  // Reset information on PPU_R0
  PPU_Increment = 1;
  ppuinfo.PPU_SP_Height = 8;

  // Reset PPU banks
  for ( nPage = 0; nPage < 8; ++nPage )
    PPUBANK[ nPage ] = &PPURAM[ nPage * 0x400 ];
  PPUBANK[ 8 ] = &PPURAM[ 8 * 0x400 ];
  PPUBANK[ 9 ] = &PPURAM[ 8 * 0x400 ];
  PPUBANK[ 10 ] = &PPURAM[ 8 * 0x400 ];
  PPUBANK[ 11 ] = &PPURAM[ 8 * 0x400 ];
  PPUBANK[ 12 ] = &PPURAM[ 8 * 0x400 ];
  PPUBANK[ 13 ] = &PPURAM[ 8 * 0x400 ];
  PPUBANK[ 14 ] = &PPURAM[ 8 * 0x400 ];
  PPUBANK[ 15 ] = &PPURAM[ 8 * 0x400 ];

  /* Mirroring of Name Table */
  pNesX_Mirroring( ROM_Mirroring );
}

/*===================================================================*/
/*                                                                   */
/*       pNesX_Mirroring() : Set up a Mirroring of Name Table        */
/*                                                                   */
/*===================================================================*/
void pNesX_Mirroring( int nType )
{
/*
 *  Set up a Mirroring of Name Table
 *
 *  Parameters
 *    int nType          (Read)
 *      Mirroring Type
 *        0 : Horizontal
 *        1 : Vertical
 *        2 : One Screen 0x2000
 *        3 : One Screen 0x2400
 */
/* Now it's entirely different
0 : Horizontal
1 : Vertical
2 : 4 Screen
*/
	PPUBANK[ NAME_TABLE0 ] = &PPURAM[ PPU_MirrorTable[ nType ][ 0 ] * 0x400 ];
	PPUBANK[ NAME_TABLE1 ] = &PPURAM[ PPU_MirrorTable[ nType ][ 1 ] * 0x400 ];
	PPUBANK[ NAME_TABLE2 ] = &PPURAM[ PPU_MirrorTable[ nType ][ 2 ] * 0x400 ];
	PPUBANK[ NAME_TABLE3 ] = &PPURAM[ PPU_MirrorTable[ nType ][ 3 ] * 0x400 ];
}

void pNesX_Mirroring_Manual (int bank1, int bank2, int bank3, int bank4)
{
	PPUBANK[ NAME_TABLE0 ] = &PPURAM[ (NAME_TABLE0 + bank1) * 0x400 ];
	PPUBANK[ NAME_TABLE1 ] = &PPURAM[ (NAME_TABLE0 + bank2) * 0x400 ];
	PPUBANK[ NAME_TABLE2 ] = &PPURAM[ (NAME_TABLE0 + bank3) * 0x400 ];
	PPUBANK[ NAME_TABLE3 ] = &PPURAM[ (NAME_TABLE0 + bank4) * 0x400 ];
}

/*
static void
timer_handler(irq_t source, irq_context_t *context, void* data)
{
	Auto_Frames++;
}
*/

/*===================================================================*/
/*                                                                   */
/*              pNesX_Main() : The main loop of pNesX                */
/*                                                                   */
/*===================================================================*/
void pNesX_Main()
{
	pNesX_Init();

	// Initialize pNesX
	if (*opt_SoundEnabled) {
		// Start Sound Emu
		DC_SoundInit();

		timer_spin_sleep(40);

		*start = 0xFFFFFFFF;
	}

/*
    if ( *opt_AutoFrameSkip ) {
		//Set TMU2 at 60Hz, using interrupts
		timer_prime(TMU2, 60, 1);
		irq_set_handler(EXC_TMU2_TUNI2, timer_handler, NULL);
		timer_start(TMU2);
		Auto_Frames = 1;
	}
*/

	// Main loop
	while ( 1 ) {
		/*-------------------------------------------------------------------*/
		/*  To the menu screen                                               */
		/*-------------------------------------------------------------------*/
		if ( ExitCount > 100 )
			break;  // Quit

		/*-------------------------------------------------------------------*/
		/*  Manage AutoFrameSkip                                             */
		/*-------------------------------------------------------------------*/
		if ( *opt_AutoFrameSkip ) {
			Auto_Frames--;
		}

		/*-------------------------------------------------------------------*/
		/*  Manage NES emulation                                             */
		/*-------------------------------------------------------------------*/
		pNesX_Cycle();

		if (HALT) {
			printf ("ERROR: System Halt - exiting emulation\n");
			break;
		}

		/*-------------------------------------------------------------------*/
		/*  Manage AutoFrameSkip                                             */
		/*-------------------------------------------------------------------*/
		if ( *opt_AutoFrameSkip ) {
			if ( Auto_Frames > 0) {
				FrameCnt = 1;
			} else {
				FrameCnt = 0;
			}
		}
	}

/*
	if (*opt_AutoFrameSkip) {
		timer_stop(TMU2);
		irq_set_handler(EXC_TMU2_TUNI2, NULL, NULL);
	}
*/

	if (*opt_SoundEnabled) {
		spu_shutdown();
	}

	// Completion treatment
	pNesX_Fin();
}

#define CYCLES_PER_LINE 113

void handle_dmc_synchronization(uint32 cycles) {
	if (audio_sync_dmc_registers(cycles)) {
		K6502_DoIRQ();
	}
}

/*===================================================================*/
/*                                                                   */
/*              pNesX_Cycle() : The loop of emulation                */
/*                                                                   */
/*===================================================================*/
void pNesX_Cycle() {
	int do_scroll_setup;
	SpriteJustHit = 241;

	//Set the PPU adress to the buffered value
	pNesX_StartFrame();

	// Dummy scanline -1;
	K6502_Step(CYCLES_PER_LINE);
	handle_dmc_synchronization(CYCLES_PER_LINE);

	// Scanline 0-239
	for (ppuinfo.PPU_Scanline = 0; ppuinfo.PPU_Scanline < 240; ppuinfo.PPU_Scanline++)
	{
		do_scroll_setup = ((PPU_R1 & 0x10) || (PPU_R1 & 0x08));

		//Do Scroll Setup even if we aren't drawing the frame
		if (do_scroll_setup) {
			ppuinfo.PPU_Addr = (ppuinfo.PPU_Addr & 0xFBE0) | (PPU_Temp & 0x041F);
		}

		// Account for rendering even if we aren't drawing the frame
		if (do_scroll_setup)
		{
			if ((ppuinfo.PPU_Addr & 0x7000) == 0x7000) /* is subtile y offset == 7? */
			{
				ppuinfo.PPU_Addr &= 0x8FFF; /* subtile y offset = 0 */

				if ((ppuinfo.PPU_Addr & 0x03E0) == 0x03A0) /* name_tab line == 29? */
				{ 
					ppuinfo.PPU_Addr ^= 0x0800;  /* switch v nametables (bit 11) */
					ppuinfo.PPU_Addr &= 0xFC1F;  /* name_tab line = 0 */
				}
				else	
				{
					if((ppuinfo.PPU_Addr & 0x03E0) == 0x03E0) /* line == 31? */
					{
						ppuinfo.PPU_Addr &= 0xFC1F;  /* name_tab line = 0 */
					}
					else
					{
						ppuinfo.PPU_Addr += 0x0020;
					}
				}
			}
			else
			{
				ppuinfo.PPU_Addr += 0x1000; /* next subtile y offset */
			}
		}

		pNesX_DrawLine();

		if (SpriteJustHit == ppuinfo.PPU_Scanline) {
			// Set a sprite hit flag
			PPU_R2 |= R2_HIT_SP;

			// NMI is required if there is necessity
			if ( ( ppuinfo.PPU_R0 & R0_NMI_SP ) && ( PPU_R1 & R1_SHOW_SP ) )
				NMI_REQ;
		}

		K6502_Step(CYCLES_PER_LINE);
		handle_dmc_synchronization(CYCLES_PER_LINE);		
		MapperHSync();
	}

	pNesX_LoadFrame();
	if (FrameCnt == 0) {
        // Switching of the double buffer
        WorkFrameIdx = 1 - WorkFrameIdx;
		if (WorkFrameIdx == 0) {
			WorkFrame = PVR_NESScreen1_Offset;
		} else {
			WorkFrame = PVR_NESScreen2_Offset;
		}
	}

	// Scanline 240
	ppuinfo.PPU_Scanline = 240;	
	if (SpriteJustHit == ppuinfo.PPU_Scanline) {
		// Set a sprite hit flag
		PPU_R2 |= R2_HIT_SP;

		// NMI is required if there is necessity
		if ( ( ppuinfo.PPU_R0 & R0_NMI_SP ) && ( PPU_R1 & R1_SHOW_SP ) )
			NMI_REQ;
	}
	K6502_Step(CYCLES_PER_LINE);
	handle_dmc_synchronization(CYCLES_PER_LINE);	
	MapperHSync();
	if (!(*opt_AutoFrameSkip))
		FrameCnt = ( FrameCnt >= FrameSkip ) ? 0 : FrameCnt + 1;
	pNesX_VSync();
	MapperVSync();

	// Scanline 241
	ppuinfo.PPU_Scanline = 241;
	if (SpriteJustHit == ppuinfo.PPU_Scanline) {
		// Set a sprite hit flag
		PPU_R2 |= R2_HIT_SP;

		// NMI is required if there is necessity
		if ( ( ppuinfo.PPU_R0 & R0_NMI_SP ) && ( PPU_R1 & R1_SHOW_SP ) )
			NMI_REQ;
	}	
	K6502_Step(1);
	K6502_DoNMI();
	K6502_Step(CYCLES_PER_LINE - 1);
	handle_dmc_synchronization(CYCLES_PER_LINE);	
	MapperHSync();

	// Scanline 242-260
	for (ppuinfo.PPU_Scanline = 242; ppuinfo.PPU_Scanline <= 260; ppuinfo.PPU_Scanline++)
	{
		K6502_Step(CYCLES_PER_LINE);
		handle_dmc_synchronization(CYCLES_PER_LINE);		
		MapperHSync();
	}

	// 87 Cycles is the remainder of the cycles from doing ALL the scanlines on the screen at 133.33 CPU cycles per scanline
	K6502_Step(10);
	PPU_R2 ^= (R2_IN_VBLANK | R2_HIT_SP);
	K6502_Step(77);
	handle_dmc_synchronization(87);	

	if (*opt_SoundEnabled) {
		pNesX_DoSpu();
	}
	audio_sync_apu_registers();
}

void pNesX_VSync() {
	// Set a V-Blank flag
	PPU_R2 |= R2_IN_VBLANK;

	// Reset latch flag
	PPU_Latch_Flag = 0;

	pNesX_PadState( &PAD1_Latch, &PAD2_Latch, &ExitCount );

	// NMI on V-Blank
	if ( ppuinfo.PPU_R0 & R0_NMI_VB )
		NMI_REQ;
}