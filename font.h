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

typedef enum EFontTextEncoding {
	NONE,
	UTF8,
	UTF16
} EFontTextEncoding;

typedef enum LineRenderMode {
	SINGLE,
	MULTILINE
} LineRenderMode;

typedef enum TextAlignmentMode {
	LEFT,
	CENTER,
	RIGHT
} TextAlignmentMode;

typedef struct KerningPair {
    uint32 first;
    uint32 second;
    int16 amount;
} KerningPair;

typedef struct CharDescriptor {
	uint32 id;
	int16 srcX;
	int16 srcY;
	int16 srcW;
	int16 srcH;
	int16 xOff;
	int16 yOff;
	int16 xAdv;
	int16 page;
	unsigned int chnl;

	UT_array* kerningPairs;
	UT_hash_handle hh;
} CharDescriptor;

typedef struct FontPage {
	uint16 width;
	uint16 height;	
	pvr_ptr_t texture;
} FontPage;

typedef struct Font {
	uint16 fontHeight; // total height of the font
	uint16 base;       // y of base line
	uint16 scaleW;
	uint16 scaleH;
	CharDescriptor* defChar;
	int hasOutline;

	float scale;
	EFontTextEncoding encoding;

	CharDescriptor* characters;
	UT_array* pages;
} Font;

Font* load_font(const char* filepath);
void draw_string(Font* font, int currentList, char* string, float xPos, float yPos, float zPos, float width, float height, LineRenderMode lineRenderMode, TextAlignmentMode textAlignmentMode, uint32 color, float scale);
//void measure_line(char* string, uint16* width, uint16* height);
//void measure_string(char* string, uint16* width, uint16* height, LineRenderMode lineRenderMode, TextAlignmentMode textAlignmentMode);
void destroy_font(Font* font);

#endif