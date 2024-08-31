#include "Mapper_3.h"

/*===================================================================*/
/*                                                                   */
/*                     Mapper 3 (VROM Switch)                        */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 3                                              */
/*-------------------------------------------------------------------*/
void Mapper_3_Init() {
	int nPage;

	/* Set ROM Banks */
	if ((NesHeader.byRomSize * 2) > 2) {
		ROMBANK0 = ROMPAGE( 0 );
		ROMBANK1 = ROMPAGE( 1 );
		ROMBANK2 = ROMPAGE( 2 );
		ROMBANK3 = ROMPAGE( 3 );
	} else {
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
void Mapper_3_Write( uint16 wAddr, unsigned char byData ) {
	if (wAddr & 0x8000) {
		//printf("Map3: Setting PPU banks to page [%u]\n", byData);
		uint16 base = (byData % NesHeader.byVRomSize) * 8;

		PPUBANK[0] = &VROM[ (base * 0x400) ];
		PPUBANK[1] = &VROM[ ((base + 1) * 0x400) ];
		PPUBANK[2] = &VROM[ ((base + 2) * 0x400) ];
		PPUBANK[3] = &VROM[ ((base + 3) * 0x400) ];
		PPUBANK[4] = &VROM[ ((base + 4) * 0x400) ];
		PPUBANK[5] = &VROM[ ((base + 5) * 0x400) ];
		PPUBANK[6] = &VROM[ ((base + 6) * 0x400) ];
		PPUBANK[7] = &VROM[ ((base + 7) * 0x400) ];
	}
}