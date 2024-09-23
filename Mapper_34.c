#include "Mapper_34.h"

void Mapper_34_Init() {
    ROMBANK0 = ROMPAGE( 0 );
	ROMBANK1 = ROMPAGE( 1 );
	ROMBANK2 = ROMLASTPAGE(1);
	ROMBANK3 = ROMLASTPAGE(0);

    if (NesHeader.byVRomSize > 0) {
        for ( int nPage = 0; nPage < 8; ++nPage )
            PPUBANK[ nPage ] = &VROM[ nPage * 0x400 ];	
    }
}

void Mapper_34_Write( uint16 wAddr, unsigned char byData ) {
    switch(wAddr) {
        case 0x7FFE: {
            PPUBANK[0] = VROMPAGE( byData * 4 );
            PPUBANK[1] = VROMPAGE( (byData * 4) + 1 );
            PPUBANK[2] = VROMPAGE( (byData * 4) + 2 ); 
            PPUBANK[3] = VROMPAGE( (byData * 4) + 3 );
        } break;

        case 0x7FFF: {
            PPUBANK[4] = VROMPAGE( byData * 4 );
            PPUBANK[5] = VROMPAGE( (byData * 4) + 1 );
            PPUBANK[6] = VROMPAGE( (byData * 4) + 2 ); 
            PPUBANK[7] = VROMPAGE( (byData * 4) + 3 );
        } break;

        case 0x7FFD:
        default: {
            ROMBANK0 = ROMPAGE( byData * 4 );
            ROMBANK1 = ROMPAGE( (byData * 4) + 1 );
            ROMBANK2 = ROMPAGE( (byData * 4) + 2 );
            ROMBANK3 = ROMPAGE( (byData * 4) + 3 );
        } break;
    }
}