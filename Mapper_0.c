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
	/* Set ROM Banks */
	if (num_8k_ROM_pages > 3) {
		set_cpu_banks_to_rom_pages(0, 1, 2, 3);
	} else if (num_8k_ROM_pages > 1) {
		set_cpu_banks_to_rom_pages(0, 1, 0, 1);
	} else {
		set_cpu_banks_to_rom_pages(0, 0, 0, 0);
	}

	/* Set PPU Banks */
	set_ppu_banks_low_to_vrom_pages(0, 1, 2, 3);
	set_ppu_banks_high_to_vrom_pages(4, 5, 6, 7);
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