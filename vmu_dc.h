/*
FrNES VMU Support-
Adapted from the NesterDC source
*/

#ifndef _VMU_DC_H
#define _VMU_DC_H

#include <kos.h>

#include "pNesX_System_DC.h"
#include "GUI_VideoPage.h"
#include "GUI_SystemPage.h"
#include "GUI_ControlPage.h"
#include "GUI_GUIPage.h"

// The dirent_vmu and file_hdr_vmu types came from 
// vmuload01.0 (file dciwrap.c) by Dan Potter
// Original code can be found on this site:
// http://dcdev.allusion.net/softprj/
 
/* VMU Directory Entry */
//typedef struct {
//	uint8	filetype;	/* 0x00 = no file; 0x33 = data; 0xcc = a game */
//	uint8	copyprotect;	/* 0x00 = copyable; 0xff = copy protected */
//	uint16	firstblk;	/* location of the first block in the file */
//	char	filename[12];	/* there is no null terminator */
//	uint8	cent;		/* these are all file creation date stuff, in BCD format */
//	uint8	year;
//	uint8	month;
//	uint8	day;
//	uint8	hour;
//	uint8	min;
//	uint8	sec;
//	uint8	dow;		/* day of week (0 = monday, etc) */
//	uint16	filesize;	/* size of the file in blocks */
//	uint16	hdroff;		/* offset of header, in blocks from start of file */
//	char	dummys[4];	/* unused */
//} dirent_vmu;

/* ROM menu header */
//typedef struct {
//	char	desc_short[16];
//	char	desc_long[32];
//	char	app_id[16];
//	uint16	icon_cnt;
//	uint16	eyecatch_type;
//	uint16	crc;
//	uint32	file_bytes;
//	char	reserved[20];
//	uint16	palette[16];
//	uint8	icon_bitmap[32*32/8];
//} file_hdr_vmu;

typedef struct
{
	uint16 opt_P1AnalogEnabled;
	uint16 opt_P2AnalogEnabled;
	uint16 opt_P1SelectKey;
	uint16 opt_P1AKey;
	uint16 opt_P1BKey;
	uint16 opt_P2SelectKey;
	uint16 opt_P2AKey;
	uint16 opt_P2BKey;
	uint16 opt_Stretch;
	uint16 opt_Filter;
	uint16 opt_ClipVars[4];
	uint16 opt_SoundEnabled;
	uint16 opt_FrameSkip;
	uint16 opt_AutoFrameSkip;
	uint16 opt_DiscFormat;
	uint16 opt_SRAM;
	uint16 GUI_OutsideWindowColor;
	uint16 GUI_InsideWindowColor;
	uint16 GUI_TextColor;
	uint16 GUI_SelectedTextColor;
	uint32 GUI_BGColor;
} user_settings;

uint16 calcCRC(const unsigned char *buf, int size);

uint16 get_fat_block (uint8 vmu_addr, uint16 block_number);

uint16 find_file (uint8 vmu_addr, char* filename);

void update_fat (uint8 vmu_addr, uint8 *free_mem, uint8 blocks_left);

void upload_vmu_data (uint8 vmu_addr, uint8 *vmu_file, uint8 *free_mem, uint8 blocks_left);

void upload_data_by_block (uint8 vmu_addr, uint8 *vmu_file, uint16 block_number, uint8 blocks_left);

void update_vmu_dir (uint8 vmu_addr, uint16 block, const char *filename, uint8 blocks);

void do_crc (uint8 *buffer, uint16 bytes);

uint8 check_free_blocks (uint8 vmu_addr, uint8 *free_mem);

uint16 find_first_free_block (uint8 *fat_table);

void create_dir_vmu (vmu_dir_t *dir_entry, const char *rom_name, uint16 first_free_block, uint8 blocks);

void create_vmu_header(uint8 *header, const char *rom_name, const char *desc_long, uint16 filesize);

void vmu_icon_draw(const char *vmu_icon, uint8 vmu);

int16 load_user_settings(uint8 vmu_addr);

int16 save_user_settings(uint8 vmu_addr);

int16 save_SRAM(uint8 vmu_addr, char* filename, char* description, unsigned char* SRAM_ptr);
int16 load_SRAM(uint8 vmu_addr, char* filename, unsigned char* SRAM_ptr);

#endif
