/*
File:        BMFont.h
Project:     Platform Development Tools
Date:        Sept. 24, 2014
Author:      Matt Slevinsky
			 Based on http://www.angelcode.com/dev/bmfonts/ 
Description: Bitmap Font Library that is compatible with angelcode's BMF compiler
*/

#ifndef __BMFFONT_H
#define __BMFFONT_H

#include <kos.h>
#include "uthash.h"
#include "utarray.h"
#include "defines.h"

typedef struct KerningPair {
    uint32 first;
    uint32 second;
    int16 amount;
} KerningPair;

typedef struct BMFCharDescriptor {
	uint32 id;
	short srcX;
	short srcY;
	short srcW;
	short srcH;
	short xOff;
	short yOff;
	short xAdv;
	short page;
	unsigned int chnl;

	UT_array* kerningPairs;
	UT_hash_handle hh;
} BMFCharDescriptor;

typedef enum EFontTextEncoding {
	NONE,
	UTF8,
	UTF16
} EFontTextEncoding;

typedef struct BMFont {
	short fontHeight; // total height of the font
	short base;       // y of base line
	short scaleW;
	short scaleH;
	BMFCharDescriptor* defChar;
	bool hasOutline;

	float scale;
	EFontTextEncoding encoding;

	BMFCharDescriptor* characters;
	UT_array* pages;
} BMFont;

typedef struct BMFPage {
	short width;
	short height;	
	pvr_ptr_t	texture;
} BMFPage;

BMFont* load_BMF(const char* filepath);
void drawString(BMFont* font, char* string, float xPos, float yPos);
void destroy_BMF(BMFont* font);

#endif