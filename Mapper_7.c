#include "Mapper_7.h"

/*-------------------------------------------------------------------*/
/*  Mapper 7 Init Function                                           */
/*-------------------------------------------------------------------*/
void Mapper_7_Init() {
	int nPage;

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
void Mapper_7_Write(uint16 wAddr, unsigned char byData) {
//	printf("Map7_Write: $%04X, %02X\n", wAddr, byData);
	unsigned char bank;

	uint32 num_8k_ROM_banks = NesHeader.byRomSize * 2;
	bank = ((byData & 0x07) << 2) % num_8k_ROM_banks;
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