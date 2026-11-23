#include "Mapper_0.h"

/*===================================================================*/
/*                                                                   */
/*                            Mapper 0                               */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 0                                              */
/*-------------------------------------------------------------------*/
void Mapper_0_Init() {
	int nPage;

	/* Set ROM Banks */
	if (num_8k_ROM_pages > 3) {
		ROMBANK0 = ROM_pages[0];
		ROMBANK1 = ROM_pages[1];
		ROMBANK2 = ROM_pages[2];
		ROMBANK3 = ROM_pages[3];
	} else if (num_8k_ROM_pages > 1) {
		ROMBANK0 = ROM_pages[0];
		ROMBANK1 = ROM_pages[1];
		ROMBANK2 = ROM_pages[0];
		ROMBANK3 = ROM_pages[1];
	} else {
		ROMBANK0 = ROM_pages[0];
		ROMBANK1 = ROM_pages[0];
		ROMBANK2 = ROM_pages[0];
		ROMBANK3 = ROM_pages[0];
	}

	/* Set PPU Banks */
	for ( nPage = 0; nPage < 8; ++nPage )
		PPUBANK[ nPage ] = VROM_pages[nPage];
}

/*-------------------------------------------------------------------*/
/*  Mapper 0 Write Function                                          */
/*-------------------------------------------------------------------*/
unsigned char Mapper_0_Read( uint16 wAddr ) {
	return (wAddr >> 8);
}

/*-------------------------------------------------------------------*/
/*  Mapper 0 Write Function                                          */
/*-------------------------------------------------------------------*/
void Mapper_0_Write( uint16 wAddr, unsigned char byData ) {
/*
 *  Dummy Write to Mapper
 *
 */
}

/*-------------------------------------------------------------------*/
/*  Mapper 0 V-Sync Function                                         */
/*-------------------------------------------------------------------*/
void Mapper_0_VSync() {
/*
 *  Dummy Callback at VSync
 *
 */
}

/*-------------------------------------------------------------------*/
/*  Mapper 0 H-Sync Function                                         */
/*-------------------------------------------------------------------*/
void Mapper_0_HSync() {
/*
 *  Dummy Callback at HSync
 *
 */
}