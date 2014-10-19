/*
File:        BMFFont.h
Project:     Platform Development Tools
Date:        Sept. 24, 2014
Author:      regex
			 Based on http://www.angelcode.com/dev/bmfonts/ 
Description: Bitmap Font Library that is compatible with angelcode's BMF compiler
*/

#ifndef __BMFFONT_H
#define __BMFFONT_H

#include "uthash.h"
#include "utarray.h"

typedef struct BMFCharDescriptor {
	int key;
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
	UT_hash_handle* hh;
} BMFChar;

enum EFontTextEncoding {
	NONE,
	UTF8,
	UTF16
};

typedef struct BMFFont {
	short fontHeight; // total height of the font
	short base;       // y of base line
	short scaleW;
	short scaleH;
	BMFCharDescriptor* defChar;
	bool hasOutline;

	float scale;
	EFontTextEncoding encoding;

	BMFCharDescriptor* chars;
	//std::vector<IDirect3DTexture9*> pages;
} BMFFont;

#endif