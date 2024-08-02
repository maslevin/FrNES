#include "Mapper_4.h"

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

void Mapper_4_Set_PPU_banks() {
	/* Set VROM Banks */
	if ( NesHeader.byVRomSize > 0 ) {
		if ( Map4_VROM_Base ) {
			PPUBANK[ 0 ] = VROMPAGE( Map4_Banks_Reg[ 2 ] );
			PPUBANK[ 1 ] = VROMPAGE( Map4_Banks_Reg[ 3 ] );
			PPUBANK[ 2 ] = VROMPAGE( Map4_Banks_Reg[ 4 ] );
			PPUBANK[ 3 ] = VROMPAGE( Map4_Banks_Reg[ 5 ] );
			PPUBANK[ 4 ] = VROMPAGE( Map4_Banks_Reg[ 0 ] );
			PPUBANK[ 5 ] = VROMPAGE( Map4_Banks_Reg[ 0 ] + 1 );
			PPUBANK[ 6 ] = VROMPAGE( Map4_Banks_Reg[ 1 ] );
			PPUBANK[ 7 ] = VROMPAGE( Map4_Banks_Reg[ 1 ] + 1 );
		} else {
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

void Mapper_4_Set_CPU_banks() {
	if ( Map4_ROM_Base ) {
		ROMBANK0 = ROMLASTPAGE( 1 );
		ROMBANK1 = ROMPAGE( Map4_Banks_Reg[ 7 ] );
		ROMBANK2 = ROMPAGE( Map4_Banks_Reg[ 6 ] );
		ROMBANK3 = ROMLASTPAGE( 0 );
	} else {
		ROMBANK0 = ROMPAGE( Map4_Banks_Reg[ 6 ] );
		ROMBANK1 = ROMPAGE( Map4_Banks_Reg[ 7 ] );
		ROMBANK2 = ROMLASTPAGE( 1 );
		ROMBANK3 = ROMLASTPAGE( 0 );
	}
}

/*-------------------------------------------------------------------*/
/*  Mapper 4 Write Function                                          */
/*-------------------------------------------------------------------*/
void Mapper_4_Write( uint16 wAddr, unsigned char byData ) {
    uint16 wMapAddr;

    wMapAddr = wAddr & 0xe001;

    switch ( wMapAddr ) {
        case 0xa000:
			// Name Table Mirroring - But only if 4 Screen Mirroring is not enabled
			if (!(NesHeader.byInfo1 & 0x08)) {
				if (byData & 0x1) {
					pNesX_Mirroring(MIRRORING_HORIZONTAL);
				} else {
					pNesX_Mirroring(MIRRORING_VERTICAL);
				}
			}
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
            Mapper_4_Set_CPU_banks();
            Mapper_4_Set_PPU_banks();
            break;

        case 0x8001:
            if ( Map4_Cmd >= 6 ) {
                byData %= ( NesHeader.byRomSize << 1 );
                Map4_Banks_Reg[ Map4_Cmd ] = byData;
                Mapper_4_Set_CPU_banks();
            } else if ( NesHeader.byVRomSize > 0 ) {
                if ( Map4_Cmd < 2 )
                    byData = ( byData & 0xfe ) % ( NesHeader.byVRomSize << 3 );
                else if ( Map4_Cmd < 6 )
                    byData %= ( NesHeader.byVRomSize << 3 );
                Map4_Banks_Reg[ Map4_Cmd ] = byData;
                Mapper_4_Set_PPU_banks();
            }
            break;
    }
}

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 4                                              */
/*-------------------------------------------------------------------*/
void Mapper_4_Init() {
	Map4_VROM_Base = Map4_ROM_Base = Map4_Cmd = Map4_IRQ_Cnt = Map4_IRQ_Set = Map4_IRQ_Enable = 0;

	//Banks Registers 6 + 7 are program rom registers
	Map4_Banks_Reg[ 6 ] = 0;
	Map4_Banks_Reg[ 7 ] = 1;

	if (NesHeader.byVRomSize) {
		Map4_Banks_Reg[ 0 ] = 0;
		Map4_Banks_Reg[ 1 ] = 2;
		Map4_Banks_Reg[ 2 ] = 4;
		Map4_Banks_Reg[ 3 ] = 5;
		Map4_Banks_Reg[ 4 ] = 6;
		Map4_Banks_Reg[ 5 ] = 7;
	} else {
		Map4_Banks_Reg[ 0 ] = 0;
		Map4_Banks_Reg[ 1 ] = 0;
		Map4_Banks_Reg[ 2 ] = 0;
		Map4_Banks_Reg[ 3 ] = 0;
		Map4_Banks_Reg[ 4 ] = 0;
		Map4_Banks_Reg[ 5 ] = 0;
	}

	Mapper_4_Write( 0x8000, 0 );
	Mapper_4_Write( 0x8001, 0 );

	/* Set up wiring of the interrupt pin */
	K6502_Set_Int_Wiring( 1, 1 ); 

	Map4_IRQ_Enable = 0;
	Map4_IRQ_Set = 0;
	Map4_IRQ_Cnt = 0;
}

/*-------------------------------------------------------------------*/
/*  Mapper 4 V-Sync Function                                         */
/*-------------------------------------------------------------------*/
void Mapper_4_VSync() {
    Map4_IRQ_Cnt = Map4_IRQ_Set;
}

/*-------------------------------------------------------------------*/
/*  Mapper 4 H-Sync Function                                         */
/*-------------------------------------------------------------------*/
void Mapper_4_HSync() {
	if (Map4_IRQ_Enable) {
		if ((ppuinfo.PPU_Scanline >= 0) && (ppuinfo.PPU_Scanline <= 239)) {
			if (PPU_R1 & (R1_SHOW_SCR | R1_SHOW_SP )) {
				if (Map4_IRQ_Cnt == 254) {
					Map4_IRQ_Cnt = Map4_IRQ_Set;
					IRQ_REQ;
				}
				Map4_IRQ_Cnt--;
			}
		}
	}
}
