#include "nesrom.h"

unsigned int sizeByteToExponentialSize(unsigned char sizeByte) {
	if (sizeByte == 0) {
		return 0;
	} else {
		return 1 << (6 + sizeByte);
	}
}

struct NES_ROM* loadRom(char* path) {
	struct NES_ROM* rom = NULL;	

	uint32 fd = fs_open(path, O_RDONLY);
	if (fd > 0) {
		printf("Seeking to end of ROM\n");
		fs_seek(fd, 0L, SEEK_END);
		int fileSize = fs_tell(fd);
		printf("Offset was %i\n", fileSize);
		fs_seek(fd, 0L, SEEK_SET);
		printf("Seeking to beginning of ROM\n");

		struct NES_HEADER* header = malloc (sizeof(NES_HEADER));
		fs_read(fd, header, sizeof(NES_HEADER));

		if (strncmp(header -> NESTOKEN, "NES\x1A", 4) == 0) {
			printf("Detected NES Header Token\n");
	 		rom = malloc(sizeof(NES_ROM));
	 		memset(rom, 0, sizeof(NES_ROM));
	 		rom -> header = header;

			//Detect header version
			BOOL categorizedHeader = FALSE;
			if ((header -> flag7 & 0x0C) == 0x08) {
				unsigned int numCharacterRomPages = (((unsigned int)header -> flag9 & 0xF0) << 4) | ((unsigned int)header -> numCharacterRomPages);
				unsigned int numProgramRomPages = (((unsigned int)header -> flag9 & 0x0F) << 8) | ((unsigned int)header -> numProgramRomPages);
				unsigned int calculatedRomSize = (numCharacterRomPages * CHARACTER_ROM_PAGESIZE) + (numProgramRomPages * PROGRAM_ROM_PAGESIZE);
				if ((header -> flag6 & FLAG_TRAINER) != 0)
					calculatedRomSize += ROM_TRAINER_SIZE;
				calculatedRomSize += sizeof(NES_HEADER);

				if (calculatedRomSize <= fileSize) {
					categorizedHeader = TRUE;
					printf("Detected a NES 2.0 header\n");
					rom -> headerVersion = NES2_0;
					rom -> numProgramRomPages = numProgramRomPages;
					rom -> numCharacterRomPages = numCharacterRomPages;
					rom -> mapperNumber = (((unsigned int)header -> flag6 & 0xF0) >> 4) | ((unsigned int)header -> flag7 & 0xF0) | (((unsigned int)header -> flag8 & 0x0F) << 8);
					rom -> submapperNumber = (((unsigned int)header -> flag8 & 0xF0) >> 4);
					rom -> bytesBatteryProgramRam = sizeByteToExponentialSize(((unsigned int)header -> flag10 & 0xF0) >> 4);
					rom -> bytesRegularProgramRam = sizeByteToExponentialSize(((unsigned int)header -> flag10 & 0x0F));
					rom -> bytesBatteryCharacterRam = sizeByteToExponentialSize(((unsigned int)header -> flag11 & 0xF0) >> 4);
					rom -> bytesRegularCharacterRam = sizeByteToExponentialSize(((unsigned int)header -> flag11 & 0x0F));															
					if ((header -> flag12 & FLAG_VIDEOMODE) != 0) {
						rom -> videoMode = PAL;
					} else {
						rom -> videoMode = NTSC;
					}
					rom -> vsPPU = header -> flag13 & 0x0F;
					rom -> vsMode = (header -> flag13 & 0x0F) >> 4;
				}
			}
			if (!categorizedHeader) {
				if (((header -> flag7 & 0x0C) == 0) && (header -> flag12 == 0) && (header -> flag13 == 0) && (header -> flag14 == 0) && (header -> flag15 == 0)) {
					printf("Detected an iNES header\n");	
					rom -> headerVersion = iNES;
					rom -> numProgramRomPages = header -> numProgramRomPages;
					rom -> numCharacterRomPages = header -> numCharacterRomPages;					
					rom -> mapperNumber = (((unsigned int)header -> flag6 & 0xF0) >> 4) | ((unsigned int)(header -> flag7 & 0xF0));					
						rom -> numProgramRamPages = header -> flag8;
						if (rom -> numProgramRamPages == 0) {
							rom -> numProgramRamPages = 1;
						}
				} else {
					printf("Detected an Archaic iNES header\n");						
					rom -> headerVersion = ARCHAIC;
					rom -> numProgramRomPages = header -> numProgramRomPages;
					rom -> numCharacterRomPages = header -> numCharacterRomPages;										
					rom -> mapperNumber = (((unsigned int)header -> flag6 & 0xF0) >> 4);					
				}
			}

			if ((header -> flag6 & FLAG_MIRRORING) == 0) {
				rom -> mirroringMode = HORIZONTAL_MIRRORING;
			} else {
				rom -> mirroringMode = VERTICAL_MIRRORING;
			}

			rom -> fourScreenVRAMEnabled = ((header -> flag6 & FLAG_FOUR_SCREEN_VRAM) != 0);
			rom -> SRAMPresent = ((header -> flag6 & FLAG_SRAM) != 0);
			rom -> trainerPresent = ((header -> flag6 & FLAG_TRAINER) != 0);
			rom -> playchoice10 = ((header -> flag7 & FLAG_PC10) != 0);
			rom -> vsUnisystem = ((header -> flag7 & FLAG_VS) != 0);

			if (rom -> trainerPresent) {
	 			rom -> trainer = malloc(ROM_TRAINER_SIZE);				
	 			if (fs_read(fd, &(rom -> trainer), ROM_TRAINER_SIZE) != ROM_TRAINER_SIZE) {
	 				printf("Unable to load trainer... exiting...\n");
	 				destroyRom(rom);
	 				return NULL;
	 			} else {
	 				printf("Loading trainer...\n");
	 			}
			}

			rom -> programRomPages = (unsigned char**)malloc(rom -> numProgramRomPages * sizeof(unsigned char**));
			printf("Allocating %i program rom pages\n", rom -> numProgramRomPages);
			int programRomIndex = 0;
			for (; programRomIndex < rom -> numProgramRomPages; programRomIndex++) {
				rom->programRomPages[programRomIndex] = (unsigned char*)malloc(PROGRAM_ROM_PAGESIZE);
				if (fs_read(fd, rom -> programRomPages[programRomIndex], PROGRAM_ROM_PAGESIZE) != PROGRAM_ROM_PAGESIZE) {
	 				printf("Unable to load program rom page %i... exiting...\n", programRomIndex);					
					destroyRom(rom);
					return NULL;
				} else {
					printf("Loading program page %i\n", programRomIndex);
				}
			}

			if (rom -> numCharacterRomPages > 0) {
				rom -> characterRomPages = (unsigned char**)malloc(rom -> numCharacterRomPages * sizeof(unsigned char**));
				printf("Allocating %i character rom pages\n", rom -> numCharacterRomPages);				
				int characterRomIndex = 0;
				for (; characterRomIndex < rom -> numCharacterRomPages; characterRomIndex++) {
					rom -> characterRomPages[characterRomIndex] = (unsigned char*)malloc(CHARACTER_ROM_PAGESIZE);
					if (fs_read(fd, rom -> characterRomPages[characterRomIndex], CHARACTER_ROM_PAGESIZE) != CHARACTER_ROM_PAGESIZE) {
	 					printf("Unable to load character rom page %i... exiting...\n", characterRomIndex);											
	 					destroyRom(rom);
	 					return NULL;
					} else {
						printf("Loading character page %i\n", characterRomIndex);
					}
				}
			}

			if (rom -> playchoice10 == TRUE) {
				rom -> playchoiceInstRom = (unsigned char*)malloc(PLAYCHOICE_INSTROM_SIZE);
				if (fs_read(fd, &(rom -> playchoiceInstRom), PLAYCHOICE_INSTROM_SIZE) != PLAYCHOICE_INSTROM_SIZE) {
					printf("Unable to load playchoice 10 InstRom\n");
					free (rom -> playchoiceInstRom);
					rom -> playchoiceInstRom = NULL;
				}
				rom -> playchoicePRom = (unsigned char*)malloc(PLAYCHOICE_PROM_SIZE);
				if (fs_read(fd, &(rom -> playchoicePRom), PLAYCHOICE_PROM_SIZE) != PLAYCHOICE_PROM_SIZE) {
					printf("Unable to load playchoice 10 PRom\n");
					free (rom -> playchoicePRom);
					rom -> playchoicePRom = NULL;
				}
			}

			int fileOffset = fs_tell(fd);
			if (fs_tell(fd) < fileSize) {
				int titleSize = fileSize - fileOffset;
				printf("Detected a trailing %i bytes; treating as ROM title\n", titleSize);
				rom -> title = malloc(titleSize);
				fs_read(fd, &(rom -> title), titleSize);
			} else {
				printf("No trailing bytes; no title present\n");
			}

			fs_close(fd);
		} else {
			printf("Corrupt ROM header\n");
			free(header);
			return NULL;
		}
	} else {
		printf("Unable to open rom path (%s) for reading\n", path);
	}

	return rom;
}

void printRomInfo(struct NES_ROM* rom) {
	if (rom == NULL) {
		printf("NULL rom present; no info\n");
		return;
	}

	if (rom -> title != NULL) {
		printf("ROM Title - %s\n", rom -> title);
	} else {
		printf("ROM Title - Unknown\n");
	}
	
	switch (rom -> headerVersion) {
		case ARCHAIC:
			printf("ROM Header - Archaic iNES\n");
			break;
		case iNES:
			printf("ROM Header - iNES 0.7\n");
			break;
		case NES2_0:
			printf("ROM Header - NES 2.0\n");
			break;
		default:
			printf("Unknown header version\n");
			break;
	}
	printf("Mapper Number %i\n", rom -> mapperNumber);
}

void destroyRom(struct NES_ROM* rom) {
	if (rom == NULL) {
		return;
	}

	printf("Destroying Rom...\n");

	if (rom -> trainer != NULL) {
		free(rom -> trainer);
		rom -> trainer = NULL;
	}

	if (rom -> programRomPages != NULL) {
		int programRomIndex = 0;
		for (; programRomIndex < rom -> numProgramRomPages; programRomIndex++) {
			free(rom -> programRomPages[programRomIndex]);
		}
		free(rom -> programRomPages);
		rom -> programRomPages = NULL;
	}

	if (rom -> characterRomPages != NULL) {
		int characterRomIndex = 0;
		for (; characterRomIndex < rom -> numCharacterRomPages; characterRomIndex++) {
			free(rom -> characterRomPages[characterRomIndex]);
		}
		free(rom -> characterRomPages);
		rom -> characterRomPages = NULL;
	}

	if (rom -> playchoiceInstRom != NULL) {
		free (rom -> playchoiceInstRom);
		rom -> playchoiceInstRom = NULL;
	}

	if (rom -> playchoicePRom != NULL) {
		free (rom -> playchoicePRom);
		rom -> playchoicePRom = NULL;
	}

	if (rom -> title != NULL) {
		free (rom -> title);
		rom -> title = NULL;
	}

	free (rom);
}