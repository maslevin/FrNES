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

uint8* ROM_pages[MAXIMUM_ROM_PAGES];
uint8* VROM_pages[MAXIMUM_VROM_PAGES];

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
	int returnValue = -1;
	if ((NesHeader.byID[0] == 0x4E) && (NesHeader.byID[1] == 0x45) && (NesHeader.byID[2] == 0x53) && (NesHeader.byID[3] == 0x1A)) {
		if ((NesHeader.byInfo2 & 0x0C) == 0x08) {
			printf("ReadRom: NES 2.0 Header Detected\n");
		} else {
			printf("ReadRom: iNES Header Detected\n");			
		}

		MapperNo = ((NesHeader.byInfo1 & 0xF0) >> 4) | (NesHeader.byInfo2 & 0xF0);
		printf("ReadRom: Mapper Number [%i]\n", MapperNo);
		printf("ReadRom: PRG ROM [%i] * 16kB banks\n", NesHeader.byRomSize);

		// Handle VRAM - sometimes we will just use the VROM variable for it
		// but other times we'll need somewhere else to store aux. bank switched VRAM
		if (NesHeader.byVRomSize == 0) {
			printf("ReadRom: Implied 8kB CHR RAM\n");
			// Implied 8kB Chr Ram will be mapped into VROM since it usually won't be bankswapped by a mapper
			num_VROM_bytes_allocated = 8192;
		} else {
			printf("ReadRom: CHR ROM [%i] * 8kB banks\n", NesHeader.byVRomSize);
			switch (MapperNo) {
				case 30: {
					printf("ReadRom: Mapper 30 Defaulting to 4 * 8kB CHR RAM\n");
					num_VRAM_bytes_allocated = 32768;
				} break;

				case 119: {
					printf("ReadRom: Mapper 119 Allocating Auxilliary 8kB CHR RAM\n");
					num_VRAM_bytes_allocated = 8192;
				} break;
			}
		}

		printf("ReadRom: Nametable Arrangement [%s]\n", (NesHeader.byInfo1 & 1) ? "horizontal mirrored" : "vertically mirrored");

		printf("ReadRom: Battery Backed RAM [%s]\n", (NesHeader.byInfo1 & 2) ? "present" : "not present");

		printf("ReadRom: Trainer Present [%s]\n", (NesHeader.byInfo1 & 4) ? "true" : "false");

		printf("ReadRom: Alternative Nametable Layout [%s]\n", (NesHeader.byInfo1 & 8) ? "present" : "not present");
		if (NesHeader.byInfo2 & 0x01) {
			printf("ReadRom: VS Unisystem ROM detected");
		}
		if (NesHeader.byInfo2 & 0x02) {
			printf("ReadRom: PlayChoice-10 ROM detected");
		}
		if (NesHeader.byReserve[0] != 0) {
			printf("ReadRom: PRG RAM [%i] * 8kB banks\n", NesHeader.byReserve[0]);
		}

		if (NesHeader.byInfo1 & 2) {
			SRAM_Enabled = 1;
		} else {
			SRAM_Enabled = 0;
		}

		i = 16;
		//Read past the trainer
		if (NesHeader.byInfo1 & 4) {
			i += 512;
		}

		ROM_offset = i;
        num_8k_ROM_pages = NesHeader.byRomSize * 2;
        num_1k_VROM_pages = NesHeader.byVRomSize * 8;
        num_ROM_bytes_allocated = NesHeader.byRomSize * 0x4000;
		for (; i < (ROM_offset + (NesHeader.byRomSize * 0x4000)); i++)
			ROM[i - ROM_offset] = ROM_Buffer[i];

		if (NesHeader.byVRomSize > 0) {
			VROM_offset = i;
            num_VROM_bytes_allocated = NesHeader.byVRomSize * 0x2000;
			for (; i < (VROM_offset + (NesHeader.byVRomSize * 0x2000)); i++)
				VROM[i - VROM_offset] = ROM_Buffer[i];
		}

        for (i = 0; i < num_8k_ROM_pages; i++) {
            ROM_pages[i] = &ROM[i * 0x2000];
        }
        for (i = 0; i < num_1k_VROM_pages; i++) {
            VROM_pages[i] = &VROM[i * 0x400];
        }        

		loaded = true;
	} else {
		printf("ReadRom: NOT AN NES FILE - exiting");
	}

	free(ROM_Buffer);

	return loaded;
}