#include "Mapper_34.h"

void Mapper_34_Init() {
    ROMBANK0 = ROMPAGE( 0 );
	ROMBANK1 = ROMPAGE( 1 );
	ROMBANK2 = ROMPAGE( 2 );
	ROMBANK3 = ROMPAGE( 3 );

    printf("ROMBANK0: ");
    for (uint32 i = 0; i < 32; i++) {
        printf("$%02X ", ROMBANK0[i]);
    }
    printf("\n");
    printf("ROMBANK1: ");
    for (uint32 i = 0; i < 32; i++) {
        printf("$%02X ", ROMBANK1[i]);
    }
    printf("\n");
    printf("ROMBANK2: ");
    for (uint32 i = 0; i < 32; i++) {
        printf("$%02X ", ROMBANK2[i]);
    }
    printf("\n");
    printf("ROMBANK3: ");
    for (uint32 i = 0; i < 32; i++) {
        printf("$%02X ", ROMBANK3[i]);
    }
    printf("\n");            

//    if (NesHeader.byVRomSize > 0) {
        for ( int nPage = 0; nPage < 8; ++nPage )
            PPUBANK[ nPage ] = &VROM[ nPage * 0x400 ];	
//    }
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
            uint8 basePage = ((byData << 2) & ((NesHeader.byRomSize << 1) - 1));

            ROMBANK0 = ROMPAGE( basePage );
            ROMBANK1 = ROMPAGE( basePage + 1 );
            ROMBANK2 = ROMPAGE( basePage + 2 );
            ROMBANK3 = ROMPAGE( basePage + 3 );

            printf("Bank Change to bank [%u]\n", basePage);
            printf("ROMBANK0: ");
            for (uint32 i = 0; i < 32; i++) {
                printf("$%02X ", ROMBANK0[i]);
            }
            printf("\n");
            printf("ROMBANK1: ");
            for (uint32 i = 0; i < 32; i++) {
                printf("$%02X ", ROMBANK1[i]);
            }
            printf("\n");
            printf("ROMBANK2: ");
            for (uint32 i = 0; i < 32; i++) {
                printf("$%02X ", ROMBANK2[i]);
            }
            printf("\n");
            printf("ROMBANK3: ");
            for (uint32 i = 0; i < 32; i++) {
                printf("$%02X ", ROMBANK3[i]);
            }
            printf("\n");               
        } break;
    }
}