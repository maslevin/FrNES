#ifndef _NES_ROM_H
#define _NES_ROM_H

#include <kos.h>
#include "defines.h"

#define ROM_TRAINER_SIZE 512
#define PROGRAM_ROM_PAGESIZE 16384
#define CHARACTER_ROM_PAGESIZE 8192
#define PROGRAM_RAM_PAGESIZE 8192
#define PLAYCHOICE_INSTROM_SIZE 8192
#define PLAYCHOICE_PROM_SIZE 32

#define FLAG_MIRRORING 0x01
#define FLAG_SRAM 0x02
#define FLAG_TRAINER 0x04
#define FLAG_FOUR_SCREEN_VRAM 0x08
#define FLAG_PC10 0x02
#define FLAG_VS 0x01
#define FLAG_VIDEOMODE 0x01

typedef enum {
	ARCHAIC,
	iNES,
	NES2_0
} NES_HEADER_VERSION;

struct NES_HEADER {
	char NESTOKEN[4];
	unsigned char numProgramRomPages;
	unsigned char numCharacterRomPages;
	unsigned char flag6;
	unsigned char flag7;
	unsigned char flag8;
	unsigned char flag9;
	unsigned char flag10;
	unsigned char flag11;
	unsigned char flag12;
	unsigned char flag13;
	unsigned char flag14;
	unsigned char flag15;
} NES_HEADER;

typedef enum {
	HORIZONTAL_MIRRORING,
	VERTICAL_MIRRORING
} MIRRORING_MODE;

typedef enum {
	NTSC,
	PAL,
	DENDY_PAL
} VIDEO_MODE;

struct NES_ROM {
	struct NES_HEADER* header;

	//storage for rom
	unsigned char* trainer;
	unsigned char** programRomPages;
	unsigned char** characterRomPages;
	unsigned char* playchoiceInstRom;
	unsigned char* playchoicePRom;

	//derived fields
	unsigned char* title;
	unsigned int mapperNumber;
	unsigned int submapperNumber;
	unsigned int numProgramRomPages;
	unsigned int numCharacterRomPages;
	unsigned int numProgramRamPages;
	unsigned int bytesBatteryProgramRam;
	unsigned int bytesRegularProgramRam;
	unsigned int bytesBatteryCharacterRam;
	unsigned int bytesRegularCharacterRam;

	NES_HEADER_VERSION headerVersion;
	MIRRORING_MODE mirroringMode;
	VIDEO_MODE videoMode;
	BOOL fourScreenVRAMEnabled;
	BOOL SRAMPresent;
	BOOL trainerPresent;
	BOOL playchoice10;
	BOOL vsUnisystem;

	unsigned int vsPPU;
	unsigned int vsMode;

} NES_ROM;

struct NES_ROM* loadRom(char* path);
void printRomInfo(struct NES_ROM* rom);
void destroyRom(struct NES_ROM* rom);

#endif