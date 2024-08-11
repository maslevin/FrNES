#include "Mapper_4.h"

uint8  Mapper_4_patch;
uint8  Mapper_4_regs[8];

uint32 Mapper_4_prg0,Mapper_4_prg1;
uint32 Mapper_4_chr01,Mapper_4_chr23,Mapper_4_chr4,Mapper_4_chr5,Mapper_4_chr6,Mapper_4_chr7;

uint32 Mapper_4_chr_swap() { return Mapper_4_regs[0] & 0x80; }
uint32 Mapper_4_prg_swap() { return Mapper_4_regs[0] & 0x40; }

uint8 Mapper_4_irq_enabled; // IRQs enabled
uint8 Mapper_4_irq_counter; // IRQ scanline counter, decreasing
uint8 Mapper_4_irq_latch;   // IRQ scanline counter latch

uint8 Mapper_4_vs_index; // VS Atari RBI Baseball and VS TKO Boxing

extern uint32 currentCRC32;
extern unsigned char ROM_Mirroring;

void MMC3_set_CPU_banks()
{
  if(Mapper_4_prg_swap())
  {
	ROMBANK0 = ROMLASTPAGE(1);
	ROMBANK1 = ROMPAGE(Mapper_4_prg1);
	ROMBANK2 = ROMPAGE(Mapper_4_prg0);
	ROMBANK3 = ROMLASTPAGE(0);
  }
  else
  {
	ROMBANK0 = ROMPAGE(Mapper_4_prg0);
	ROMBANK1 = ROMPAGE(Mapper_4_prg1);
	ROMBANK2 = ROMLASTPAGE(1);
	ROMBANK3 = ROMLASTPAGE(0);
  }
}

void MMC3_set_PPU_banks()
{
//  if(NesHeader.byVRomSize > 0)
  {
    if(Mapper_4_chr_swap())
    {
		PPUBANK[0] = &VROM[Mapper_4_chr4 * 0x400];
		PPUBANK[1] = &VROM[Mapper_4_chr5 * 0x400];
		PPUBANK[2] = &VROM[Mapper_4_chr6 * 0x400];
		PPUBANK[3] = &VROM[Mapper_4_chr7 * 0x400];
		PPUBANK[4] = &VROM[Mapper_4_chr01 * 0x400];
		PPUBANK[5] = &VROM[(Mapper_4_chr01 + 1) * 0x400];
		PPUBANK[6] = &VROM[Mapper_4_chr23 * 0x400];
		PPUBANK[7] = &VROM[(Mapper_4_chr23 + 1) * 0x400];
    }
    else
    {
		PPUBANK[0] = &VROM[Mapper_4_chr01 * 0x400];
		PPUBANK[1] = &VROM[(Mapper_4_chr01 + 1) * 0x400];
		PPUBANK[2] = &VROM[Mapper_4_chr23 * 0x400];
		PPUBANK[3] = &VROM[(Mapper_4_chr23 + 1) * 0x400];
		PPUBANK[4] = &VROM[Mapper_4_chr4 * 0x400];
		PPUBANK[5] = &VROM[Mapper_4_chr5 * 0x400];
		PPUBANK[6] = &VROM[Mapper_4_chr6 * 0x400];
		PPUBANK[7] = &VROM[Mapper_4_chr7 * 0x400];
    }
  }
/*  else
  {
    if(chr_swap())
    {
      set_VRAM_bank0(0, chr4);
      set_VRAM_bank0(1, chr5);
      set_VRAM_bank0(2, chr6);
      set_VRAM_bank0(3, chr7);
      set_VRAM_bank0(4, chr01+0);
      set_VRAM_bank0(5, chr01+1);
      set_VRAM_bank0(6, chr23+0);
      set_VRAM_bank0(7, chr23+1);
    }
    else
    {
      set_VRAM_bank0(0, chr01+0);
      set_VRAM_bank0(1, chr01+1);
      set_VRAM_bank0(2, chr23+0);
      set_VRAM_bank0(3, chr23+1);
      set_VRAM_bank0(4, chr4);
      set_VRAM_bank0(5, chr5);
      set_VRAM_bank0(6, chr6);
      set_VRAM_bank0(7, chr7);
    }
  } */
}


void Mapper_4_Init() {
  Mapper_4_patch = 0;

  if(currentCRC32 == 0xdebea5a6 || // Ninja Ryukenden 2 - Ankoku no Jashin Ken
     currentCRC32 == 0xc43da8e2 || // Ninja Gaiden 2
     currentCRC32 == 0xc5fea9f2)   // Dai2Ji - Super Robot Taisen
  {
    Mapper_4_patch = 1;
  }

  if(currentCRC32 == 0xd7a97b38)   // Chou Jinrou Senki - Warwolf
  {
    Mapper_4_patch = 2;
  }

  if(currentCRC32 == 0xeb2dba63)   // VS TKO Boxing
  {
    Mapper_4_patch = 3;
    Mapper_4_vs_index = 0;
  }

  if(currentCRC32 == 0x135adf7c)   // VS Atari RBI Baseball
  {
    Mapper_4_patch = 4;
    Mapper_4_vs_index = 0;
  }

  // clear registers FIRST!!!
  for(int i = 0; i < 8; i++) Mapper_4_regs[i] = 0x00;

  // set CPU bank pointers
  Mapper_4_prg0 = 0;
  Mapper_4_prg1 = 1;
  MMC3_set_CPU_banks();

  // set VROM banks
  if(NesHeader.byVRomSize > 0)
  {
    Mapper_4_chr01 = 0;
    Mapper_4_chr23 = 2;
    Mapper_4_chr4  = 4;
    Mapper_4_chr5  = 5;
    Mapper_4_chr6  = 6;
    Mapper_4_chr7  = 7;
    MMC3_set_PPU_banks();
  }
  else
  {
    Mapper_4_chr01 = Mapper_4_chr23 = Mapper_4_chr4 = Mapper_4_chr5 = Mapper_4_chr6 = Mapper_4_chr7 = 0;
  }

  Mapper_4_irq_enabled = 0;
  Mapper_4_irq_counter = 0;
  Mapper_4_irq_latch = 0;
}

unsigned char Mapper_4_Read(uint16 addr)
{
  if(Mapper_4_patch == 3)
  {
    // VS TKO Boxing security
    if(addr == 0x5E00)
    {
      Mapper_4_vs_index = 0;
      return 0x00;
    }
    else if(addr == 0x5E01)
    {
      uint8 security_data[32] = 
      {
        0xff, 0xbf, 0xb7, 0x97, 0x97, 0x17, 0x57, 0x4f,
        0x6f, 0x6b, 0xeb, 0xa9, 0xb1, 0x90, 0x94, 0x14,
        0x56, 0x4e, 0x6f, 0x6b, 0xeb, 0xa9, 0xb1, 0x90,
        0xd4, 0x5c, 0x3e, 0x26, 0x87, 0x83, 0x13, 0x00
      };
      return security_data[(Mapper_4_vs_index++) & 0x1F];
    }
  }
  else if(Mapper_4_patch == 4)
  {
    // VS Atari RBI Baseball security
    if(addr == 0x5E00)
    {
      Mapper_4_vs_index = 0;
      return 0xFF;
    }
    else if(addr == 0x5E01)
    {
      switch(Mapper_4_vs_index++)
      {
        case 0x09:
          return 0x6F;
        default:
          return 0xB4;
      }
    }
  }
  return  (uint8)(addr >> 8);
}

void Mapper_4_Write(uint16 addr, unsigned char data)
{
  switch(addr & 0xE001)
  {
    case 0x8000:
      {
        Mapper_4_regs[0] = data;
        MMC3_set_PPU_banks();
        MMC3_set_CPU_banks();
      }
      break;

    case 0x8001:
      {
        uint32 bank_num;

        Mapper_4_regs[1] = data;
        bank_num = Mapper_4_regs[1];

        switch(Mapper_4_regs[0] & 0x07)
        {
          case 0x00:
            {
              //if(num_1k_VROM_banks)
              {
                bank_num &= 0xfe;
                Mapper_4_chr01 = bank_num;
                MMC3_set_PPU_banks();
              }
            }
            break;

          case 0x01:
            {
              //if(num_1k_VROM_banks)
              {
                bank_num &= 0xfe;
                Mapper_4_chr23 = bank_num;
                MMC3_set_PPU_banks();
              }
            }
            break;

          case 0x02:
            {
              //if(num_1k_VROM_banks)
              {
                Mapper_4_chr4 = bank_num;
                MMC3_set_PPU_banks();
              }
            }
            break;

          case 0x03:
            {
              //if(num_1k_VROM_banks)
              {
                Mapper_4_chr5 = bank_num;
                MMC3_set_PPU_banks();
              }
            }
            break;

          case 0x04:
            {
              //if(num_1k_VROM_banks)
              {
                Mapper_4_chr6 = bank_num;
                MMC3_set_PPU_banks();
              }
            }
            break;

          case 0x05:
            {
              //if(num_1k_VROM_banks)
              {
                Mapper_4_chr7 = bank_num;
                MMC3_set_PPU_banks();
              }
            }
            break;

          case 0x06:
            {
              Mapper_4_prg0 = bank_num;
              MMC3_set_CPU_banks();
            }
            break;

          case 0x07:
            {
              Mapper_4_prg1 = bank_num;
              MMC3_set_CPU_banks();
            }
            break;
        }
      }
      break;

    case 0xA000:
      {
        Mapper_4_regs[2] = data;

        if(data & 0x40)
        {
          //LOG("MAP4 MIRRORING: 0x40 ???\n");
        }

        if(ROM_Mirroring != MIRRORING_FOUR_SCREEN)
        {
          if(data & 0x01)
          {
			pNesX_Mirroring(MIRRORING_HORIZONTAL);
          }
          else
          {
			pNesX_Mirroring(MIRRORING_VERTICAL);
          }
        }
      }
      break;

    case 0xA001:
      {
        Mapper_4_regs[3] = data;

        if(data & 0x80)
        {
          // enable save RAM $6000-$7FFF
        }
        else
        {
          // disable save RAM $6000-$7FFF
        }
      }
      break;

    case 0xC000:
      Mapper_4_regs[4] = data;
      Mapper_4_irq_counter = Mapper_4_regs[4];
      break;

    case 0xC001:
      Mapper_4_regs[5] = data;
      Mapper_4_irq_latch = Mapper_4_regs[5];
      break;

    case 0xE000:
      Mapper_4_regs[6] = data;
      Mapper_4_irq_enabled = 0;
      break;

    case 0xE001:
      Mapper_4_regs[7] = data;
      Mapper_4_irq_enabled = 1;
      break;
      
/*	  
    default:
      LOG("MAP4: UNKNOWN");
      LOGHEX(addr);
      LOGHEX(data);
      break;
*/	  
  }
}

void Mapper_4_HSync()
{
  if(Mapper_4_irq_enabled)
  {
    if((ppuinfo.PPU_Scanline >= 0) && (ppuinfo.PPU_Scanline <= 239))
    {
      if (PPU_R1 & (R1_SHOW_SCR | R1_SHOW_SP ))
      {
        if(Mapper_4_patch == 1)
        {
          if(!(--Mapper_4_irq_counter))
          {
            Mapper_4_irq_counter = Mapper_4_irq_latch;
			IRQ_REQ;
          }
        }
        else if(Mapper_4_patch == 2)
        {
          if(--Mapper_4_irq_counter == 0x01)
          {
            Mapper_4_irq_counter = Mapper_4_irq_latch;
			IRQ_REQ;
          }
        }
        else
        {
          if(!(Mapper_4_irq_counter--))
          {
            Mapper_4_irq_counter = Mapper_4_irq_latch;
            IRQ_REQ;
          }
        }
      }
    }
  }
}