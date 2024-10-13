#include "cartridge.h"

uint8 ROM[MAXIMUM_ROM_SIZE];
uint8 VROM[MAXIMUM_VROM_SIZE];
uint8 VRAM[MAXIMUM_VRAM_SIZE];
uint8 WRAM[MAXIMUM_WRAM_SIZE];

uint32 num_ROM_bytes_allocated;
uint32 num_VROM_bytes_allocated;
uint32 num_VRAM_bytes_allocated;
uint32 num_WRAM_bytes_allocated;

uint32 num_8k_ROM_pages;
uint32 num_1k_VROM_pages;
uint32 num_1k_VRAM_pages;
uint32 num_8k_NVRAM_pages;
uint32 num_8k_WRAM_pages;

uint8* ROM_pages[MAXIMUM_ROM_PAGES];
uint8* VROM_pages[MAXIMUM_VROM_PAGES];
uint8* VRAM_pages[MAXIMUM_VRAM_PAGES];
uint8* WRAM_pages[MAXIMUM_WRAM_PAGES];

uint32 SRAM_Enabled;
uint32 currentCRC32;

uint8 ROM_Mirroring;
uint8 ROM_SRAM;
uint8 ROM_Trainer;
uint8 ROM_FourScr;

NesHeader_t NesHeader;

bool ReadRom (const char *filepath, uint32 filesize) {
    bool loaded = false;
	unsigned char* ROM_Buffer;
	int i;
	int ROM_offset;
	int VROM_offset;
	bool nes2header = false;
	uint32 prgRomSize = 0;
	uint32 chrRomSize = 0;
	uint32 prgRamSize = 0;
	uint32 prgNVRamSize = 0;
	uint32 chrRamSize = 0;
	uint32 chrNVRamSize = 0;

	ROM_Buffer = malloc(filesize);

	//MS - checksum calculation loads the rom currently, we should decouple that
	currentCRC32 = ReturnChecksum(filepath, filesize, ROM_Buffer);

	//Attempt to read an iNES header
	memcpy(&NesHeader, ROM_Buffer, sizeof(NesHeader));

    num_ROM_bytes_allocated = 0;
    num_VROM_bytes_allocated = 0;
    num_VRAM_bytes_allocated = 0;
    num_WRAM_bytes_allocated = 0;

	//Check if bytes 0-3 are NES header bytes
	if ((NesHeader.byID[0] == 0x4E) && (NesHeader.byID[1] == 0x45) && (NesHeader.byID[2] == 0x53) && (NesHeader.byID[3] == 0x1A)) {
		if ((NesHeader.byInfo2 & 0x0C) == 0x08) {
			printf("ReadRom: NES 2.0 Header Detected\n");
			nes2header = true;			
		} else {
			printf("ReadRom: iNES Header Detected\n");			
		}

		MapperNo = ((NesHeader.byInfo1 & 0xF0) >> 4) | (NesHeader.byInfo2 & 0xF0);
		if (nes2header) {
			MapperNo |= (NesHeader.byReserve[0] & 0x0F) << 16;
			SubmapperNo = (NesHeader.byReserve[0] >> 4);

			prgRomSize = (((uint16)NesHeader.byReserve[1] & 0x0F) << 8 | NesHeader.byRomSize) * 0x4000;
			chrRomSize = (((uint16)NesHeader.byReserve[1] & 0xF0) << 4 | NesHeader.byVRomSize) * 0x2000; 
			uint8 shifts = (NesHeader.byReserve[2] & 0x0F);
			if (shifts != 0) {
				prgRamSize = 64 << shifts;
			}
			shifts = ((NesHeader.byReserve[2] & 0xF0) >> 4);
			if (shifts != 0) {
				prgNVRamSize = 64 << shifts;
			}
			shifts = (NesHeader.byReserve[3] & 0x0F);
			if (shifts != 0) {
				chrRamSize = 64 << shifts;
			}
			shifts = ((NesHeader.byReserve[3] & 0xF0) >> 4);
			if (shifts != 0) {
				chrNVRamSize = 64 << shifts;
			}

			printf("ReadRom: CPU/PPU timing mode [");
			switch (NesHeader.byReserve[4] & 0x3) {
				case 0:
					printf("NTSC NES]\n");
					break;
				case 1:
					printf("PAL NES]\n");
					break;
				case 2:
					printf("Multi-region]\n");				
					break;
				case 3:
					printf("Dendy]\n");				
					break;
			}

			switch (NesHeader.byInfo2 & 0x3) {
				case 0:
					printf("ReadRom: ROM is NES / Famicom type\n");
					break;
				case 1:
					printf("ReadRom: ROM is Vs. System type\n");
					printf("ReadRom: Vs. PPU Type [%u]\n", NesHeader.byReserve[5] & 0x0F);
					printf("ReadRom: Vs. Hardware Type [%u]\n", (NesHeader.byReserve[5] & 0xF0) >> 4);
					break;
				case 2:
					printf("ReadRom: ROM is Playchoice 10 type\n");
					break;
				case 3: 
					printf("ReadRom: ROM is Extended? type\n");
					printf("ReadRom: Extended Console Type [%u]\n", (NesHeader.byReserve[5] & 0x0F));
					break;
			}

			if (NesHeader.byReserve[6] & 0x03) {
				printf("ReadRom: [%u] Miscellaneous ROMs Present\n", NesHeader.byReserve[6] & 0x03);
			}

			if (NesHeader.byReserve[7] & 0x3F) {
				printf("ReadRom: Default Expansion Device [%u]\n", NesHeader.byReserve[7] & 0x3F);
			}
		} else {

			prgRomSize = NesHeader.byRomSize * 0x4000;
			chrRomSize = NesHeader.byVRomSize * 0x2000;
			if (NesHeader.byInfo1 & 2) {
				if (NesHeader.byReserve[0] != 0) {
					prgNVRamSize = NesHeader.byReserve[0] * 0x2000;
				} else {
					printf("ReadRom: iNES Header Implied 8kB PRG NVRAM\n");
					prgNVRamSize = 0x2000;
				}
			} else {
				if (NesHeader.byReserve[0] != 0) {
					prgRamSize = NesHeader.byReserve[0] * 0x2000;
				} else {
					printf("ReadRom: iNES Header Implied 8kB PRG RAM\n");
					prgRamSize = 0x2000;
				}
			}

			// Handle VRAM - sometimes we will just use the VROM variable for itgit 
			// but other times we'll need somewhere else to store aux. bank switched VRAM
			if (NesHeader.byVRomSize == 0) {
				switch (MapperNo) {
					case 30: {
						printf("ReadRom: Mapper 30 Defaulting to 4 * 8kB CHR RAM\n");
						chrRamSize = 0x8000;
					} break;

					case 111: {
						printf("ReadRom: Mapper 111 Defaulting to 2 * 8kB CHR RAM\n");
						chrRamSize = 0x4000;
					} break;

					default: {
						printf("ReadRom: Implied 8kB CHR RAM\n");
						chrRamSize = 0x2000;
					} break;
				}
			} else {
				switch (MapperNo) {
					case 119: {
						printf("ReadRom: Mapper 119 Allocating Auxilliary 8kB CHR RAM\n");
						chrRamSize = 0x2000;
					} break;
				}
			}			
		}

		printf("ReadRom: Mapper Number [%u]\n", MapperNo);
		if (nes2header) {
			printf("ReadRom: NES 2.0 Submapper [%u]\n", SubmapperNo);
		}
		printf("ReadRom: PRG ROM [%lu kB] - [%lu] * 16kB banks\n", prgRomSize / 1024, prgRomSize / 16384);
		if (prgRamSize != 0) {
			printf("ReadRom: PRG RAM [%lu kB] - [%lu] * 8kB banks\n", prgRamSize / 1024, prgRamSize / 8192);
		}
		if (prgNVRamSize != 0) {
			printf("ReadRom: PRG NVRAM [%lu kB] - [%lu] * 8kB banks\n", prgNVRamSize / 1024, prgNVRamSize / 8192);
		}
		if (chrRomSize != 0) {
			printf("ReadRom: CHR ROM [%lu kB] - [%lu] * 8kB banks\n", chrRomSize / 1024, chrRomSize / 8192);		
		}
		if (chrRamSize != 0) {
			printf("ReadRom: CHR RAM [%lu kB] - [%lu] * 8kB banks\n", chrRamSize / 1024, chrRamSize / 8192);
		}
		if (chrNVRamSize != 0) {
			printf("ReadRom: CHR NVRAM [%lu kB] - [%lu] * 8kB banks\n", chrNVRamSize / 1024, chrNVRamSize / 8192);
		}

		printf("ReadRom: Nametable Arrangement [%s]\n", (NesHeader.byInfo1 & 1) ? "horizontal mirrored" : "vertically mirrored");			
		printf("ReadRom: Battery Backed RAM [%s]\n", (NesHeader.byInfo1 & 2) ? "present" : "not present");

		if (NesHeader.byInfo1 & 2) {
			SRAM_Enabled = 1;
		} else {
			SRAM_Enabled = 0;
		}

		printf("ReadRom: Trainer Present [%s]\n", (NesHeader.byInfo1 & 4) ? "true" : "false");
		printf("ReadRom: Alternative Nametable Layout [%s]\n", (NesHeader.byInfo1 & 8) ? "present" : "not present");		

		i = 16;
		//Read past the trainer
		if (NesHeader.byInfo1 & 4) {
			i += 512;
		}

		ROM_offset = i;
        num_8k_ROM_pages = prgRomSize / 8192;
        num_1k_VROM_pages = chrRomSize / 1024;
		num_1k_VRAM_pages = chrRamSize / 1024;
		
        num_ROM_bytes_allocated = prgRomSize;
		for (; i < (ROM_offset + prgRomSize); i++) {
			ROM[i - ROM_offset] = ROM_Buffer[i];
		}

		if (chrRomSize > 0) {
			VROM_offset = i;
            num_VROM_bytes_allocated = chrRomSize;
			for (; i < (VROM_offset + chrRomSize); i++) {
				VROM[i - VROM_offset] = ROM_Buffer[i];
			}
		}

		num_VRAM_bytes_allocated = chrRamSize;
		num_WRAM_bytes_allocated = prgNVRamSize + prgRamSize;
		num_8k_WRAM_pages = (prgNVRamSize + prgRamSize) / 8192;
		num_8k_NVRAM_pages = prgNVRamSize / 8192;

        for (i = 0; i < num_8k_ROM_pages; i++) {
            ROM_pages[i] = &ROM[i * 0x2000];
        }
		if (num_1k_VROM_pages) {
			for (i = 0; i < num_1k_VROM_pages; i++) {
				VROM_pages[i] = &VROM[i * 0x400];
			}
			if (num_1k_VRAM_pages) {
				memset4(VRAM, 0, MAXIMUM_VRAM_SIZE);
				for (i = 0; i < num_1k_VROM_pages; i++) {
					VRAM_pages[i] = &VRAM[i * 0x400];
				}				
			}
		} else if (num_1k_VRAM_pages) {
			memset4(VROM, 0, MAXIMUM_VROM_SIZE);			
			for (i = 0; i < num_1k_VRAM_pages; i++) {
				VROM_pages[i] = &VROM[i * 0x400];
			}			
		}
		for (i = 0; i < num_8k_WRAM_pages; i++) {
			WRAM_pages[i] = &WRAM[i * 0x2000];
		}

		loaded = true;
	} else {
		printf("ReadRom: NOT AN NES FILE - exiting");
	}

	free(ROM_Buffer);

	return loaded;
}