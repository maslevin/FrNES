/*===================================================================*/
/*                                                                   */
/*  K6502_rw.c : NES 6502 read/write functions                       */
/*                                                                   */
/*  1999/11/03  Racoon  New preparation                              */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#include "K6502_rw.h"
#include "pNesX_Sound_APU.h"
#include "macros.h"

extern int SpriteHitPos;
extern uint16* opt_SoundEnabled;

/*===================================================================*/
/*                                                                   */
/*            K6502_ReadZp() : Reading from the zero page            */
/*                                                                   */
/*===================================================================*/
inline unsigned char K6502_ReadZp( unsigned char byAddr )
{
/*
 *  Reading from the zero page
 *
 *  Parameters
 *    unsigned char byAddr              (Read)
 *      An address inside the zero page
 *
 *  Return values
 *    Read Data
 */

  return RAM[ byAddr ];
}

/*===================================================================*/
/*                                                                   */
/*               K6502_Read() : Reading operation                    */
/*                                                                   */
/*===================================================================*/
inline unsigned char K6502_Read( uint16 wAddr )
{
/*
 *  Reading operation
 *
 *  Parameters
 *    uint16 wAddr              (Read)
 *      Address to read
 *
 *  Return values
 *    Read data
 *
 *  Remarks
 *    0x0000 - 0x1fff  RAM ( 0x800 - 0x1fff is mirror of 0x0 - 0x7ff )
 *    0x2000 - 0x3fff  PPU
 *    0x4000 - 0x5fff  Sound
 *    0x6000 - 0x7fff  SRAM ( Battery Backed )
 *    0x8000 - 0xffff  ROM
 *
 */
  unsigned char byRet;

  switch ( wAddr & 0xE000 )
  {
    case 0x0000:  /* RAM */
        return RAM[ wAddr & 0x7ff ];

    case 0x2000: {
      switch (wAddr) {
        case 0x2002: {
            /* PPU Status $2002*/
            // Set return value
            byRet = PPU_R2;

            // Reset a V-Blank flag
            PPU_R2 &= ~R2_IN_VBLANK;

            // Reset address latch
            PPU_Latch_Flag = 0;

            // Make a Nametable 0 in V-Blank
            if ( ppuinfo.PPU_Scanline >= SCAN_VBLANK_START && !( ppuinfo.PPU_R0 & R0_NMI_VB ) )
            {
              ppuinfo.PPU_R0 &= ~R0_NAME_ADDR;
            }

            //Set the Sprite Hit Flag
            if (SpriteHitPos == 1)
              byRet |= R2_HIT_SP;

            return byRet;
        }

        case 0x2004: {
            /* PPU Sprite RAM $2004 */
            return SPRRAM[PPU_R3];
        }

        case 0x2007: {
            uint16 addr;
            addr = ppuinfo.PPU_Addr;
            // Increment PPU Address
            ppuinfo.PPU_Addr += PPU_Increment;
            addr &= 0x3fff;
        
            if (addr >= 0x3000) {
              if (addr >= 0x3f00) {
                PPU_R7 = PPUBANK[ (addr - 0x1000) >> 10 ][ (addr - 0x1000) & 0x3ff ];
                return PPURAM[addr];
              }

              // handle mirroring
              addr &= 0xefff;
            }

            byRet = PPU_R7;
            PPU_R7 = PPUBANK[ addr >> 10 ][ addr & 0x3ff ];
            return byRet;
        }
      }
    } break;

    case 0x4000:  /* Sound */
      if ( wAddr == 0x4016 ) {
        // Set Joypad1 data
        byRet = (unsigned char)( ( PAD1_Latch >> PAD1_Bit ) & 1 ) | 0x40;
        PAD1_Bit = ( PAD1_Bit == 23 ) ? 0 : ( PAD1_Bit + 1 );
        return byRet;
      } else if ( wAddr == 0x4017 ) {
        // Set Joypad2 data
        byRet = (unsigned char)( ( PAD2_Latch >> PAD2_Bit ) & 1 ) | 0x40;
        PAD2_Bit = ( PAD2_Bit == 23 ) ? 0 : ( PAD2_Bit + 1 );
        return byRet;
      } else if (wAddr < 0x4016) {
		    return audio_read(wAddr);
      } else if (wAddr < 0x6000) {
        return MapperRead(wAddr);
      }
	  break;
      // The other sound registers are not readable.

    case 0x6000:  /* SRAM */
      return SRAM[ wAddr & 0x1fff ];

    case 0x8000:  /* ROM BANK 0 */
      return ROMBANK0[ wAddr & 0x1fff ];

    case 0xa000:  /* ROM BANK 1 */
      return ROMBANK1[ wAddr & 0x1fff ];

    case 0xc000:  /* ROM BANK 2 */
      return ROMBANK2[ wAddr & 0x1fff ];

    case 0xe000:  /* ROM BANK 3 */
      return ROMBANK3[ wAddr & 0x1fff ];
  }

  return ( wAddr >> 8 ); /* when a register is not readable the upper half
                            address is returned. */
}

/*===================================================================*/
/*                                                                   */
/*               K6502_Write() : Writing operation                    */
/*                                                                   */
/*===================================================================*/
inline void K6502_Write( uint16 wAddr, unsigned char byData )
{
/*
 *  Writing operation
 *
 *  Parameters
 *    uint16 wAddr              (Read)
 *      Address to write
 *
 *    unsigned char byData             (Read)
 *      Data to write
 *
 *  Remarks
 *    0x0000 - 0x1fff  RAM ( 0x800 - 0x1fff is mirror of 0x0 - 0x7ff )
 *    0x2000 - 0x3fff  PPU
 *    0x4000 - 0x5fff  Sound
 *    0x6000 - 0x7fff  SRAM ( Battery Backed )
 *    0x8000 - 0xffff  ROM
 *
 */

  switch ( wAddr & 0xe000 )
  {
    case 0x0000:  /* RAM */
      RAM[ wAddr & 0x7ff ] = byData;
      break;

    case 0x2000:  /* PPU */
      switch ( wAddr & 0x7 )
      {
        case 0:    /* 0x2000 */
          ppuinfo.PPU_R0 = byData;
          PPU_Increment = ( ppuinfo.PPU_R0 & R0_INC_ADDR ) ? 32 : 1;
          ppuinfo.PPU_BG_Base = ( ppuinfo.PPU_R0 & R0_BG_ADDR ) ? ChrBuf + 256 * 64 : ChrBuf;
          ppuinfo.PPU_SP_Base = ( ppuinfo.PPU_R0 & R0_SP_ADDR ) ? ChrBuf + 256 * 64 : ChrBuf;
          ppuinfo.PPU_SP_Height = ( ppuinfo.PPU_R0 & R0_SP_SIZE ) ? 16 : 8;

          //Account for Loopy's scrolling discoveries
          PPU_Temp = (PPU_Temp & 0xF3FF) | ( (((uint16)byData) & 0x0003) << 10);
          break;

        case 1:   /* 0x2001 */
		      PPU_R1 = byData;
          break;

        case 2:   /* 0x2002 */
          //PPU_R2 = byData; // 0x2002 is not writable
          break;

        case 3:   /* 0x2003 */
          // Sprite RAM Address
          PPU_R3 = byData;
          break;

        case 4:   /* 0x2004 */
          // Write data to Sprite RAM
          SPRRAM[ PPU_R3++ ] = byData;
          break;

        case 5:   /* 0x2005 */
          // Set Scroll Register
          if ( PPU_Latch_Flag )
          {
            //Added : more Loopy Stuff
            PPU_Temp = (PPU_Temp & 0xFC1F) | ( (((uint16)byData) & 0xF8) << 2);
            PPU_Temp = (PPU_Temp & 0x8FFF) | ( (((uint16)byData) & 0x07) << 12);
          }
          else
          {
            ppuinfo.PPU_Scr_H_Bit = byData & 7;
            PPU_Temp = (PPU_Temp & 0xFFE0) | ( (((uint16)byData) & 0xF8) >> 3);
	        }
          PPU_Latch_Flag ^= 1;
          break;

        case 6:   /* 0x2006 */
          // Set PPU Address
          if ( PPU_Latch_Flag )
          {
            PPU_Temp = (PPU_Temp & 0xFF00) | ( ((uint16)byData) & 0x00FF);
            ppuinfo.PPU_Addr = PPU_Temp;
          }
          else
          {
            PPU_Temp = (PPU_Temp & 0x00FF) | ( ( ((uint16)byData) & 0x003F) << 8 );
          }
          PPU_Latch_Flag ^= 1;
          break;

        case 7:/* 0x2007 */
          {
            uint16 addr;
            addr = ppuinfo.PPU_Addr;
            ppuinfo.PPU_Addr += PPU_Increment;
            addr &= 0x3FFF;

            if (addr >= 0x3000)
            {
              if (addr >= 0x3F00)
              {
                byData &= 0x3F;
                // If it's the Zero Entry
                if ( !( addr & 0xf ) )  /* 0x3f00 or 0x3f10 */
                {
                  // Palette mirror
                  PPURAM[ 0x3f10 ] = PPURAM[ 0x3f14 ] = PPURAM[ 0x3f18 ] = PPURAM[ 0x3f1c ] = 
                  PPURAM[ 0x3f00 ] = PPURAM[ 0x3f04 ] = PPURAM[ 0x3f08 ] = PPURAM[ 0x3f0c ] = byData;
                  // Flag the High bit so it can be used for transparency purposes
                  PalTable[ 0x00 ] = PalTable[ 0x04 ] = PalTable[ 0x08 ] = PalTable[ 0x0c ] =
                  PalTable[ 0x10 ] = PalTable[ 0x14 ] = PalTable[ 0x18 ] = PalTable[ 0x1c ] = NesPalette[ byData ] | 0x8000;
                }
                else
                if ( addr & 3 )
                {
                  // Palette
                  PPURAM[ addr ] = byData;
                  PalTable[ addr & 0x1f ] = NesPalette[ byData ];
                }
                return;
              }
              addr &= 0xEFFF;
            }

            PPUBANK[addr >> 10][addr & 0x3FF] = byData;
            ChrBufUpdate |= ( 1 << ( addr >> 10 ) );

            // Increment PPU Address
            //PPU_Addr += PPU_Increment;
            //PPU_Addr &= 0x3fff;
                break;
          }
      }
      break;

    case 0x4000:  /* Sound */
      switch ( wAddr & 0x1f )
      {
        case 0x14:  /* 0x4014 */
          // Sprite DMA
          switch ( byData >> 5 )
          {
            case 0x0:  /* RAM */
              pNesX_MemoryCopy_Offset( (uint32*) SPRRAM, (uint32*) &RAM[ ( (uint16)byData << 8 ) & 0x7ff ], SPRRAM_SIZE, PPU_R3 );
              break;

            case 0x3:  /* SRAM */
              pNesX_MemoryCopy_Offset( (uint32*) SPRRAM, (uint32*) &SRAM[ ( (uint16)byData << 8 ) & 0x1fff ], SPRRAM_SIZE, PPU_R3 );
              break;

            case 0x4:  /* ROM BANK 0 */
              pNesX_MemoryCopy_Offset( (uint32*) SPRRAM, (uint32*) &ROMBANK0[ ( (uint16)byData << 8 ) & 0x1fff ], SPRRAM_SIZE, PPU_R3 );
              break;

            case 0x5:  /* ROM BANK 1 */
              pNesX_MemoryCopy_Offset( (uint32*) SPRRAM, (uint32*) &ROMBANK1[ ( (uint16)byData << 8 ) & 0x1fff ], SPRRAM_SIZE, PPU_R3 );
              break;

            case 0x6:  /* ROM BANK 2 */
              pNesX_MemoryCopy_Offset( (uint32*) SPRRAM, (uint32*) &ROMBANK2[ ( (uint16)byData << 8 ) & 0x1fff ], SPRRAM_SIZE, PPU_R3 );
              break;

            case 0x7:  /* ROM BANK 3 */
              pNesX_MemoryCopy_Offset( (uint32*) SPRRAM, (uint32*) &ROMBANK3[ ( (uint16)byData << 8 ) & 0x1fff ], SPRRAM_SIZE, PPU_R3 );
              break;
          }
          break;

        case 0x16:  /* 0x4016 */
          // Reset joypad
          if ( !( APU_Reg[ 0x16 ] & 1 ) && ( byData & 1 ) )
          {
            PAD1_Bit = 0;
            PAD2_Bit = 0;
          }
          break;
		    default:
          APU_Reg[ wAddr & 0x1f ] = byData;
          if (*opt_SoundEnabled) {
            audio_write(wAddr, byData);
          }
          break;
      }
      MapperWrite(wAddr, byData);
      // Set Bank Table
      VIRPC;
      BankTable[ 4 ] = ROMBANK0;
      BankTable[ 5 ] = ROMBANK1;
      BankTable[ 6 ] = ROMBANK2;
      BankTable[ 7 ] = ROMBANK3;
      REALPC;      
      break;

    case 0x6000:  /* SRAM */
      SRAM[ wAddr & 0x1fff ] = byData;
      break;

    case 0x8000:  /* ROM BANK 0 */
    case 0xa000:  /* ROM BANK 1 */
    case 0xc000:  /* ROM BANK 2 */
    case 0xe000:  /* ROM BANK 3 */
      // Write to Mapper
      MapperWrite( wAddr, byData );
      
      // Set Bank Table
      VIRPC;
      BankTable[ 4 ] = ROMBANK0;
      BankTable[ 5 ] = ROMBANK1;
      BankTable[ 6 ] = ROMBANK2;
      BankTable[ 7 ] = ROMBANK3;
      REALPC;
      break;
  }
}

// Reading/Writing operation (uint16 version)
inline uint16 K6502_ReadW( uint16 wAddr ){ 
  uint16 nextWAddr = wAddr + 1;
  if ((wAddr & 0xFF) == 0xFF) {
    nextWAddr = wAddr & 0xFF00;
  }
  return K6502_Read( wAddr ) | (uint16)K6502_Read( nextWAddr ) << 8;
};
inline void K6502_WriteW( uint16 wAddr, uint16 wData ){ K6502_Write( wAddr, wData & 0xff ); K6502_Write( wAddr + 1, wData >> 8 ); };
inline uint16 K6502_ReadZpW( unsigned char byAddr ){ return K6502_ReadZp( byAddr ) | ( K6502_ReadZp( byAddr + 1 ) << 8 ); };