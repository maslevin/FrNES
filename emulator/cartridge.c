#include "cartridge.h"
#include <stdlib.h>

#include "memory.h"
#include "input_recording.h"
#include "mapper.h"
#include "fds.h"

uint8_t* ROM;
uint8_t* VROM;
uint8_t* cartridge_VRAM;

uint32 num_ROM_bytes_allocated;
uint32 num_VROM_bytes_allocated;
uint32 num_VRAM_bytes_allocated;
uint32 num_WRAM_bytes_allocated;

uint32 num_8k_ROM_pages;
uint32 num_1k_VROM_pages;
uint32 num_1k_VRAM_pages;
uint32 num_8k_NVRAM_pages;
uint32 num_8k_WRAM_pages;

uint8* ROM_pages[MAXIMUM_ROM_PAGES] FRNES_COLD_BSS;
uint8* VROM_pages[MAXIMUM_VROM_PAGES] FRNES_COLD_BSS;
uint8* VRAM_pages[MAXIMUM_VRAM_PAGES];

uint32 SRAM_Enabled;
uint32 currentCRC32;

uint8 ROM_Mirroring;
uint8 ROM_SRAM;
uint8 ROM_Trainer;
uint8 ROM_FourScr;

NesHeader_t NesHeader;

uint32_t cartridge_mapper_number = 0;
uint32_t cartridge_submapper = 0;

int load_nes_file (const char *filepath) {
	uint8_t* ROM_Buffer;
	uint32_t i;
	uint32_t ROM_offset;
	uint32_t VROM_offset;
    uint32_t MapperNo;
    uint32_t SubmapperNo;
	bool nes2header = false;
	uint32_t prgRomSize = 0;
	uint32_t chrRomSize = 0;
	uint32_t prgRamSize = 0;
	uint32_t prgNVRamSize = 0;
	uint32_t chrRamSize = 0;
	uint32_t chrNVRamSize = 0;
    cartridge_submapper = 0;
    cartridge_single_screen = cartridge_db_no_battery = 0;
    ROM = NULL;
    VROM = NULL;
    cartridge_VRAM = NULL;

    printf("load_nes_file - getting file size\n");

    struct stat file_stat;
    if (fs_stat(filepath, &file_stat, 0) == -1) {
        printf(" ...ERROR: unable to retrieve file status\n");
        return -1;
    } else {
        printf(" ...detected file size [%li]\n", file_stat.st_size);
    }        

    printf(" ...opening file\n");
    file_t fd = fs_open(filepath, O_RDONLY);
    if (fd == -1) {
        printf(" ...could not open requested file [%s]\n", filepath);
        return -1;
    }

    printf(" ...allocating ROM buffer\n");
	ROM_Buffer = malloc(file_stat.st_size);

    printf(" ...reading ROM from filesystem\n");
    if (fs_read(fd, ROM_Buffer, file_stat.st_size) != file_stat.st_size) {
        printf(" ...ERROR: unable to read ROM from filesystem\n");
        free(ROM_Buffer);
        return -1;
    }
    fs_close(fd);

	// (a Famicom Disk System image becomes a mapper 20 cartridge, see fds.c, and an NSF tune one of the NSF board,
	// nsf.c: cartridge_convert, mapper_boards.c, out of the link time optimized code this is inlined into)
	{
		uint32_t size = file_stat.st_size;
		ROM_Buffer = cartridge_convert(ROM_Buffer, &size);
		if (ROM_Buffer == NULL) {
			return -1;
		}
		file_stat.st_size = size;
	}

	//Attempt to read an iNES header
	memcpy(&NesHeader, ROM_Buffer, sizeof(NesHeader));

    num_ROM_bytes_allocated = 0;
    num_VROM_bytes_allocated = 0;
    num_VRAM_bytes_allocated = 0;
    num_WRAM_bytes_allocated = 0;

	//Check if bytes 0-3 are NES header bytes
	if ((NesHeader.byID[0] == 0x4E) && (NesHeader.byID[1] == 0x45) && (NesHeader.byID[2] == 0x53) && (NesHeader.byID[3] == 0x1A)) {
		// the header's generation, by the nesdev wiki's iNES page ("Recommended detection procedure", byte 7 AND $0C):
		// $08 NES 2.0; $04 archaic iNES (bytes 7-15 hold a tool's signature, "DiskDude!" and the like: only byte 6's
		// mapper nibble counts, so they're cleared before anything reads them); $00 iNES, whose bytes 8-15 an iNES 1.0
		// ROM's load doesn't read, and junk in bytes 12-15 makes it iNES 0.7, byte 7's mapper nibble kept (the wiki's
		// rule of thumb would mask it: Major League's "by PIRO" header has its mapper, 32, there); $0C, which the wiki
		// leaves as "iNES 0.7 or archaic", archaic
		if (((NesHeader.byInfo2 & 0x0C) == 0x04) || ((NesHeader.byInfo2 & 0x0C) == 0x0C)) {
			printf(" - Archaic iNES header (bytes 7-15 ignored)\n");
			NesHeader.byInfo2 = 0;
			memset(NesHeader.byReserve, 0, sizeof(NesHeader.byReserve));
		}
		if ((NesHeader.byInfo2 & 0x0C) == 0x08) {
			printf(" - NES 2.0 Header Detected\n");
			nes2header = true;			
		} else {
			printf(" - iNES Header Detected\n");			
		}

		MapperNo = ((NesHeader.byInfo1 & 0xF0) >> 4) | (NesHeader.byInfo2 & 0xF0);
		if (nes2header) {
			// (byte 8's low nibble: mapper bits 8-11; a header's mapper is 12 bits)
			MapperNo |= (NesHeader.byReserve[0] & 0x0F) << 8;
			SubmapperNo = (NesHeader.byReserve[0] >> 4);
			cartridge_submapper = SubmapperNo;

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

			printf(" - CPU/PPU timing mode [");
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
					printf(" - ROM is NES / Famicom type\n");
					break;
				case 1:
					printf(" - ROM is Vs. System type\n");
					printf(" - Vs. PPU Type [%u]\n", NesHeader.byReserve[5] & 0x0F);
					printf(" - Vs. Hardware Type [%u]\n", (NesHeader.byReserve[5] & 0xF0) >> 4);
					break;
				case 2:
					printf(" - ROM is Playchoice 10 type\n");
					break;
				case 3: 
					printf(" - ROM is Extended? type\n");
					printf(" - Extended Console Type [%u]\n", (NesHeader.byReserve[5] & 0x0F));
					break;
			}

			if (NesHeader.byReserve[6] & 0x03) {
				printf(" - [%u] Miscellaneous ROMs Present\n", NesHeader.byReserve[6] & 0x03);
			}

			if (NesHeader.byReserve[7] & 0x3F) {
				printf(" - Default Expansion Device [%u]\n", NesHeader.byReserve[7] & 0x3F);
			}
		} else {

			prgRomSize = NesHeader.byRomSize * 0x4000;
			chrRomSize = NesHeader.byVRomSize * 0x2000;
			// the game database's mapper and submapper when it knows the ROM (Mesen 2 uses them instead of an iNES 1.0
			// header's), by Mesen 2's "PRG+CHR CRC32": everything after the header and trainer to the end of the file
			// (an overdump's extra bytes included, so it isn't found and keeps its header, as in Mesen)
			{
				uint32_t start = 16 + ((NesHeader.byInfo1 & 4) ? 512 : 0);
				uint32_t length = (start < (uint32_t)file_stat.st_size) ? (uint32_t)file_stat.st_size - start : 0;
				int32_t board = board_db_mapper(input_recording_crc32(&ROM_Buffer[start], length));
				if (board >= 0) {
					if ((uint32_t)(board & 0xfff) != MapperNo) {
						printf(" - Mapper %lu from the game database (header says %lu)\n", (unsigned long)(board & 0xfff), (unsigned long)MapperNo);
					}
					MapperNo = board & 0xfff;
					cartridge_submapper = (uint32_t)board >> 12;
				}
			}
			// (dumps whose headers give less CHR ROM than the file holds, see mapper_boards.c)
			chrRomSize = board_chr_rom_size(&ROM_Buffer[16], file_stat.st_size - 16, chrRomSize);
			// PRG RAM as Mesen 2 gives it: the board's, or its game database's (see mapper_boards.c)
			if (NesHeader.byInfo1 & 2) {
				prgNVRamSize = board_prg_ram_size(&ROM_Buffer[16], file_stat.st_size - 16, MapperNo);
			} else {
				prgRamSize = board_prg_ram_size(&ROM_Buffer[16], file_stat.st_size - 16, MapperNo);
			}

			// Handle VRAM - sometimes we will just use the VROM variable for it 
			// but other times we'll need somewhere else to store aux. bank switched VRAM
			if (NesHeader.byVRomSize == 0) {
				switch (MapperNo) {
					case 30: {
						printf(" - Mapper 30 Defaulting to 4 * 8kB CHR RAM\n");
						chrRamSize = 0x8000;
					} break;

					case 111: {
						printf(" - Mapper 111 Defaulting to 2 * 8kB CHR RAM\n");
						chrRamSize = 0x4000;
					} break;

					case 13: {
						printf(" - Mapper 13 Defaulting to 16kB CHR RAM\n");
						chrRamSize = 0x4000;
					} break;

					default: {
						printf(" - Implied 8kB CHR RAM\n");
						chrRamSize = 0x2000;
					} break;
				}
			} else {
				switch (MapperNo) {
					case 119: {
						printf(" - Mapper 119 Allocating Auxilliary 8kB CHR RAM\n");
						chrRamSize = 0x2000;
					} break;
					case 77: {
						printf(" - Mapper 77 Allocating Auxilliary 6kB CHR RAM\n");
						chrRamSize = 0x1800;
					} break;
				}
			}			
		}

		printf(" - Mapper Number [%lu]\n", MapperNo);
		cartridge_mapper_number = MapperNo;
		if (nes2header) {
			printf(" - NES 2.0 Submapper [%lu]\n", SubmapperNo);
		}
		printf(" - PRG ROM [%lu kB] - [%lu] * 16kB banks\n", prgRomSize / 1024, prgRomSize / 16384);
		if (prgRamSize != 0) {
			printf(" - PRG RAM [%lu kB] - [%lu] * 8kB banks\n", prgRamSize / 1024, prgRamSize / 8192);
		}
		if (prgNVRamSize != 0) {
			printf(" - PRG NVRAM [%lu kB] - [%lu] * 8kB banks\n", prgNVRamSize / 1024, prgNVRamSize / 8192);
		}
		if (chrRomSize != 0) {
			printf(" - CHR ROM [%lu kB] - [%lu] * 8kB banks\n", chrRomSize / 1024, chrRomSize / 8192);		
		}
		if (chrRamSize != 0) {
			printf(" - CHR RAM [%lu kB] - [%lu] * 8kB banks\n", chrRamSize / 1024, chrRamSize / 8192);
		}
		if (chrNVRamSize != 0) {
			printf(" - CHR NVRAM [%lu kB] - [%lu] * 8kB banks\n", chrNVRamSize / 1024, chrNVRamSize / 8192);
		}

		printf(" - Nametable Arrangement [%s]\n", (NesHeader.byInfo1 & 1) ? "horizontal mirrored" : "vertically mirrored");			
		printf(" - Battery Backed RAM [%s]\n", (NesHeader.byInfo1 & 2) ? "present" : "not present");

		if (NesHeader.byInfo1 & 2) {
			SRAM_Enabled = 1;
		} else {
			SRAM_Enabled = 0;
		}

		printf(" - Trainer Present [%s]\n", (NesHeader.byInfo1 & 4) ? "true" : "false");
		printf(" - Alternative Nametable Layout [%s]\n", (NesHeader.byInfo1 & 8) ? "present" : "not present");		

		i = 16;
		//Read past the trainer
		if (NesHeader.byInfo1 & 4) {
			i += 512;
		}

		ROM_offset = i;
        num_8k_ROM_pages = prgRomSize / 8192;
        num_1k_VROM_pages = chrRomSize / 1024;
		num_1k_VRAM_pages = chrRamSize / 1024;
		
        printf(" - Loading ROM\n");
        num_ROM_bytes_allocated = prgRomSize;
        ROM = aligned_alloc(0x1000, num_ROM_bytes_allocated);
		for (; i < (ROM_offset + prgRomSize); i++) {
			ROM[i - ROM_offset] = ROM_Buffer[i];
		}

		if (chrRomSize > 0) {
			VROM_offset = i;
            printf(" - Loading VROM\n");
            num_VROM_bytes_allocated = chrRomSize;
            VROM = aligned_alloc(0x400, num_VROM_bytes_allocated);
			for (; i < (VROM_offset + chrRomSize); i++) {
				VROM[i - VROM_offset] = ROM_Buffer[i];
			}
			// Written through cached P1, then read through the pattern table windows (other virtual addresses): write
			// the lines back now, as memory.c does for the nametables and WRAM
			dcache_wback_range((uintptr_t)VROM, num_VROM_bytes_allocated);
		}

		num_VRAM_bytes_allocated = chrRamSize;
        if (num_VRAM_bytes_allocated > 0) {
            cartridge_VRAM = aligned_alloc(0x400, chrRamSize);
            memset(cartridge_VRAM, 0, chrRamSize);
            // (as VROM above: a late write-back of these zeros would clobber CHR the game wrote through a window)
            dcache_wback_range((uintptr_t)cartridge_VRAM, chrRamSize);
        }

		// NES 2.0 headers give the chips' own sizes, under 8 kB on some boards (Gradius II's VRC4 2 kB, MMC6 1 kB):
		// WRAM is whole 8 kB pages (it's mapped in 4 kB MMU pages, so the chip's mirrors through $6000-$7FFF are
		// plain RAM here). With none, $6000-$7FFF is open bus and VRC2 / VRC4 boards take it for the VRC2 latch
		// (a local: num_WRAM_bytes_allocated is never read, so LTO drops it from .bss)
		uint32_t wram_size = (prgNVRamSize + prgRamSize + 0x1fff) & ~0x1fff;
		num_WRAM_bytes_allocated = wram_size;
        if (wram_size) {
            initialize_wram(wram_size, NULL);
        }
		num_8k_WRAM_pages = wram_size / 8192;        
		num_8k_NVRAM_pages = prgNVRamSize / 8192;

        for (i = 0; i < num_8k_ROM_pages; i++) {
            ROM_pages[i] = &ROM[i * 0x2000];
        }
		if (num_1k_VROM_pages) {
			for (i = 0; i < num_1k_VROM_pages; i++) {
				VROM_pages[i] = &VROM[i * 0x400];
			}
			if (num_1k_VRAM_pages) {
				for (i = 0; i < num_1k_VRAM_pages; i++) {
					VRAM_pages[i] = &cartridge_VRAM[i * 0x400];
				}				
			}
		} else if (num_1k_VRAM_pages) {
			// CHR RAM only: the pattern tables are banked from the cartridge's CHR RAM (allocated and cleared above)
			for (i = 0; i < num_1k_VRAM_pages; i++) {
				VROM_pages[i] = &cartridge_VRAM[i * 0x400];
			}			
		} 
	} else {
		printf(" ...ERROR NOT AN NES FILE - exiting\n");
	    free(ROM_Buffer);
		ROM_Buffer = NULL;
        return -1;
	}

	// CRC32 of everything after the iNES header, as FrNES computes it (identifies the ROM for input recordings)
	currentCRC32 = input_recording_crc32(&ROM_Buffer[16], file_stat.st_size - 16);
	printf(" - ROM CRC32 [%08lX]\n", (unsigned long)currentCRC32);

	free(ROM_Buffer);
	ROM_Buffer = NULL;

	return 0;
}

int unload_nes_file() {
	printf("unload_nes_file - shutting down\n");
    if (ROM != NULL) {
		printf(" ...freeing ROM\n");
        free(ROM);
		ROM = NULL;
    }
    if (VROM != NULL) {
		printf(" ...freeing VROM\n");		
        free(VROM);
		VROM = NULL;
    }
    if (cartridge_VRAM != NULL) {
		printf(" ...freeing VRAM\n");		
        free(cartridge_VRAM);
		cartridge_VRAM = NULL;
    }
	return 0;
}