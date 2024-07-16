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
  { 5, NULL },
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
	if ( NesHeader.byVRomSize > 0 )
	{
		for ( nPage = 0; nPage < 8; ++nPage )
			PPUBANK[ nPage ] = &VROM[ nPage * 0x400 ];
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
		}
		else
		{
			nVBank = Map1_Reg[ 2 ] << 2;
			for ( nPage = 0; nPage < 4; ++nPage )
				PPUBANK[ nPage + 4 ] = &VROM[ (nVBank + nPage) * 0x400 ];
		}
    }
    else
    {
      // 8K VROM Switching
      nVBank = ( Map1_Reg[ 1 ] & 0xe ) << 2;
      for ( nPage = 0; nPage < 8; ++nPage )
        PPUBANK[ nPage ] = &VROM[ (nVBank + nPage) * 0x400 ];
    }
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
  /* Set ROM Banks */
  byData %= NesHeader.byRomSize;
  byData <<= 1;

  ROMBANK0 = ROMPAGE( byData );
  ROMBANK1 = ROMPAGE( byData + 1 );
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

	/* Set up wiring of the interrupt pin */
	/* "DragonQuest" doesn't run if IRQ isn't made to occur in CLI */
	K6502_Set_Int_Wiring( 1, 0 ); 
}

/*-------------------------------------------------------------------*/
/*  Mapper 3 Write Function                                          */
/*-------------------------------------------------------------------*/
void Map3_Write( uint16 wAddr, unsigned char byData )
{
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
/*
 *  Callback at HSync
 *
 */

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

/*-------------------------------------------------------------------*/
/*  Mapper 7 Init Function                                           */
/*-------------------------------------------------------------------*/
void Map7_Init()
{
	int nPage;

	/* Initialize Mapper */
	MapperInit = Map7_Init;

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

	/* Set up wiring of the interrupt pin */
	K6502_Set_Int_Wiring( 1, 1 ); 
}

/*-------------------------------------------------------------------*/
/*  Mapper 7 Write Function                                          */
/*-------------------------------------------------------------------*/
void Map7_Write(uint16 wAddr, unsigned char byData) {
//	printf("Map7_Write: $%04X, %02X\n", wAddr, byData);
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

	/* Set up wiring of the interrupt pin */
	K6502_Set_Int_Wiring( 1, 1 ); 
}

void Map9_PPU_Latch_FDFE(uint16 wAddr) {
	if (wAddr & 0x1000) {
//		printf("Map9: Latching BG Addr [0x%04x]\n", wAddr);		
		Map9_Latch_1000 = (wAddr & 0x0FF0) >> 4;
		Map9_set_VROM_1000();
	} else {
//		printf("Map9: Latching Spr Addr [0x%04x]\n", wAddr);
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
}

void Map9_Write(uint16 wAddr, unsigned char byData)
{
	switch(wAddr & 0xF000)
	{
		case 0xA000:
			{
				// 8K ROM bank at $8000
				Map9_Regs[0] = byData & 0x0F;
				ROMBANK0 = ROMPAGE(Map9_Regs[0]);
			}
			break;

		case 0xB000:
			{
				// B000-BFFF: select 4k VROM for (0000) $FD latch
				Map9_Regs[1] = byData & 0x1F;
				Map9_set_VROM_0000();
			}
			break;
		case 0xC000:
			{
				// C000-CFFF: select 4k VROM for (0000) $FE latch
				Map9_Regs[2] = byData & 0x1F;
				Map9_set_VROM_0000();
			}
			break;

		case 0xD000:
			{
				// D000-DFFF: select 4k VROM for (1000) $FD latch
				Map9_Regs[3] = byData & 0x1F;
				Map9_set_VROM_1000();
			}
			break;

		case 0xE000:
			{
				// E000-EFFF: select 4k VROM for (1000) $FE latch
				Map9_Regs[4] = byData & 0x1F;
				Map9_set_VROM_1000();
			}
			break;

		case 0xF000:
			{
				Map9_Regs[5] = byData;
				if (Map9_Regs[5] & 0x01) {
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
}