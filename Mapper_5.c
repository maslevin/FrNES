#include "Mapper_5.h"

#include "nes_apu.h"

/*===================================================================*/
/*                                                                   */
/*                        Mapper 5 (MMC5)                            */
/*                                                                   */
/*===================================================================*/

uint32 Mapper_5_wb[8];
unsigned char Mapper_5_wram[8 * 0x2000];
unsigned char Mapper_5_wram_size;
unsigned char Mapper_5_chr_reg[8][2];
unsigned char Mapper_5_irq_enabled;
unsigned char Mapper_5_irq_status;
uint32 Mapper_5_irq_line;
uint32 Mapper_5_value0;
uint32 Mapper_5_value1;
unsigned char Mapper_5_wram_protect0;
unsigned char Mapper_5_wram_protect1;
unsigned char Mapper_5_prg_size;
unsigned char Mapper_5_chr_size;
unsigned char Mapper_5_gfx_mode;
unsigned char Mapper_5_split_control;
unsigned char Mapper_5_split_bank;

extern uint32 currentCRC32;

void Mapper_5_Set_WRAM_Bank(unsigned char page, unsigned char bank) {
	pNesX_DebugPrint("Map5_set_WRAM_bank: %u, %u\n", page, bank);    
	if (bank != 8) {
		if(Mapper_5_wram_size == 1) bank = (bank > 3) ? 8 : 0;
		if(Mapper_5_wram_size == 2) bank = (bank > 3) ? 1 : 0;
		if(Mapper_5_wram_size == 3) bank = (bank > 3) ? 8 : bank;
		if(Mapper_5_wram_size == 4) bank = (bank > 3) ? 4 : bank;
	}
	Mapper_5_wb[page] = bank;

	if (bank != 8) {
		pNesX_DebugPrint("Setting ROM Bank [%u] to WRAM Bank [%u]\n", page, bank);
        switch (page) {
            case 4:
                ROMBANK0 = &Mapper_5_wram[(bank * 0x2000)]; 
                break;
            case 5:
                ROMBANK1 = &Mapper_5_wram[(bank * 0x2000)]; 
                break;            
            case 6:
                ROMBANK2 = &Mapper_5_wram[(bank * 0x2000)]; 
                break;            
            case 7:
                ROMBANK3 = &Mapper_5_wram[(bank * 0x2000)]; 
                break;
        }
	}
}

void Mapper_5_Set_CPU_Bank(unsigned char page, unsigned char bank) {
	//printf("Map5_set_CPU_bank: %u, %u\n", page, bank);
	uint32 num_8k_ROM_banks = NesHeader.byRomSize * 2;
	if (bank & 0x80) {
		switch (Mapper_5_prg_size) {
			case 0:
				if (page == 7) {
					printf("Setting ROMBANKS 0-3 to [%u-%u]\n",(bank & 0x7C) % num_8k_ROM_banks,((bank & 0x7C)+3) % num_8k_ROM_banks);
					ROMBANK0 = ROMPAGE((bank & 0x7C) % num_8k_ROM_banks);
					ROMBANK1 = ROMPAGE(((bank & 0x7C)+1) % num_8k_ROM_banks);
					ROMBANK2 = ROMPAGE(((bank & 0x7C)+2) % num_8k_ROM_banks);
					ROMBANK3 = ROMPAGE(((bank & 0x7C)+3) % num_8k_ROM_banks);
					Mapper_5_wb[4] = Mapper_5_wb[5] = Mapper_5_wb[6] = 8;
				}
				break;
			case 1:
				if (page == 5) {
					printf("Setting ROMBANKS 0-1 to [%u-%u]\n",(bank & 0x7E) % num_8k_ROM_banks,((bank & 0x7E)+1) % num_8k_ROM_banks);
					ROMBANK0 = ROMPAGE((bank & 0x7E) % num_8k_ROM_banks);
					ROMBANK1 = ROMPAGE(((bank & 0x7E)+1) % num_8k_ROM_banks);
					Mapper_5_wb[4] = Mapper_5_wb[5] = 8;
				}
				if (page == 7) {
					printf("Setting ROMBANKS 2-3 to [%u-%u]\n",(bank & 0x7E) % num_8k_ROM_banks,((bank & 0x7E)+1) % num_8k_ROM_banks);					
					ROMBANK2 = ROMPAGE((bank & 0x7E) % num_8k_ROM_banks);
					ROMBANK3 = ROMPAGE(((bank & 0x7E)+1) % num_8k_ROM_banks);
					Mapper_5_wb[6] = 8;
				}			
				break;
			case 2:
				if (page == 5) {
					printf("Setting ROMBANKS 0-1 to [%u-%u]\n",(bank & 0x7F) % num_8k_ROM_banks,((bank & 0x7F) + 1)% num_8k_ROM_banks);
					ROMBANK0 = ROMPAGE((bank & 0x7E) % num_8k_ROM_banks);
					ROMBANK1 = ROMPAGE(((bank & 0x7E)+1) % num_8k_ROM_banks);
					Mapper_5_wb[4] = Mapper_5_wb[5] = 8;
				}
				if (page == 6) {
					printf("Setting ROMBANK 2 to [%u]\n",(bank & 0x7F) % num_8k_ROM_banks);
					ROMBANK2 = ROMPAGE((bank & 0x7F) % num_8k_ROM_banks);
					Mapper_5_wb[6] = 8;
				}
				if (page == 7) {
					printf("Setting ROMBANK 3 to [%u]\n",(bank & 0x7F) % num_8k_ROM_banks);					
					ROMBANK3 = ROMPAGE((bank & 0x7F) % num_8k_ROM_banks);
				}			
				break;
			case 3:
				if (page == 4) {
					printf("Setting ROMBANK 0 to [%u]\n",(bank & 0x7F) % num_8k_ROM_banks);
					ROMBANK0 = ROMPAGE((bank & 0x7F) % num_8k_ROM_banks);
					Mapper_5_wb[4] = 8;
				}
				if (page == 5) {
					printf("Setting ROMBANK 1 to [%u]\n",(bank & 0x7F) % num_8k_ROM_banks);					
					ROMBANK1 = ROMPAGE((bank & 0x7F) % num_8k_ROM_banks);
					Mapper_5_wb[5] = 8;
				}
				if (page == 6) {
					printf("Setting ROMBANK 2 to [%u]\n",(bank & 0x7F) % num_8k_ROM_banks);					
					ROMBANK2 = ROMPAGE((bank & 0x7F) % num_8k_ROM_banks);
					Mapper_5_wb[6] = 8;
				}
				if (page == 7) {
					printf("Setting ROMBANK 3 to [%u]\n",(bank & 0x7F) % num_8k_ROM_banks);					
					ROMBANK3 = ROMPAGE((bank & 0x7F) % num_8k_ROM_banks);
				}			
				break;
		}
	} else {
		switch (Mapper_5_prg_size) {
			case 1:
				if (page == 5) {
					Mapper_5_Set_WRAM_Bank(4, (bank & 0x06));
					Mapper_5_Set_WRAM_Bank(5, (bank & 0x06)+1);
				}			
				break;

			case 2:
				if (page == 5) {
					Mapper_5_Set_WRAM_Bank(4, (bank & 0x06));
					Mapper_5_Set_WRAM_Bank(5, (bank & 0x06)+1);
				}
				if (page == 6) {
					Mapper_5_Set_WRAM_Bank(6, bank & 0x07);
				}			
				break;

			case 3:
				if (page == 4) {
					Mapper_5_Set_WRAM_Bank(4, bank & 0x07);
				}
				if (page == 5) {
					Mapper_5_Set_WRAM_Bank(5, bank & 0x07);
				}
				if (page == 6) {
					Mapper_5_Set_WRAM_Bank(6, bank & 0x07);
				}			
				break;
		}
	}	
}

void Mapper_5_Sync_Chr_Banks(unsigned char mode) {
	pNesX_DebugPrint("Map5_Sync_Chr_Bank: %u\n", mode);    
	switch (Mapper_5_chr_size) {
		case 0:
			PPUBANK[0] = VROMPAGE(Mapper_5_chr_reg[7][mode]*8);
			PPUBANK[1] = VROMPAGE(Mapper_5_chr_reg[7][mode]*8+1);
			PPUBANK[2] = VROMPAGE(Mapper_5_chr_reg[7][mode]*8+2);
			PPUBANK[3] = VROMPAGE(Mapper_5_chr_reg[7][mode]*8+3);
			PPUBANK[4] = VROMPAGE(Mapper_5_chr_reg[7][mode]*8+4);
			PPUBANK[5] = VROMPAGE(Mapper_5_chr_reg[7][mode]*8+5);
			PPUBANK[6] = VROMPAGE(Mapper_5_chr_reg[7][mode]*8+6);
			PPUBANK[7] = VROMPAGE(Mapper_5_chr_reg[7][mode]*8+7);		
			break;

		case 1:
			PPUBANK[0] = VROMPAGE(Mapper_5_chr_reg[3][mode]*4);
			PPUBANK[1] = VROMPAGE(Mapper_5_chr_reg[3][mode]*4+1);
			PPUBANK[2] = VROMPAGE(Mapper_5_chr_reg[3][mode]*4+2);
			PPUBANK[3] = VROMPAGE(Mapper_5_chr_reg[3][mode]*4+3);
			PPUBANK[4] = VROMPAGE(Mapper_5_chr_reg[7][mode]*4);
			PPUBANK[5] = VROMPAGE(Mapper_5_chr_reg[7][mode]*4+1);
			PPUBANK[6] = VROMPAGE(Mapper_5_chr_reg[7][mode]*4+2);
			PPUBANK[7] = VROMPAGE(Mapper_5_chr_reg[7][mode]*4+3);
			break;

		case 2:
			PPUBANK[0] = VROMPAGE(Mapper_5_chr_reg[1][mode]*2);
			PPUBANK[1] = VROMPAGE(Mapper_5_chr_reg[1][mode]*2+1);
			PPUBANK[2] = VROMPAGE(Mapper_5_chr_reg[3][mode]*2);
			PPUBANK[3] = VROMPAGE(Mapper_5_chr_reg[3][mode]*2+1);
			PPUBANK[4] = VROMPAGE(Mapper_5_chr_reg[5][mode]*2);
			PPUBANK[5] = VROMPAGE(Mapper_5_chr_reg[5][mode]*2+1);
			PPUBANK[6] = VROMPAGE(Mapper_5_chr_reg[7][mode]*2);
			PPUBANK[7] = VROMPAGE(Mapper_5_chr_reg[7][mode]*2+1);
			break;

		default:
			PPUBANK[0] = VROMPAGE(Mapper_5_chr_reg[0][mode]);
			PPUBANK[1] = VROMPAGE(Mapper_5_chr_reg[1][mode]);
			PPUBANK[2] = VROMPAGE(Mapper_5_chr_reg[2][mode]);
			PPUBANK[3] = VROMPAGE(Mapper_5_chr_reg[3][mode]);
			PPUBANK[4] = VROMPAGE(Mapper_5_chr_reg[4][mode]);
			PPUBANK[5] = VROMPAGE(Mapper_5_chr_reg[5][mode]);
			PPUBANK[6] = VROMPAGE(Mapper_5_chr_reg[6][mode]);
			PPUBANK[7] = VROMPAGE(Mapper_5_chr_reg[7][mode]);	
			break;
	}
}

unsigned char Mapper_5_PPU_Latch_RenderScreen(uint8 mode, uint32 addr) {
    unsigned char ex_pal = 0;

    if (Mapper_5_gfx_mode == 1 && mode == 1) {
        // ex gfx mode
        unsigned char * nametable2 = PPUBANK[NAME_TABLE2];
        uint32 bank = (nametable2[addr] & 0x3F) << 2;
        PPUBANK[0] = VROMPAGE(bank);
        PPUBANK[1] = VROMPAGE(bank + 1);
        PPUBANK[2] = VROMPAGE(bank + 2);
        PPUBANK[3] = VROMPAGE(bank + 3);
        PPUBANK[4] = VROMPAGE(bank);
        PPUBANK[5] = VROMPAGE(bank + 1);
        PPUBANK[6] = VROMPAGE(bank + 2);
        PPUBANK[7] = VROMPAGE(bank + 3);	
        ex_pal = ((nametable2[addr] & 0xC0) >> 4) | 0x01;
    } else {
        // normal
        Mapper_5_Sync_Chr_Banks(mode);
    }
    return ex_pal;
}

/*-------------------------------------------------------------------*/
/*  Mapper 5 Init Function                                           */
/*-------------------------------------------------------------------*/
void Mapper_5_Init() {
	Mapper_5_wram_size = 1;

	//MS - this should probably move to the ROM loader?
	// NES 2.0 I think has information for this so we don't need to special case by CRC of the ROM
    if (currentCRC32 == 0x2b548d75 || // Bandit Kings of Ancient China (J)
    	currentCRC32 == 0xf4cd4998 || // Dai Koukai Jidai (J)
    	currentCRC32 == 0x8fa95456 || // Ishin no Arashi (J)
    	currentCRC32 == 0x98c8e090 || // Nobunaga no Yabou - Sengoku Gunyuu Den (J)
    	currentCRC32 == 0x57e3218b || // L'Empereur (J)
    	currentCRC32 == 0x2f50bd38 || // L'Empereur (U)
    	currentCRC32 == 0x8e9a5e2f || // L'Empereur (Alt)(U)
    	currentCRC32 == 0xb56958d1 || // Nobunaga's Ambition 2 (J)
    	currentCRC32 == 0xe6c28c5f || // Suikoden - Tenmei no Chikai (J)
    	currentCRC32 == 0xcd35e2e9) {  // Uncharted Waters (J)
	    Mapper_5_wram_size = 2;
    }

	if (currentCRC32 == 0xf4120e58 || // Aoki Ookami to Shiroki Mejika - Genchou Hishi (J)
    	currentCRC32 == 0x286613d8 || // Nobunaga no Yabou - Bushou Fuuun Roku (J)
    	currentCRC32 == 0x11eaad26 || // Romance of the 3 Kingdoms 2 (J)
    	currentCRC32 == 0x95ba5733) {  // Sangokushi 2 (J)
		Mapper_5_wram_size = 3;
	}

	uint32 i;
	// set SaveRAM	
	for(i = 0; i < 0x10000; i++) {
		Mapper_5_wram[i] = SRAM[i];
	}
	Mapper_5_Set_WRAM_Bank(3,0);

	apu_set_exsound(NES_APU_EXSOUND_MMC5);

	// set CPU bank pointers
	ROMBANK0 = ROMLASTPAGE(0);
	ROMBANK1 = ROMLASTPAGE(0);
	ROMBANK2 = ROMLASTPAGE(0);
	ROMBANK3 = ROMLASTPAGE(0);

	// set PPU bank pointers
	PPUBANK[ 0 ] = VROMPAGE( 0 );
	PPUBANK[ 1 ] = VROMPAGE( 1 );
	PPUBANK[ 2 ] = VROMPAGE( 2 );
	PPUBANK[ 3 ] = VROMPAGE( 3 );
	PPUBANK[ 4 ] = VROMPAGE( 4 );
	PPUBANK[ 5 ] = VROMPAGE( 5 );
	PPUBANK[ 6 ] = VROMPAGE( 6 );
	PPUBANK[ 7 ] = VROMPAGE( 7 );	

	for(i = 0; i < 8; i++) {
		Mapper_5_chr_reg[i][0] = i;
		Mapper_5_chr_reg[i][1] = (i & 0x03) + 4;
	}
	Mapper_5_wb[3] = 0;
	Mapper_5_wb[4] = Mapper_5_wb[5] = Mapper_5_wb[6] = 8;

	Mapper_5_prg_size = 3;
	Mapper_5_wram_protect0 = 0x02;
	Mapper_5_wram_protect1 = 0x01;
	Mapper_5_chr_size = 3;
	Mapper_5_gfx_mode = 0;

	Mapper_5_irq_enabled = 0;
	Mapper_5_irq_status = 0;
	Mapper_5_irq_line = 0;

	Mapper_5_split_control = 0;
	Mapper_5_split_bank = 0;

    /* Set up wiring of the interrupt pin */
    K6502_Set_Int_Wiring( 1, 1 );    
}

/*-------------------------------------------------------------------*/
/*  Mapper 5 Write Function                                          */
/*-------------------------------------------------------------------*/
void Mapper_5_Write(uint16 wAddr, unsigned char byData) {
	pNesX_DebugPrint("Map5_Write: $%04X, %02X\n", wAddr, byData);	
	uint32 i;

	switch(wAddr) {
		case 0x5100: {
			printf("Set Prg Size [%u]\n", byData & 0x03);
			Mapper_5_prg_size = byData & 0x03;
		} break;

		case 0x5101: {
			printf("Set Chr Size [%u]\n", byData & 0x03);
			Mapper_5_chr_size = byData & 0x03;
		} break;

		case 0x5102: {
			printf("Set Wram Protection 0 [%u]\n", byData & 0x03);
			Mapper_5_wram_protect0 = byData & 0x03;
		} break;

		case 0x5103: {
			printf("Set Wram Protection 1 [%u]\n", byData & 0x03);			
			Mapper_5_wram_protect1 = byData & 0x03;
		} break;

		case 0x5104: {
			printf("Set Gfx Mode [%u]\n", byData & 0x03);
			Mapper_5_gfx_mode = byData & 0x03;
		} break;

		case 0x5105: {
			printf("Set Mirroring [%02x]\n", byData);
			PPUBANK[ NAME_TABLE0 ] = &PPURAM[ (NAME_TABLE0 + (byData & 0x03)) * 0x400 ];
			byData >>= 2;			
			PPUBANK[ NAME_TABLE1 ] = &PPURAM[ (NAME_TABLE0 + (byData & 0x03)) * 0x400 ];
			byData >>= 2;			
			PPUBANK[ NAME_TABLE2 ] = &PPURAM[ (NAME_TABLE0 + (byData & 0x03)) * 0x400 ];
			byData >>= 2;			
			PPUBANK[ NAME_TABLE3 ] = &PPURAM[ (NAME_TABLE0 + (byData & 0x03)) * 0x400 ];
		} break;

		case 0x5106: {
			printf("Write to Nametable3 Low [%u]\n", byData & 0x03);			
			unsigned char* nametable3 = PPUBANK[NAME_TABLE3];
			for (i = 0; i < 0x3C0; i++) {
				nametable3[i] = byData;
			}
		} break;

		case 0x5107: {
			printf("Write to Nametable3 High [%u]\n", byData & 0x03);			
			unsigned char* nametable3 = PPUBANK[NAME_TABLE3];
			byData &= 0x03;
			byData = byData | (byData<<2) | (byData<<4) | (byData<<6);
			for (i = 0x3C0; i < 0x400; i++) {
				nametable3[i] = byData;
			}
		} break;

		case 0x5113: {
			printf("Write to WRAM Bank [%u]\n", byData & 0x07);
			Mapper_5_Set_WRAM_Bank(3, byData & 0x07);
		} break;

		case 0x5114:
		case 0x5115:
		case 0x5116:
		case 0x5117: {
			printf("Write to CPU Bank [%u]\n", byData & 0x07);			
			Mapper_5_Set_CPU_Bank(wAddr & 0x07, byData);
		} break;

		case 0x5120:
		case 0x5121:
		case 0x5122:
		case 0x5123:
		case 0x5124:
		case 0x5125:
		case 0x5126:
		case 0x5127: {
			printf("Write to Chr Registers [%u]\n", byData & 0x07);
			Mapper_5_chr_reg[wAddr & 0x07][0] = byData;
			Mapper_5_Sync_Chr_Banks(0);
		} break;

		case 0x5128:
		case 0x5129:
		case 0x512A:
		case 0x512B: {
			printf("Write to Chr Registers 2 [%u]\n", byData & 0x07);
			Mapper_5_chr_reg[(wAddr & 0x03) + 0][1] = byData;
			Mapper_5_chr_reg[(wAddr & 0x03) + 4][1] = byData;
		} break;

		case 0x5200: {
			printf("Write to Split Control [%u]\n", byData & 0x07);			
			Mapper_5_split_control = byData;
		} break;

		case 0x5201: {
		// Intentionally commented out from Nester Source
		//split_scroll = data;
		} break;

		case 0x5202: {
			Mapper_5_split_bank = byData & 0x3F;
        } break;

		case 0x5203: {
			Mapper_5_irq_line = byData;
		} break;

		case 0x5204: {
			Mapper_5_irq_enabled = byData;
		} break;

		case 0x5205: {
			Mapper_5_value0 = byData;
		} break;

		case 0x5206: {
			Mapper_5_value1 = byData;
		} break;

		default: {
			if (wAddr >= 0x5000 && wAddr <= 0x5015) {
				ex_write(wAddr, byData);
			}
			else if (wAddr >= 0x5C00 && wAddr <= 0x5FFF) {
				if (Mapper_5_gfx_mode != 3) {			
					uint8* nametable2 = PPUBANK[NAME_TABLE2];
					nametable2[wAddr & 0x3FF] = byData; //(irq_status & 0) ? data : 0x40;
				}
			} else if (wAddr >= 0x8000 && wAddr <= 0x9FFF) {
				if (Mapper_5_wb[4] != 8) {
					Mapper_5_wram[Mapper_5_wb[4]*0x2000+(wAddr&0x1FFF)] = byData;
					SRAM[Mapper_5_wb[4]*0x2000+(wAddr&0x1FFF)] = byData;
				}
			} else if (wAddr >= 0xA000 && wAddr <= 0xBFFF) {
				if (Mapper_5_wb[5] != 8) {
					Mapper_5_wram[Mapper_5_wb[5]*0x2000+(wAddr&0x1FFF)] = byData;
					SRAM[Mapper_5_wb[5]*0x2000+(wAddr&0x1FFF)] = byData;
				}
			} else if(wAddr >= 0xC000 && wAddr <= 0xDFFF) {
				if (Mapper_5_wb[6] != 8) {
					Mapper_5_wram[Mapper_5_wb[6]*0x2000+(wAddr&0x1FFF)] = byData;
					SRAM[Mapper_5_wb[6]*0x2000+(wAddr&0x1FFF)] = byData;
				}
			}
		} break;
	}
}

/*-------------------------------------------------------------------*/
/*  Mapper 5 Read Function                                           */
/*-------------------------------------------------------------------*/
unsigned char Mapper_5_Read( uint16 wAddr ) {
    pNesX_DebugPrint("Map5_Read: $%04X\n", wAddr);
    uint8 ret = (uint8)(wAddr >> 8);
    switch (wAddr) {
        case 0x5204: {
            ret = Mapper_5_irq_status;
            Mapper_5_irq_status &= ~0x80;
        } break;

        case 0x5205: {
            ret = (uint8)((Mapper_5_value0 * Mapper_5_value1) & 0x00FF);
        } break;

        case 0x5206: {
            ret = (uint8)(((Mapper_5_value0 * Mapper_5_value1) & 0xFF00) >> 8);
        } break;

        default: {
            if ((wAddr >= 0x5C00) && (wAddr <=0x5FFF)) {
                if (Mapper_5_gfx_mode == 2 || Mapper_5_gfx_mode == 3) {
                    uint8* nametable2 = PPUBANK[NAME_TABLE2];
                    ret = nametable2[wAddr & 0x3FF];
                }                
            }
        } break;
    }

    return ret;
}

/*-------------------------------------------------------------------*/
/*  Mapper 5 H-Sync Function                                         */
/*-------------------------------------------------------------------*/
void Mapper_5_HSync() {
	if (ppuinfo.PPU_Scanline <= 240) {
		Mapper_5_irq_status |= 0x40;
		// MS - really this should fire on the same scanline as set, but janky frnes timing requires it to fire 1 scanline early
		// HACK!
		if (ppuinfo.PPU_Scanline == Mapper_5_irq_line - 1) {
			if ((PPU_R1 & (R1_SHOW_SCR | R1_SHOW_SP )) == (R1_SHOW_SCR | R1_SHOW_SP)) {
				Mapper_5_irq_status |= 0x80;
			}
		}
		if ((Mapper_5_irq_status & 0x80) && (Mapper_5_irq_enabled & 0x80)) {
			IRQ_REQ;
		}
	} else {
		Mapper_5_irq_status &= ~0x40;
	}
}