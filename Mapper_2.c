#include "Mapper.h"
#include "Mapper_2.h"

/*===================================================================*/
/*                                                                   */
/*                        Mapper 2 (UNROM)                           */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 2                                              */
/*-------------------------------------------------------------------*/
void Mapper_2_Init() {
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
void Mapper_2_Write( uint16 wAddr, unsigned char byData ) {
    /* Set ROM Banks */
    byData %= NesHeader.byRomSize;
    byData <<= 1;

    ROMBANK0 = ROMPAGE( byData );
    ROMBANK1 = ROMPAGE( byData + 1 );
}