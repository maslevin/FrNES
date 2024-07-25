#include "Mapper_1.h"

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
void Mapper_1_Init() {
    int nPage;

    memset( Map1_Reg, 0, sizeof Map1_Reg );
    Map1_Cnt = Map1_Latch = 0;

    /* Set ROM Banks */
    ROMBANK0 = ROMPAGE( 0 );
    ROMBANK1 = ROMPAGE( 1 );
    ROMBANK2 = ROMLASTPAGE( 1 );
    ROMBANK3 = ROMLASTPAGE( 0 );

    /* Set PPU VROM Banks */
    if ( NesHeader.byVRomSize > 0 ) {
        for ( nPage = 0; nPage < 8; ++nPage )
            PPUBANK[ nPage ] = &VROM[ nPage * 0x400 ];
    }

    /* Set up wiring of the interrupt pin */
    K6502_Set_Int_Wiring( 1, 1 );
}

/*-------------------------------------------------------------------*/
/*  Mapper 1 Write Function                                          */
/*-------------------------------------------------------------------*/
void Mapper_1_Write( uint16 wAddr, unsigned char byData ) {
    int nReg;
    int nROMPos;
    int nBank;
    int nVBank;
    int nPage;

    nReg = ( wAddr >> 13 ) - 4;

    if ( byData & 0x80 ) {
        // Reset
        Map1_Reg[ 0 ] |= 0xc;
        Map1_Cnt = 0;
        Map1_Latch = 0;
        return;
    } else {
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
    if ( NesHeader.byRomSize == 32 ) {
        /* 512K Program ROM */
        // 0 (First 256K) or 32 (Second 256K)
        nROMPos = ( Map1_Reg[ 1 ] & 0x10 ) << 1;
    } else if ( NesHeader.byRomSize == 64 ) {
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
    if ( !( Map1_Reg[ 0 ] & 8 ) ) {
        // 32K ROM Bank
        /* Set ROM Banks */
        //    nBank = ( ( Map1_Reg[ 3 ] << 2 ) + nROMPos ) % ( NesHeader.byRomSize << 1 );
        nBank = ( ( Map1_Reg[ 3 ] << 2 ) + nROMPos );

        ROMBANK0 = ROMPAGE( nBank );
        ROMBANK1 = ROMPAGE( nBank + 1 );
        ROMBANK2 = ROMPAGE( nBank + 2 );
        ROMBANK3 = ROMPAGE( nBank + 3 );
    } else if ( Map1_Reg[ 0 ] & 4 ) {
        // 16K ROM Bank at 0x8000
        nBank = ( ( Map1_Reg[ 3 ] << 1 ) + nROMPos );
        ROMBANK0 = ROMPAGE( nBank );
        ROMBANK1 = ROMPAGE( nBank + 1 );
        ROMBANK2 = ROMLASTPAGE( 1 );
        ROMBANK3 = ROMLASTPAGE( 0 );
    } else {
        // 16K ROM Bank at 0xc000
        nBank = ( ( Map1_Reg[ 3 ] << 1 ) + nROMPos );
        ROMBANK0 = ROMPAGE( 0 );
        ROMBANK1 = ROMPAGE( 1 );
        ROMBANK2 = ROMPAGE( nBank );
        ROMBANK3 = ROMPAGE( nBank + 1 );
    }

    // Select PPU VROM Bank
    if ( NesHeader.byVRomSize > 0 ) {
        if ( Map1_Reg[ 0 ] & 0x10 ) {
            // 4K VROM Switching
            if (nReg == 1) {
                nVBank = Map1_Reg[ 1 ] << 2;
                for ( nPage = 0; nPage < 4; ++nPage )
                    PPUBANK[ nPage ] = &VROM[ (nVBank + nPage) * 0x400 ];
            } else {
                nVBank = Map1_Reg[ 2 ] << 2;
                for ( nPage = 0; nPage < 4; ++nPage )
                    PPUBANK[ nPage + 4 ] = &VROM[ (nVBank + nPage) * 0x400 ];
            }
        } else {
            // 8K VROM Switching
            nVBank = ( Map1_Reg[ 1 ] & 0xe ) << 2;
            for ( nPage = 0; nPage < 8; ++nPage )
                PPUBANK[ nPage ] = &VROM[ (nVBank + nPage) * 0x400 ];
        }
    }
}
