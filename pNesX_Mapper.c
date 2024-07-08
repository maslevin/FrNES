/*===================================================================*/
/*                                                                   */
/*  pNesX_Mapper.c : pNesX Mapper Function                           */
/*                                                                   */
/*  1999/11/03  Racoon  New preparation                              */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                      Mapper fixes + 7 + 9 initial drivers         */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/
#include "kos.h"
#include "macros.h"

#include "pNesX.h"
#include "pNesX_System.h"
#include "pNesX_Mapper.h"
#include "pNesX_System_DC.h"
#include "K6502.h"

/*-------------------------------------------------------------------*/
/*  Table of Mapper initialize function                              */
/*-------------------------------------------------------------------*/

struct MapperTable_tag MapperTable[] =
{
  { 0, Map0_Init },
  { 1, Map1_Init },
  { 2, Map2_Init },
  { 3, Map3_Init },
  { 4, Map4_Init },
  { 5, Map5_Init },
  { 6, NULL },
  { 7, Map7_Init }, // Battletoads -> RARE Mapper
  { 8, NULL },
  { 9, Map9_Init }, // Mike Tyson's Punchout!
  { 10, NULL },
  { 11, NULL },
  { 12, NULL },
  { 13, NULL },
  { 14, NULL },
  { 15, NULL },
  { 16, NULL },
  { 17, NULL }, 
  { 18, NULL },
  { 19, NULL },
  { 20, NULL },
  { 21, NULL },
  { 22, NULL },
  { 23, NULL },
  { 24, NULL },
  { 25, NULL },
  { 26, NULL },
  { 27, NULL },
  { 28, NULL },
  { 29, NULL },
  { 30, Map30_Init }, // Battle Kid 2
  { 31, NULL },
  { 32, NULL },
  { 33, NULL },
  { 34, NULL },
  { 35, NULL },
  { 36, NULL },
  { 37, NULL },
  { 38, NULL },
  { 39, NULL },
  { 40, NULL },  
  { 41, NULL },
  { 42, NULL },
  { 43, NULL },
  { 44, NULL },
  { 45, NULL },
  { 46, NULL },
  { 47, NULL },
  { 48, NULL },
  { 49, NULL },
  { 40, NULL },  
  { 41, NULL },
  { 42, NULL },
  { 43, NULL },
  { 44, NULL },
  { 45, NULL },
  { 46, NULL },
  { 47, NULL },
  { 48, NULL },
  { 49, NULL },
  { 40, NULL },  
  { 41, NULL },
  { 42, NULL },
  { 43, NULL },
  { 44, NULL },
  { 45, NULL },
  { 46, NULL },
  { 47, NULL },
  { 48, NULL },
  { 49, NULL },
  { 50, NULL },  
  { 51, NULL },
  { 52, NULL },
  { 53, NULL },
  { 54, NULL },
  { 55, NULL },
  { 56, NULL },
  { 57, NULL },
  { 58, NULL },
  { 59, NULL },    
  { 60, NULL },  
  { 61, NULL },
  { 62, NULL },
  { 63, NULL },
  { 64, NULL },
  { 65, NULL },
  { 66, NULL },
  { 67, NULL },
  { 68, NULL },
  { 69, NULL },
  { 70, NULL },  
  { 71, NULL },
  { 72, NULL },
  { 73, NULL },
  { 74, NULL },
  { 75, NULL },
  { 76, NULL },
  { 77, NULL },
  { 78, NULL },
  { 79, NULL },    
  { -1, NULL }
};

/*===================================================================*/
/*                                                                   */
/*                            Mapper 0                               */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 0                                              */
/*-------------------------------------------------------------------*/
void Map0_Init()
{
	int nPage;
	/* Initialize Mapper */
	MapperInit = Map0_Init;

	/* Write to Mapper */
	MapperWrite = Map0_Write;

	/* Read from Mapper */
	MapperRead = Map0_Read;

	/* Callback at VSync */
	MapperVSync = Map0_VSync;

	/* Callback at HSync */
	MapperHSync = Map0_HSync;

	/* Set ROM Banks */
	if ((NesHeader.byRomSize * 2) > 3)
	{
		ROMBANK0 = ROMPAGE( 0 );
		ROMBANK1 = ROMPAGE( 1 );
		ROMBANK2 = ROMPAGE( 2 );
		ROMBANK3 = ROMPAGE( 3 );
	}
	else
	if ((NesHeader.byRomSize * 2) > 1)
	{
		ROMBANK0 = ROMPAGE( 0 );
		ROMBANK1 = ROMPAGE( 1 );
		ROMBANK2 = ROMPAGE( 0 );
		ROMBANK3 = ROMPAGE( 1 );
	}
	else
	{
		ROMBANK0 = ROMPAGE( 0 );
		ROMBANK1 = ROMPAGE( 0 );
		ROMBANK2 = ROMPAGE( 0 );
		ROMBANK3 = ROMPAGE( 0 );
	}

	/* Set PPU Banks */
	if ( NesHeader.byVRomSize > 0 ) {
		for ( nPage = 0; nPage < 8; ++nPage )
			PPUBANK[ nPage ] = &VROM[ nPage * 0x400 ];
		memset4(ChrBufFlags, 0, 128);
	}

	/* Set up wiring of the interrupt pin */
	K6502_Set_Int_Wiring( 1, 1 ); 
}

/*-------------------------------------------------------------------*/
/*  Mapper 0 Write Function                                          */
/*-------------------------------------------------------------------*/
void Map0_Write( uint16 wAddr, unsigned char byData )
{
/*
 *  Dummy Write to Mapper
 *
 */
}

/*-------------------------------------------------------------------*/
/*  Mapper 1 Read Function                                          */
/*-------------------------------------------------------------------*/
unsigned char Map0_Read( uint16 wAddr )
{
	return (unsigned char)(wAddr >> 8);
}

/*-------------------------------------------------------------------*/
/*  Mapper 0 V-Sync Function                                         */
/*-------------------------------------------------------------------*/
void Map0_VSync()
{
/*
 *  Dummy Callback at VSync
 *
 */
}

/*-------------------------------------------------------------------*/
/*  Mapper 0 H-Sync Function                                         */
/*-------------------------------------------------------------------*/
void Map0_HSync()
{
/*
 *  Dummy Callback at HSync
 *
 */
}

/*===================================================================*/
/*                                                                   */
/*                            Mapper 1                               */
/*                                                                   */
/*===================================================================*/

unsigned char Map1_Reg[ 4 ];

int Map1_Cnt;
unsigned char Map1_Latch;

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 1                                              */
/*-------------------------------------------------------------------*/
void Map1_Init()
{
  int nPage;

  /* Initialize Mapper */
  MapperInit = Map1_Init;

  /* Read from Mapper */
  MapperRead = Map0_Read;

  /* Write to Mapper */
  MapperWrite = Map1_Write;

  /* Callback at VSync */
  MapperVSync = Map0_VSync;

  /* Callback at HSync */
  MapperHSync = Map0_HSync;

  pNesX_MemorySet( Map1_Reg, 0, sizeof Map1_Reg );
  Map1_Cnt = Map1_Latch = 0;

  /* Set ROM Banks */
  ROMBANK0 = ROMPAGE( 0 );
  ROMBANK1 = ROMPAGE( 1 );
  ROMBANK2 = ROMLASTPAGE( 1 );
  ROMBANK3 = ROMLASTPAGE( 0 );

  /* Set PPU VROM Banks */
  if ( NesHeader.byVRomSize > 0 )
  {
    for ( nPage = 0; nPage < 8; ++nPage )
      PPUBANK[ nPage ] = &VROM[ nPage * 0x400 ];
	memset4(ChrBufFlags, 0, 128);
  }

  /* Set up wiring of the interrupt pin */
  K6502_Set_Int_Wiring( 1, 1 );
}

/*-------------------------------------------------------------------*/
/*  Mapper 1 Write Function                                          */
/*-------------------------------------------------------------------*/
void Map1_Write( uint16 wAddr, unsigned char byData )
{
/*
 * MMC1
 */
	if (wAddr < 0x8000)
		return;


  int nReg;
  int nROMPos;
  int nBank;
  int nVBank;
  int nPage;

  nReg = ( wAddr >> 13 ) - 4;

  if ( byData & 0x80 )
  {
    // Reset
    Map1_Reg[ 0 ] |= 0xc;
    Map1_Cnt = 0;
    Map1_Latch = 0;
    return;
  }
  else
  {
    // Bit Data Set
    Map1_Latch |= ( ( byData & 1 ) << Map1_Cnt );
    ++Map1_Cnt;
  }
  
  if ( Map1_Cnt < 5 )
    return;

  // 5Bits Latched
  Map1_Reg[ nReg ] = ( nReg == 3 ) ? ( Map1_Latch & 0xf ) : Map1_Latch;
  Map1_Cnt = 0;
  Map1_Latch = 0;
  
  // Name Table Mirroring
  pNesX_Mirroring( ( Map1_Reg[ 0 ] & 3 ) ^ 3 );
    
  // Select Program ROM Bank

  // 256K ROM Position Selection
  nROMPos = 0;
  if ( NesHeader.byRomSize == 32 )
  {
    /* 512K Program ROM */
    // 0 (First 256K) or 32 (Second 256K)
    nROMPos = ( Map1_Reg[ 1 ] & 0x10 ) << 1;
  }
  else
  if ( NesHeader.byRomSize == 64 )
  {
    /* 1024K Program ROM */
    if ( Map1_Reg[ 0 ] & 0x10 )
    {
      // Acknowledge 256K selection register 1 
      nROMPos = ( ( Map1_Reg[ 1 ] & 0x10 ) << 1 ) |
               ( ( Map1_Reg[ 2 ] & 0x10 ) << 2 );
    }
    else 
    {
      // Ignore 256K selection register 1
      // 0 (First 256K) or 64 (Third 256K)
      nROMPos = ( Map1_Reg[ 1 ] & 0x10 ) << 2;
    }
  }
    
  // Set Program ROM Bank
  if ( !( Map1_Reg[ 0 ] & 8 ) )
  {
    // 32K ROM Bank
    /* Set ROM Banks */
//    nBank = ( ( Map1_Reg[ 3 ] << 2 ) + nROMPos ) % ( NesHeader.byRomSize << 1 );
    nBank = ( ( Map1_Reg[ 3 ] << 2 ) + nROMPos );

    ROMBANK0 = ROMPAGE( nBank );
    ROMBANK1 = ROMPAGE( nBank + 1 );
    ROMBANK2 = ROMPAGE( nBank + 2 );
    ROMBANK3 = ROMPAGE( nBank + 3 );
  }
  else
  if ( Map1_Reg[ 0 ] & 4 )
  {
    // 16K ROM Bank at 0x8000
    nBank = ( ( Map1_Reg[ 3 ] << 1 ) + nROMPos );
    ROMBANK0 = ROMPAGE( nBank );
    ROMBANK1 = ROMPAGE( nBank + 1 );
    ROMBANK2 = ROMLASTPAGE( 1 );
    ROMBANK3 = ROMLASTPAGE( 0 );
  }
  else
  {
    // 16K ROM Bank at 0xc000
    nBank = ( ( Map1_Reg[ 3 ] << 1 ) + nROMPos );
    ROMBANK0 = ROMPAGE( 0 );
    ROMBANK1 = ROMPAGE( 1 );
    ROMBANK2 = ROMPAGE( nBank );
    ROMBANK3 = ROMPAGE( nBank + 1 );
  }

  // Select PPU VROM Bank
  if ( NesHeader.byVRomSize > 0 )
  {
    if ( Map1_Reg[ 0 ] & 0x10 )
    {
		// 4K VROM Switching
		if (nReg == 1)
		{
			nVBank = Map1_Reg[ 1 ] << 2;
			for ( nPage = 0; nPage < 4; ++nPage )
				PPUBANK[ nPage ] = &VROM[ (nVBank + nPage) * 0x400 ];
		    memset4(ChrBufFlags, 0, 64);
		}
		else
		{
			nVBank = Map1_Reg[ 2 ] << 2;
			for ( nPage = 0; nPage < 4; ++nPage )
				PPUBANK[ nPage + 4 ] = &VROM[ (nVBank + nPage) * 0x400 ];
			memset4(ChrBufFlags + 64, 0, 64);
		}
    }
    else
    {
      // 8K VROM Switching
      nVBank = ( Map1_Reg[ 1 ] & 0xe ) << 2;
      for ( nPage = 0; nPage < 8; ++nPage )
        PPUBANK[ nPage ] = &VROM[ (nVBank + nPage) * 0x400 ];
	  memset4(ChrBufFlags, 0, 128);		
    }

//    pNesX_SetupChr();
  }
}


/*===================================================================*/
/*                                                                   */
/*                        Mapper 2 (UNROM)                           */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 2                                              */
/*-------------------------------------------------------------------*/
void Map2_Init()
{
	/* Initialize Mapper */
	MapperInit = Map2_Init;

    /* Read from Mapper */
    MapperRead = Map0_Read;

	/* Write to Mapper */
	MapperWrite = Map2_Write;

	/* Callback at VSync */
	MapperVSync = Map0_VSync;

	/* Callback at HSync */
	MapperHSync = Map0_HSync;

	/* Set ROM Banks */
	ROMBANK0 = ROMPAGE( 0 );
	ROMBANK1 = ROMPAGE( 1 );
	ROMBANK2 = ROMLASTPAGE( 1 );
	ROMBANK3 = ROMLASTPAGE( 0 );

	/* Set up wiring of the interrupt pin */
	K6502_Set_Int_Wiring( 1, 1 ); 
}

/*-------------------------------------------------------------------*/
/*  Mapper 2 Write Function                                          */
/*-------------------------------------------------------------------*/
void Map2_Write( uint16 wAddr, unsigned char byData )
{
	if (wAddr >= 0x8000) {
		/* Set ROM Banks */
		byData %= NesHeader.byRomSize;
		byData <<= 1;

		ROMBANK0 = ROMPAGE( byData );
		ROMBANK1 = ROMPAGE( byData + 1 );
	}
}

/*===================================================================*/
/*                                                                   */
/*                     Mapper 3 (VROM Switch)                        */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 3                                              */
/*-------------------------------------------------------------------*/
void Map3_Init()
{
	int nPage;

	/* Initialize Mapper */
	MapperInit = Map3_Init;

    /* Read from Mapper */
    MapperRead = Map0_Read;

	/* Write to Mapper */
	MapperWrite = Map3_Write;

	/* Callback at VSync */
	MapperVSync = Map0_VSync;

	/* Callback at HSync */
	MapperHSync = Map0_HSync;

	/* Set ROM Banks */
	if ((NesHeader.byRomSize * 2) > 2)
	{
		ROMBANK0 = ROMPAGE( 0 );
		ROMBANK1 = ROMPAGE( 1 );
		ROMBANK2 = ROMPAGE( 2 );
		ROMBANK3 = ROMPAGE( 3 );
	}
	else
	{
		ROMBANK0 = ROMPAGE( 0 );
		ROMBANK1 = ROMPAGE( 1 );
		ROMBANK2 = ROMPAGE( 0 );
		ROMBANK3 = ROMPAGE( 1 );
	}

	/* Set PPU Banks */
	for ( nPage = 0; nPage < 8; ++nPage )
		PPUBANK[ nPage ] = &VROM[ nPage * 0x400 ];
	memset4(ChrBufFlags, 0, 128);		

	/* Set up wiring of the interrupt pin */
	/* "DragonQuest" doesn't run if IRQ isn't made to occur in CLI */
	K6502_Set_Int_Wiring( 1, 0 ); 
}

/*-------------------------------------------------------------------*/
/*  Mapper 3 Write Function                                          */
/*-------------------------------------------------------------------*/
void Map3_Write( uint16 wAddr, unsigned char byData )
{
	if (wAddr < 0x8000) {
		return;
	}

	uint16 base;

	byData &= ((NesHeader.byVRomSize * 8) >> 1) - 1;

	base = ((uint16)byData) << 3;
	PPUBANK[0] = &VROM[ (base * 0x400) ];
	PPUBANK[1] = &VROM[ ((base + 1) * 0x400) ];
	PPUBANK[2] = &VROM[ ((base + 2) * 0x400) ];
	PPUBANK[3] = &VROM[ ((base + 3) * 0x400) ];
	PPUBANK[4] = &VROM[ ((base + 4) * 0x400) ];
	PPUBANK[5] = &VROM[ ((base + 5) * 0x400) ];
	PPUBANK[6] = &VROM[ ((base + 6) * 0x400) ];
	PPUBANK[7] = &VROM[ ((base + 7) * 0x400) ];
	memset4(ChrBufFlags, 0, 128);
}

/*===================================================================*/
/*                                                                   */
/*                        Mapper 4 (MMC3)                            */
/*                                                                   */
/*===================================================================*/

unsigned char Map4_VROM_Base;
unsigned char Map4_ROM_Base;
unsigned char Map4_Cmd;
unsigned char Map4_Banks_Reg[ 8 ];
unsigned char Map4_IRQ_Cnt;
unsigned char Map4_IRQ_Set;
unsigned char Map4_IRQ_Enable;

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 4                                              */
/*-------------------------------------------------------------------*/
void Map4_Init()
{
	/* Initialize Mapper */
	MapperInit = Map4_Init;

    /* Read from Mapper */
    MapperRead = Map0_Read;

	/* Write to Mapper */
	MapperWrite = Map4_Write;

	/* Callback at VSync */
	MapperVSync = Map4_VSync;

	/* Callback at HSync */
	MapperHSync = Map4_HSync;

	Map4_VROM_Base = Map4_ROM_Base = Map4_Cmd = Map4_IRQ_Cnt = Map4_IRQ_Set = Map4_IRQ_Enable = 0;

	//Banks Registers 6 + 7 are program rom registers
	Map4_Banks_Reg[ 6 ] = 0;
	Map4_Banks_Reg[ 7 ] = 1;

	if (NesHeader.byVRomSize)
	{
		Map4_Banks_Reg[ 0 ] = 0;
		Map4_Banks_Reg[ 1 ] = 2;
		Map4_Banks_Reg[ 2 ] = 4;
		Map4_Banks_Reg[ 3 ] = 5;
		Map4_Banks_Reg[ 4 ] = 6;
		Map4_Banks_Reg[ 5 ] = 7;
	}
	else
	{
		Map4_Banks_Reg[ 0 ] = 0;
		Map4_Banks_Reg[ 1 ] = 0;
		Map4_Banks_Reg[ 2 ] = 0;
		Map4_Banks_Reg[ 3 ] = 0;
		Map4_Banks_Reg[ 4 ] = 0;
		Map4_Banks_Reg[ 5 ] = 0;
	}

	Map4_Write( 0x8000, 0 );
	Map4_Write( 0x8001, 0 );

	/* Set up wiring of the interrupt pin */
	K6502_Set_Int_Wiring( 1, 1 ); 

	Map4_IRQ_Enable = 0;
	Map4_IRQ_Set = 0;
	Map4_IRQ_Cnt = 0;
}

void Map4_set_PPU_banks()
{
	/* Set VROM Banks */
	if ( NesHeader.byVRomSize > 0 )
	{
		if ( Map4_VROM_Base )
		{
			PPUBANK[ 0 ] = VROMPAGE( Map4_Banks_Reg[ 2 ] );
			PPUBANK[ 1 ] = VROMPAGE( Map4_Banks_Reg[ 3 ] );
			PPUBANK[ 2 ] = VROMPAGE( Map4_Banks_Reg[ 4 ] );
			PPUBANK[ 3 ] = VROMPAGE( Map4_Banks_Reg[ 5 ] );
			PPUBANK[ 4 ] = VROMPAGE( Map4_Banks_Reg[ 0 ] );
			PPUBANK[ 5 ] = VROMPAGE( Map4_Banks_Reg[ 0 ] + 1 );
			PPUBANK[ 6 ] = VROMPAGE( Map4_Banks_Reg[ 1 ] );
			PPUBANK[ 7 ] = VROMPAGE( Map4_Banks_Reg[ 1 ] + 1 );
		}
		else
		{
			PPUBANK[ 0 ] = VROMPAGE( Map4_Banks_Reg[ 0 ] );
			PPUBANK[ 1 ] = VROMPAGE( Map4_Banks_Reg[ 0 ] + 1 );
			PPUBANK[ 2 ] = VROMPAGE( Map4_Banks_Reg[ 1 ] );
			PPUBANK[ 3 ] = VROMPAGE( Map4_Banks_Reg[ 1 ] + 1 );
			PPUBANK[ 4 ] = VROMPAGE( Map4_Banks_Reg[ 2 ] );
			PPUBANK[ 5 ] = VROMPAGE( Map4_Banks_Reg[ 3 ] );
			PPUBANK[ 6 ] = VROMPAGE( Map4_Banks_Reg[ 4 ] );
			PPUBANK[ 7 ] = VROMPAGE( Map4_Banks_Reg[ 5 ] );
		}

		memset4(ChrBufFlags, 0, 128);
	}
}

void Map4_set_CPU_banks()
{
	if ( Map4_ROM_Base )
	{
		ROMBANK0 = ROMLASTPAGE( 1 );
		ROMBANK1 = ROMPAGE( Map4_Banks_Reg[ 7 ] );
		ROMBANK2 = ROMPAGE( Map4_Banks_Reg[ 6 ] );
		ROMBANK3 = ROMLASTPAGE( 0 );
	}
	else
	{
		ROMBANK0 = ROMPAGE( Map4_Banks_Reg[ 6 ] );
		ROMBANK1 = ROMPAGE( Map4_Banks_Reg[ 7 ] );
		ROMBANK2 = ROMLASTPAGE( 1 );
		ROMBANK3 = ROMLASTPAGE( 0 );
	}
}

/*-------------------------------------------------------------------*/
/*  Mapper 4 Write Function                                          */
/*-------------------------------------------------------------------*/
void Map4_Write( uint16 wAddr, unsigned char byData )
{
  uint16 wMapAddr;

  wMapAddr = wAddr & 0xe001;

  switch ( wMapAddr )
  {
	case 0xa000:
		// Name Table Mirroring - But only if 4 Screen Mirroring is not enabled
		if (!(NesHeader.byInfo1 & 0x08))
			pNesX_Mirroring( ~byData & 1 );
		break;

	case 0xa001:
		// Enable/Disable SRAM - Not Emulated
		break;

	case 0xc000:
		Map4_IRQ_Cnt = byData - 2;
		break;

	case 0xc001:
		Map4_IRQ_Set = byData;
		break;

    case 0xe000:
		Map4_IRQ_Enable = 0;
      break;

    case 0xe001:
        Map4_IRQ_Enable = 1;
		break;

	case 0x8000:
        Map4_VROM_Base = byData >> 7;
        Map4_ROM_Base = ( byData >> 6 ) & 1;
        Map4_Cmd = byData & 7;
		Map4_set_CPU_banks();
		Map4_set_PPU_banks();
		break;

	case 0x8001:
		if ( Map4_Cmd >= 6 )
		{
			byData %= ( NesHeader.byRomSize << 1 );
			Map4_Banks_Reg[ Map4_Cmd ] = byData;
			Map4_set_CPU_banks();
		}
		else
		if ( NesHeader.byVRomSize > 0 )
		{
			if ( Map4_Cmd < 2 )
				byData = ( byData & 0xfe ) % ( NesHeader.byVRomSize << 3 );
			else
			if ( Map4_Cmd < 6 )
				byData %= ( NesHeader.byVRomSize << 3 );
			Map4_Banks_Reg[ Map4_Cmd ] = byData;
			Map4_set_PPU_banks();
		}
		break;
  }
}

/*-------------------------------------------------------------------*/
/*  Mapper 4 V-Sync Function                                         */
/*-------------------------------------------------------------------*/
void Map4_VSync()
{
/*
 *  Callback at VSync
 *
 */
  Map4_IRQ_Cnt = Map4_IRQ_Set;
}

/*-------------------------------------------------------------------*/
/*  Mapper 4 H-Sync Function                                         */
/*-------------------------------------------------------------------*/
void Map4_HSync()
{
	if (Map4_IRQ_Enable)
	{
		if ((ppuinfo.PPU_Scanline >= 0) && (ppuinfo.PPU_Scanline <= 239))
		{
			if (PPU_R1 & (R1_SHOW_SCR | R1_SHOW_SP ))
			{
				if (!(Map4_IRQ_Cnt--))
				{
					Map4_IRQ_Cnt = Map4_IRQ_Set;
					IRQ_REQ;
				}
			}
		}
	}
}

/*===================================================================*/
/*                                                                   */
/*                        Mapper 5 (MMC5)                            */
/*                                                                   */
/*===================================================================*/
// MS - Ported from Nester Source

uint32 Map5_wb[8];
unsigned char Map5_wram[8 * 0x2000];
unsigned char Map5_wram_size;
unsigned char Map5_chr_reg[8][2];
unsigned char Map5_irq_enabled;
unsigned char Map5_irq_status;
uint32 Map5_irq_line;
uint32 Map5_value0;
uint32 Map5_value1;
unsigned char Map5_wram_protect0;
unsigned char Map5_wram_protect1;
unsigned char Map5_prg_size;
unsigned char Map5_chr_size;
unsigned char Map5_gfx_mode;
unsigned char Map5_split_control;
unsigned char Map5_split_bank;

extern uint32 currentCRC32;

void MMC5_set_WRAM_bank(unsigned char page, unsigned char bank) {
//	printf("MMC5_set_WRAM_bank: %i, %i\n", page, bank);
	if (bank != 8) {
		if(Map5_wram_size == 1) bank = (bank > 3) ? 8 : 0;
		if(Map5_wram_size == 2) bank = (bank > 3) ? 1 : 0;
		if(Map5_wram_size == 3) bank = (bank > 3) ? 8 : bank;
		if(Map5_wram_size == 4) bank = (bank > 3) ? 4 : bank;
	}
	Map5_wb[page] = bank;

	if (bank != 8) {
		BankTable[page] = Map5_wram + (bank * 0x2000);
	}
}

void MMC5_set_CPU_bank(unsigned char page, unsigned char bank) {
//	printf("MMC5_set_CPU_bank: %i, %i\n", page, bank);
	if (bank & 0x80) {
		switch (Map5_prg_size) {
			case 0:
				if (page == 7) {
					ROMBANK0 = ROMPAGE((bank & 0x7C));
					ROMBANK1 = ROMPAGE((bank & 0x7C)+1);
					ROMBANK2 = ROMPAGE((bank & 0x7C)+2);
					ROMBANK3 = ROMPAGE((bank & 0x7C)+3);
					Map5_wb[4] = Map5_wb[5] = Map5_wb[6] = 8;
				}
				break;
			case 1:
				if (page == 5) {
					ROMBANK0 = ROMPAGE((bank & 0x7E));
					ROMBANK1 = ROMPAGE((bank & 0x7E)+1);
					Map5_wb[4] = Map5_wb[5] = 8;
				}
				if (page == 7) {
					ROMBANK2 = ROMPAGE((bank & 0x7E));
					ROMBANK3 = ROMPAGE((bank & 0x7E)+1);
					Map5_wb[6] = 8;
				}			
				break;
			case 2:
				if (page == 5) {
					ROMBANK0 = ROMPAGE((bank & 0x7E));
					ROMBANK1 = ROMPAGE((bank & 0x7E)+1);
					Map5_wb[4] = Map5_wb[5] = 8;
				}
				if (page == 6) {
					ROMBANK2 = ROMPAGE((bank & 0x7F));			
					Map5_wb[6] = 8;
				}
				if (page == 7) {
					ROMBANK3 = ROMPAGE((bank & 0x7F));
				}			
				break;
			case 3:
				if (page == 4) {
					ROMBANK0 = ROMPAGE((bank & 0x7F));
					Map5_wb[4] = 8;
				}
				if (page == 5) {
					ROMBANK1 = ROMPAGE((bank & 0x7F));
					Map5_wb[5] = 8;
				}
				if (page == 6) {
					ROMBANK2 = ROMPAGE((bank & 0x7F));			
					Map5_wb[6] = 8;
				}
				if (page == 7) {
					ROMBANK3 = ROMPAGE((bank & 0x7F));
				}			
				break;
		}
	} else {
		switch (Map5_prg_size) {
			case 1:
				if (page == 5) {
					MMC5_set_WRAM_bank(4, (bank & 0x06));
					MMC5_set_WRAM_bank(5, (bank & 0x06)+1);
				}			
				break;

			case 2:
				if (page == 5) {
					MMC5_set_WRAM_bank(4, (bank & 0x06));
					MMC5_set_WRAM_bank(5, (bank & 0x06)+1);
				}
				if (page == 6) {
					MMC5_set_WRAM_bank(6, bank & 0x07);
				}			
				break;

			case 3:
				if (page == 4) {
					MMC5_set_WRAM_bank(4, bank & 0x07);
				}
				if (page == 5) {
					MMC5_set_WRAM_bank(5, bank & 0x07);
				}
				if (page == 6) {
					MMC5_set_WRAM_bank(6, bank & 0x07);
				}			
				break;
		}
	}	
}

void sync_Chr_banks(unsigned char mode) {
	switch (Map5_chr_size) {
		case 0:
			PPUBANK[0] = VROMPAGE(Map5_chr_reg[7][mode]*8);
			PPUBANK[1] = VROMPAGE(Map5_chr_reg[7][mode]*8+1);
			PPUBANK[2] = VROMPAGE(Map5_chr_reg[7][mode]*8+2);
			PPUBANK[3] = VROMPAGE(Map5_chr_reg[7][mode]*8+3);
			PPUBANK[4] = VROMPAGE(Map5_chr_reg[7][mode]*8+4);
			PPUBANK[5] = VROMPAGE(Map5_chr_reg[7][mode]*8+5);
			PPUBANK[6] = VROMPAGE(Map5_chr_reg[7][mode]*8+6);
			PPUBANK[7] = VROMPAGE(Map5_chr_reg[7][mode]*8+7);		
			break;

		case 1:
			PPUBANK[0] = VROMPAGE(Map5_chr_reg[3][mode]*4);
			PPUBANK[1] = VROMPAGE(Map5_chr_reg[3][mode]*4+1);
			PPUBANK[2] = VROMPAGE(Map5_chr_reg[3][mode]*4+2);
			PPUBANK[3] = VROMPAGE(Map5_chr_reg[3][mode]*4+3);
			PPUBANK[4] = VROMPAGE(Map5_chr_reg[7][mode]*4);
			PPUBANK[5] = VROMPAGE(Map5_chr_reg[7][mode]*4+1);
			PPUBANK[6] = VROMPAGE(Map5_chr_reg[7][mode]*4+2);
			PPUBANK[7] = VROMPAGE(Map5_chr_reg[7][mode]*4+3);
			break;

		case 2:
			PPUBANK[0] = VROMPAGE(Map5_chr_reg[1][mode]*2);
			PPUBANK[1] = VROMPAGE(Map5_chr_reg[1][mode]*2+1);
			PPUBANK[2] = VROMPAGE(Map5_chr_reg[3][mode]*2);
			PPUBANK[3] = VROMPAGE(Map5_chr_reg[3][mode]*2+1);
			PPUBANK[4] = VROMPAGE(Map5_chr_reg[5][mode]*2);
			PPUBANK[5] = VROMPAGE(Map5_chr_reg[5][mode]*2+1);
			PPUBANK[6] = VROMPAGE(Map5_chr_reg[7][mode]*2);
			PPUBANK[7] = VROMPAGE(Map5_chr_reg[7][mode]*2+1);
			break;

		default:
			PPUBANK[0] = VROMPAGE(Map5_chr_reg[0][mode]);
			PPUBANK[1] = VROMPAGE(Map5_chr_reg[1][mode]);
			PPUBANK[2] = VROMPAGE(Map5_chr_reg[2][mode]);
			PPUBANK[3] = VROMPAGE(Map5_chr_reg[3][mode]);
			PPUBANK[4] = VROMPAGE(Map5_chr_reg[4][mode]);
			PPUBANK[5] = VROMPAGE(Map5_chr_reg[5][mode]);
			PPUBANK[6] = VROMPAGE(Map5_chr_reg[6][mode]);
			PPUBANK[7] = VROMPAGE(Map5_chr_reg[7][mode]);	
			break;
	}	
}

unsigned char Map5_PPU_Latch_RenderScreen(uint8 mode, uint32 addr) {
  unsigned char ex_pal = 0;

  if(Map5_gfx_mode == 1 && mode == 1) {
    // ex gfx mode
    unsigned char * nametable2 = PPUBANK[NAME_TABLE2];
    uint32 bank = (nametable2[addr] & 0x3F) << 2;
	PPUBANK[0] = VROMPAGE(bank);
	PPUBANK[1] = VROMPAGE(bank + 1);
	PPUBANK[2] = VROMPAGE(bank + 2);
	PPUBANK[3] = VROMPAGE(bank + 3);
	PPUBANK[4] = VROMPAGE(bank);
	PPUBANK[5] = VROMPAGE(bank + 1);
	PPUBANK[6] = VROMPAGE(bank + 2);
	PPUBANK[7] = VROMPAGE(bank + 3);	
    ex_pal = ((nametable2[addr] & 0xC0) >> 4) | 0x01;
  } else {
    // normal
    sync_Chr_banks(mode);
  }
  memset4(ChrBufFlags, 0, 128);
  return ex_pal;
}

/*-------------------------------------------------------------------*/
/*  Mapper 5 Init Function                                           */
/*-------------------------------------------------------------------*/
void Map5_Init() {
	/* Initialize Mapper */
	MapperInit = Map5_Init;

    /* Read from Mapper */
    MapperRead = Map5_Read;

	/* Write to Mapper */
	MapperWrite = Map5_Write;

	/* Callback at VSync */
	MapperVSync = Map0_VSync;

	/* Callback at HSync */
	MapperHSync = Map5_HSync;

	Map5_wram_size = 1;

	//MS - this should probably move to the ROM loader?
	// NES 2.0 I think has information for this so we don't need to special case by CRC of the ROM
    if (currentCRC32 == 0x2b548d75 || // Bandit Kings of Ancient China (J)
    	currentCRC32 == 0xf4cd4998 || // Dai Koukai Jidai (J)
    	currentCRC32 == 0x8fa95456 || // Ishin no Arashi (J)
    	currentCRC32 == 0x98c8e090 || // Nobunaga no Yabou - Sengoku Gunyuu Den (J)
    	currentCRC32 == 0x57e3218b || // L'Empereur (J)
    	currentCRC32 == 0x2f50bd38 || // L'Empereur (U)
    	currentCRC32 == 0x8e9a5e2f || // L'Empereur (Alt)(U)
    	currentCRC32 == 0xb56958d1 || // Nobunaga's Ambition 2 (J)
    	currentCRC32 == 0xe6c28c5f || // Suikoden - Tenmei no Chikai (J)
    	currentCRC32 == 0xcd35e2e9) {  // Uncharted Waters (J)
	    Map5_wram_size = 2;
    }

	if (currentCRC32 == 0xf4120e58 || // Aoki Ookami to Shiroki Mejika - Genchou Hishi (J)
    	currentCRC32 == 0x286613d8 || // Nobunaga no Yabou - Bushou Fuuun Roku (J)
    	currentCRC32 == 0x11eaad26 || // Romance of the 3 Kingdoms 2 (J)
    	currentCRC32 == 0x95ba5733) {  // Sangokushi 2 (J)
		Map5_wram_size = 3;
	}

	// set SaveRAM
	uint32 i;
	for(i = 0; i < 0x10000; i++) {
		Map5_wram[i] = SRAM[i];
	}
	MMC5_set_WRAM_bank(3,0);

    // Init ExSound
	// MS - TODO: Port over MMC5 extension sound
    // parent_NES->apu->SelectExSound(NES_APU_EXSOUND_MMC5);

	// set CPU bank pointers
	ROMBANK0 = ROMLASTPAGE(0);
	ROMBANK1 = ROMLASTPAGE(0);
	ROMBANK2 = ROMLASTPAGE(0);
	ROMBANK3 = ROMLASTPAGE(0);

	// set PPU bank pointers
	PPUBANK[ 0 ] = VROMPAGE( 0 );
	PPUBANK[ 1 ] = VROMPAGE( 1 );
	PPUBANK[ 2 ] = VROMPAGE( 2 );
	PPUBANK[ 3 ] = VROMPAGE( 3 );
	PPUBANK[ 4 ] = VROMPAGE( 4 );
	PPUBANK[ 5 ] = VROMPAGE( 5 );
	PPUBANK[ 6 ] = VROMPAGE( 6 );
	PPUBANK[ 7 ] = VROMPAGE( 7 );
	memset4(ChrBufFlags, 0, 128);	

	for(i = 0; i < 8; i++) {
		Map5_chr_reg[i][0] = i;
		Map5_chr_reg[i][1] = (i & 0x03) + 4;
	}
	Map5_wb[3] = 0;
	Map5_wb[4] = Map5_wb[5] = Map5_wb[6] = 8;

	Map5_prg_size = 3;
	Map5_wram_protect0 = 0x02;
	Map5_wram_protect1 = 0x01;
	Map5_chr_size = 3;
	Map5_gfx_mode = 0;

	Map5_irq_enabled = 0;
	Map5_irq_status = 0;
	Map5_irq_line = 0;

	Map5_split_control = 0;
	Map5_split_bank = 0;	
}

/*-------------------------------------------------------------------*/
/*  Mapper 5 Read Function                                           */
/*-------------------------------------------------------------------*/
unsigned char Map5_Read(uint16 wAddr) {
	unsigned char ret = (unsigned char)(wAddr >> 8);
	if (wAddr == 0x5204) {
		ret = Map5_irq_status;
		Map5_irq_status &= ~0x80;
	} else if (wAddr == 0x5205) {
		ret = (unsigned char)((Map5_value0 * Map5_value1) & 0x00FF);
	} else if (wAddr == 0x5206) {
		ret = (unsigned char)(((Map5_value0 * Map5_value1) & 0xFF00) >> 8);
	} else if (wAddr >= 0x5C00 && wAddr <=0x5FFF) {
		if (Map5_gfx_mode == 2 || Map5_gfx_mode == 3) {
			unsigned char* nametable2 = PPUBANK[NAME_TABLE2];
			ret = nametable2[wAddr & 0x3FF];
		}
	}
	return ret;
}

/*-------------------------------------------------------------------*/
/*  Mapper 5 Write Function                                          */
/*-------------------------------------------------------------------*/
void Map5_Write(uint16 wAddr, unsigned char byData) {
//	printf("Map5_Write: $%04X, %02X\n", wAddr, byData);	
	uint32 i;

	switch(wAddr) {
		case 0x5100: {
			Map5_prg_size = byData & 0x03;
		}
		break;

		case 0x5101: {
			Map5_chr_size = byData & 0x03;
		}
		break;

		case 0x5102: {
			Map5_wram_protect0 = byData & 0x03;
		}
		break;

		case 0x5103: {
			Map5_wram_protect1 = byData & 0x03;
		}
		break;

		case 0x5104: {
			Map5_gfx_mode = byData & 0x03;
		}
		break;

		case 0x5105: {
			PPUBANK[0x8] = PPURAM + (0x400 * (NAME_TABLE0 + (byData & 0x03)));
			byData >>= 2;			
			PPUBANK[0x9] = PPURAM + (0x400 * (NAME_TABLE0 + (byData & 0x03)));
			byData >>= 2;			
			PPUBANK[0xA] = PPURAM + (0x400 * (NAME_TABLE0 + (byData & 0x03)));
			byData >>= 2;			
			PPUBANK[0xB] = PPURAM + (0x400 * (NAME_TABLE0 + (byData & 0x03)));
		}
		break;

		case 0x5106: {
			unsigned char* nametable3 = PPUBANK[NAME_TABLE3];
			for (i = 0; i < 0x3C0; i++) {
				nametable3[i] = byData;
			}
		}
		break;

		case 0x5107: {
			unsigned char* nametable3 = PPUBANK[NAME_TABLE3];
			byData &= 0x03;
			byData = byData | (byData<<2) | (byData<<4) | (byData<<6);
			for (i = 0x3C0; i < 0x400; i++) {
				nametable3[i] = byData;
			}
		}
		break;

		case 0x5113: {
			MMC5_set_WRAM_bank(3, byData & 0x07);
		}
		break;

		case 0x5114:
		case 0x5115:
		case 0x5116:
		case 0x5117: {
			VIRPC;
			MMC5_set_CPU_bank(wAddr & 0x07, byData);
			BankTable[ 4 ] = ROMBANK0;
			BankTable[ 5 ] = ROMBANK1;
			BankTable[ 6 ] = ROMBANK2;
			BankTable[ 7 ] = ROMBANK3;
			REALPC;
		}
		break;

		case 0x5120:
		case 0x5121:
		case 0x5122:
		case 0x5123:
		case 0x5124:
		case 0x5125:
		case 0x5126:
		case 0x5127: {
			Map5_chr_reg[wAddr & 0x07][0] = byData;
			sync_Chr_banks(0);
		}
		break;

		case 0x5128:
		case 0x5129:
		case 0x512A:
		case 0x512B: {
			Map5_chr_reg[(wAddr & 0x03) + 0][1] = byData;
			Map5_chr_reg[(wAddr & 0x03) + 4][1] = byData;
		}
		break;

		case 0x5200: {
			Map5_split_control = byData;
		}
		break;

		case 0x5201: {
		// Intentionally commented out from Nester Source
		//split_scroll = data;
		}
		break;

		case 0x5202: {
			Map5_split_bank = byData & 0x3F;
		}
		break;

		case 0x5203: {
			Map5_irq_line = byData;
		}
		break;

		case 0x5204: {
			Map5_irq_enabled = byData;
		}
		break;

		case 0x5205: {
			Map5_value0 = byData;
		}
		break;

		case 0x5206: {
			Map5_value1 = byData;
		}
		break;

		default:
		{
			if (wAddr >= 0x5000 && wAddr <= 0x5015) {
//	MS - TODO: Add Expansion Audio Support				
//				parent_NES->apu->ExWrite(addr, data);
			}
			else if (wAddr >= 0x5C00 && wAddr <= 0x5FFF) {
				if (Map5_gfx_mode != 3) {			
					uint8* nametable2 = PPUBANK[NAME_TABLE2];
					nametable2[wAddr & 0x3FF] = byData; //(irq_status & 0) ? data : 0x40;
				}
			} else if (wAddr >= 0x8000 && wAddr <= 0x9FFF) {
				if (Map5_wb[4] != 8) {
					Map5_wram[Map5_wb[4]*0x2000+(wAddr&0x1FFF)] = byData;
					SRAM[Map5_wb[4]*0x2000+(wAddr&0x1FFF)] = byData;
				}
			} else if (wAddr >= 0xA000 && wAddr <= 0xBFFF) {
				if (Map5_wb[5] != 8) {
					Map5_wram[Map5_wb[5]*0x2000+(wAddr&0x1FFF)] = byData;
					SRAM[Map5_wb[5]*0x2000+(wAddr&0x1FFF)] = byData;
				}
			} else if(wAddr >= 0xC000 && wAddr <= 0xDFFF) {
				if (Map5_wb[6] != 8) {
					Map5_wram[Map5_wb[6]*0x2000+(wAddr&0x1FFF)] = byData;
					SRAM[Map5_wb[6]*0x2000+(wAddr&0x1FFF)] = byData;
				}
			}			
		}
		break;
	}
}

/*-------------------------------------------------------------------*/
/*  Mapper 5 H-Sync Function                                         */
/*-------------------------------------------------------------------*/
void Map5_HSync() {
	if(ppuinfo.PPU_Scanline <= 240) {
		if (ppuinfo.PPU_Scanline == Map5_irq_line) {
			if ((PPU_R1 & (R1_SHOW_SCR | R1_SHOW_SP )) == (R1_SHOW_SCR | R1_SHOW_SP)) {
				Map5_irq_status |= 0x80;
			}
		}
		if((Map5_irq_status & 0x80) && (Map5_irq_enabled & 0x80)) {
			K6502_DoIRQ();
		}
	} else {
		Map5_irq_status |= 0x40;
	}
}

/*-------------------------------------------------------------------*/
/*  Mapper 7 Init Function                                           */
/*-------------------------------------------------------------------*/
void Map7_Init()
{
	int nPage;

	/* Initialize Mapper */
	MapperInit = Map7_Init;

    /* Read from Mapper */
    MapperRead = Map0_Read;

	/* Write to Mapper */
	MapperWrite = Map7_Write;

	/* Callback at VSync */
	MapperVSync = Map0_VSync;

	/* Callback at HSync */
	MapperHSync = Map0_HSync;

	ROMBANK0 = ROMPAGE( 0 );
	ROMBANK1 = ROMPAGE( 1 );
	ROMBANK2 = ROMPAGE( 2 );
	ROMBANK3 = ROMPAGE( 3 );

	for ( nPage = 0; nPage < 8; ++nPage )
		PPUBANK[ nPage ] = &PPURAM[ nPage * 0x400 ];
	memset4(ChrBufFlags, 0, 128);

	/* Set up wiring of the interrupt pin */
	K6502_Set_Int_Wiring( 1, 1 ); 
}

/*-------------------------------------------------------------------*/
/*  Mapper 7 Write Function                                          */
/*-------------------------------------------------------------------*/
void Map7_Write(uint16 wAddr, unsigned char byData) {
//	printf("Map7_Write: $%04X, %02X\n", wAddr, byData);
	if (wAddr >= 0x8000) {
		unsigned char bank;

		bank = (byData & 0x07) << 2;
	//	printf("Setting ROM Bank to [%i]\n", bank);

		ROMBANK0 = ROMPAGE( bank );
		ROMBANK1 = ROMPAGE( bank + 1 );
		ROMBANK2 = ROMPAGE( bank + 2 );
		ROMBANK3 = ROMPAGE( bank + 3 );

		if (byData & 0x10) {
	//		printf("Setting ROM Mirroring to 1,1,1,1\n");
			pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_HIGH);
		} else {
	//		printf("Setting ROM Mirroring to 0,0,0,0\n");		
			pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_LOW);
		}
	}
}

/*-------------------------------------------------------------------*/
/*  Mapper 9 Variables                                               */
/*-------------------------------------------------------------------*/
unsigned char Map9_Regs[6];
unsigned char Map9_Latch_0000;
unsigned char Map9_Latch_1000;
unsigned char Map9_bank1swap;
unsigned char Map9_bank2swap;

/*-------------------------------------------------------------------*/
/*  Mapper 9 Init Function                                           */
/*-------------------------------------------------------------------*/
void Map9_Init()
{
	/* Initialize Mapper */
	MapperInit = Map9_Init;

	/* Write to Mapper */
	MapperWrite = Map9_Write;

	/* Callback at VSync - NULL*/
	MapperVSync = Map0_VSync;

	/* Callback at HSync - NULL */
	MapperHSync = Map0_HSync;

	// set ROM bank pointers
	ROMBANK0 = ROMPAGE(0);
	ROMBANK1 = ROMLASTPAGE(2);
	ROMBANK2 = ROMLASTPAGE(1);
	ROMBANK3 = ROMLASTPAGE(0);

	// clean out the registers
	Map9_Regs[0] = 0;
	Map9_Regs[1] = 0;
	Map9_Regs[2] = 4;
	Map9_Regs[3] = 0;
	Map9_Regs[4] = 0;
	Map9_Regs[5] = 0;

	Map9_Latch_0000 = 0xFE;
	Map9_Latch_1000 = 0xFE;

	Map9_set_VROM_0000();
	Map9_set_VROM_1000();
//	pNesX_SetupChr();

	/* Set up wiring of the interrupt pin */
	K6502_Set_Int_Wiring( 1, 1 ); 
}

void Map9_PPU_Latch_FDFE(uint16 wAddr)
{
	if(wAddr & 0x1000)
	{
		Map9_Latch_1000 = (wAddr & 0x0FF0) >> 4;
		Map9_set_VROM_1000();
	}
	else
	{
		Map9_Latch_0000 = (wAddr & 0x0FF0) >> 4;
		Map9_set_VROM_0000();
	}
}

void Map9_set_VROM_0000()
{
	unsigned char bank_num;

	if (Map9_Latch_0000 == 0xFD)
		bank_num = Map9_Regs[1];
	else
		bank_num = Map9_Regs[2];

	bank_num <<= 2;

	PPUBANK[0] = VROMPAGE( bank_num );
	PPUBANK[1] = VROMPAGE( bank_num + 1 );
	PPUBANK[2] = VROMPAGE( bank_num + 2 ); 
	PPUBANK[3] = VROMPAGE( bank_num + 3 );

	memset4(ChrBufFlags, 0, 64);
}

void Map9_set_VROM_1000()
{
	unsigned char bank_num;

	if (Map9_Latch_1000 == 0xFD)
		bank_num = Map9_Regs[3];
	else
		bank_num = Map9_Regs[4];

	bank_num <<= 2;

	PPUBANK[4] = VROMPAGE( bank_num );
	PPUBANK[5] = VROMPAGE( bank_num + 1 );
	PPUBANK[6] = VROMPAGE( bank_num + 2 ); 
	PPUBANK[7] = VROMPAGE( bank_num + 3 );
	memset4(ChrBufFlags + (64), 0, 64);
}

void Map9_Write(uint16 wAddr, unsigned char byData)
{
	switch(wAddr & 0xF000)
	{
		case 0xA000:
			{
				// 8K ROM bank at $8000
				Map9_Regs[0] = byData;
				ROMBANK0 = ROMPAGE(Map9_Regs[0]);
			}
			break;

		case 0xB000:
			{
				// B000-BFFF: select 4k VROM for (0000) $FD latch
				Map9_Regs[1] = byData;
				Map9_set_VROM_0000();
			}
			break;
		case 0xC000:
			{
				// C000-CFFF: select 4k VROM for (0000) $FE latch
				Map9_Regs[2] = byData;
				Map9_set_VROM_0000();
			}
			break;

		case 0xD000:
			{
				// D000-DFFF: select 4k VROM for (1000) $FD latch
				Map9_Regs[3] = byData;
				Map9_set_VROM_1000();
			}
			break;

		case 0xE000:
			{
				// E000-EFFF: select 4k VROM for (1000) $FE latch
				Map9_Regs[4] = byData;
				Map9_set_VROM_1000();
			}
			break;

		case 0xF000:
			{
				Map9_Regs[5] = byData;

				if(Map9_Regs[5] & 0x01) {
					//Horizontal Mirror
					pNesX_Mirroring(MIRRORING_HORIZONTAL);
				} else {
					//Vertical Mirror
					pNesX_Mirroring(MIRRORING_VERTICAL);
				}
			}
			break;
	}
}

void Map30_Init() {
//	printf("Map30_Init\n");
	/* Initialize Mapper */
	MapperInit = Map30_Init;

    /* Read from Mapper */
    MapperRead = Map0_Read;

	/* Write to Mapper */
	MapperWrite = Map30_Write;

	/* Callback at VSync - NULL*/
	MapperVSync = Map0_VSync;

	/* Callback at HSync - NULL */
	MapperHSync = Map0_HSync;

	ROMBANK0 = ROMPAGE( 0 );
	ROMBANK1 = ROMPAGE( 1 );
	ROMBANK2 = ROMLASTPAGE(1);
	ROMBANK3 = ROMLASTPAGE(0);
}

void Map30_Write( uint16 wAddr, unsigned char byData ) {
//	printf("Map30_Write: $%04X, %02X\n", wAddr, byData);
	if (wAddr < 0x8000) {
		return;
	}

    unsigned char prg = byData & 0x1f;
    unsigned char chr = (byData & 0x60) >> 5;

    prg &= (NesHeader.byRomSize - 1);
    ROMBANK0 = ROMPAGE( prg * 2 );
    ROMBANK1 = ROMPAGE( (prg * 2) + 1 );
	ROMBANK2 = ROMLASTPAGE(1);
	ROMBANK3 = ROMLASTPAGE(0);   

	unsigned char c = chr * 8;
	for ( int nPage = 0; nPage < 8; ++nPage )
		PPUBANK[ nPage ] = &PPURAM[(nPage + c) * 0x400 ];
	memset4(ChrBufFlags, 0, 128);
}