/*
File:        BMFont.c
Project:     Platform Development Tools
Date:        Sept. 24, 2014
Author:      Matt Slevinsky
			 Based on http://www.angelcode.com/dev/bmfonts/ 
Description: Bitmap Font Library that is compatible with angelcode's BMF compiler
*/

#include <png/png.h>
#include "font.h"

#pragma pack(push)
#pragma pack(1)
typedef struct InfoBlock {
    uint16 fontSize;
    uint8 reserved:4;
    uint8 bold:1;
    uint8 italic:1;
    uint8 unicode:1;
    uint8 smooth:1;
    uint8 charSet;
    uint16 stretchH;
    uint8 aa;
    uint8 paddingUp;
    uint8 paddingRight;
    uint8 paddingDown;
    uint8 paddingLeft;
    uint8 spacingHoriz;
    uint8 spacingVert;
    uint8 outline;         // Added with version 2
    char fontName[];
} InfoBlock;
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
typedef struct CommonBlock
{
    uint16 lineHeight;
    uint16 base;
    uint16 scaleW;
    uint16 scaleH;
    uint16 pages;
    uint8 packed  :1;
    uint8 reserved:7;
	uint8 alphaChnl;
	uint8 redChnl;
	uint8 greenChnl;
	uint8 blueChnl;
} CommonBlock;
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
typedef struct CharInfo
{
    uint32 id;
    uint16  x;
    uint16  y;
    uint16  width;
    uint16  height;
    int16 xoffset;
    int16 yoffset;
    int16 xadvance;
    uint8  page;
    uint8  chnl;
} CharInfo;
#pragma pack(pop)

void readInfoBlock(uint32 fd, Font* font, int blockSize, int* outlineThickness) {
	InfoBlock* infoBlock = malloc(blockSize);
	fs_read(fd, infoBlock, blockSize);

	printf("INFO: Loading font %s\n", infoBlock -> fontName);
	printf("INFO: Font Size : %i\n", infoBlock -> fontSize);
	printf("INFO: Bold : %s\n", infoBlock -> bold ? "true" : "false");
	printf("INFO: Italic : %s\n", infoBlock -> italic ? "true" : "false");
	printf("INFO: Unicode : %s\n", infoBlock -> unicode ? "true" : "false");
	printf("INFO: Smooth : %s\n", infoBlock -> smooth ? "true" : "false");
	printf("INFO: Outline : %i\n", infoBlock -> outline);	

	// We're only interested in the outline thickness
	*outlineThickness = infoBlock -> outline;

	free(infoBlock);
}

void bmfpage_copy(void *_dst, const void *_src) {
  FontPage *dst = (FontPage*)_dst, *src = (FontPage*)_src;
  dst->width = src->width;
  dst->height = src->height;
  dst->texture = src->texture;
  //TODO : Reference count and release texture when refcount = 0
}

void bmfpage_dtor(void *_elt) {
  //MS - do we need this?
  //pvr_mem_free(elt -> texture);
}

UT_icd bmfpage_icd = {sizeof(FontPage), NULL, bmfpage_copy, bmfpage_dtor};

void readCommonBlock(uint32 fd, Font* font, int blockSize, int* outlineThickness) {
	CommonBlock* commonBlock = malloc(sizeof(CommonBlock));
	fs_read(fd, commonBlock, sizeof(CommonBlock));

	font -> fontHeight = commonBlock -> lineHeight;
	font -> base = commonBlock -> base;
	font -> scaleW = commonBlock -> scaleW;
	font -> scaleH = commonBlock -> scaleH;
	utarray_new(font -> pages, &bmfpage_icd);

	if ((commonBlock -> packed != 0) && 
		(*outlineThickness != 0))
		font -> hasOutline = 1;

	free(commonBlock);
}


void readPage(Font* font, char* pagePath) {
	uint32 w,h;
	FontPage* page = (FontPage*)malloc(sizeof(FontPage));
	png_load_texture(pagePath, &(page -> texture), PNG_FULL_ALPHA, &w, &h);
	page -> width = (uint16)w;
	page -> height = (uint16)h;	
  	utarray_push_back(font -> pages, page);
}

void readPagesBlock(uint32 fd, Font* font, int blockSize) {
	char* pageNames = malloc(blockSize);
	char* pagePtr = pageNames;
	fs_read(fd, pageNames, blockSize);

	int id = 0;
	int bytesProcessed = 0;
	int temp;
	do {
		char* pngPath = malloc((int)strlen(pagePtr) + 5);
		strcpy(pngPath, "/rd/"); // TODO : strip leading path from font file and look in the same location
		strcat(pngPath, pagePtr);
		printf("Loading texture %i:%s\n", id, pngPath);		

		readPage(font, pngPath);

		temp = 1 + (int)strlen(pagePtr);
		pagePtr += temp;
		bytesProcessed += temp;

		free(pngPath);
		id++;
	} while (bytesProcessed < blockSize);

	free(pageNames);
}

void addCharacter(Font* font, CharInfo charInfo) {
	//printf("Adding character mapping for character %c\n", (char)charInfo.id);

	CharDescriptor* bmfChar = malloc(sizeof(CharDescriptor));
	bmfChar -> id = charInfo.id;
	bmfChar -> srcX = charInfo.x;
	bmfChar -> srcY = charInfo.y;
	bmfChar -> srcW = charInfo.width;
	bmfChar -> srcH = charInfo.height;
	bmfChar -> xOff = charInfo.xoffset;
	bmfChar -> yOff = charInfo.yoffset;
	bmfChar -> xAdv = charInfo.xadvance;
	bmfChar -> page = charInfo.page;
	bmfChar -> chnl = charInfo.chnl;
	utarray_new(bmfChar -> kerningPairs, &ut_int_icd);

	if (charInfo.id == -1) {
		font -> defChar = bmfChar;
	}

	CharDescriptor** characters = &(font -> characters);
	HASH_ADD_INT(*characters, id, bmfChar);
}

void readCharsBlock(uint32 fd, Font* font, int blockSize) {
	CharInfo* charInfos = malloc(blockSize);
	fs_read(fd, charInfos, blockSize);

	font -> characters = NULL;

	int i;
	for (i = 0; (i * sizeof(CharInfo)) < blockSize; i++) {
		addCharacter(font, charInfos[i]);
	}

	free(charInfos);
}

void addKerningPair(Font* font, KerningPair kerningPair) {
	if (kerningPair.first >= 0 && kerningPair.first < 256) {
		CharDescriptor** characters = &(font -> characters);
		CharDescriptor* descriptor;
		HASH_FIND_INT(*characters, &(kerningPair.first), descriptor);
		if (descriptor != NULL) {
			//printf("Adding kerning pair for character %c\n", (char)descriptor -> id);
			utarray_push_back(descriptor -> kerningPairs, &kerningPair.second);
			utarray_push_back(descriptor -> kerningPairs, &kerningPair.amount);
		}
	}	
}

void readKerningPairsBlock(uint32 fd, Font* font, int blockSize) {
	KerningPair* kerningPairs = malloc(blockSize);
	fs_read(fd, kerningPairs, blockSize);

	int i;
	for (i = 0; (i * sizeof(KerningPair)) < blockSize; i++) {
		addKerningPair(font, kerningPairs[i]);
	}

	free(kerningPairs);
}

Font* load_font(const char* filepath) {
	uint32 fd;
	Font* font = NULL;
	int outlineThickness;

	fd = fs_open(filepath, O_RDONLY);
	if (fd > 0) {
		printf("Opened file path [%s] successfully for reading\n", filepath);

		char magicString[4];
		if (fs_read(fd, magicString, 4) == 4) {
			if (strncmp(magicString, "BMF\003", 4) != 0) {
				printf("Unrecognized format for %s detected.  Aborting font load.\n", filepath);
				fs_close(fd);
				return NULL;
			}

 			font = malloc(sizeof(Font));

			char blockType;
			int blockSize;
			while (fs_read(fd, &blockType, 1)) {
				// Read the blockSize
				fs_read(fd, &blockSize, 4);

				switch( blockType ) {
					case 1: // info
						printf("Detected info block\n");
						readInfoBlock(fd, font, blockSize, &outlineThickness);
						break;
					case 2: // common
						printf("Detected common block\n");				
						readCommonBlock(fd, font, blockSize, &outlineThickness);
						break;
					case 3: // pages
						printf("Detected pages block\n");				
						readPagesBlock(fd, font, blockSize);
						break;
					case 4: // chars
						printf("Detected chars block\n");				
						readCharsBlock(fd, font, blockSize);
						break;
					case 5: // kerning pairs
						printf("Detected kerning pairs block\n");				
						readKerningPairsBlock(fd, font, blockSize);
						break;
					default:
						printf("Unexpected block type (%d) detected.  Aborting font load.\n", blockType);
						free(font);
						fs_close(fd);
						return NULL;
				}
			}
		}
	} else {
		printf("Unable to open file path [%s] for reading\n", filepath);
	}
	printf("Font Loaded successfully.\n");
	return font;
}

void deleteAllDescriptors(CharDescriptor** root) {
	CharDescriptor* currentDescriptor;
	CharDescriptor* tmp;

	HASH_ITER(hh, *root, currentDescriptor, tmp) {
		utarray_free(currentDescriptor->kerningPairs);
		HASH_DEL(*root,currentDescriptor);  /* delete; users advances to next */
		free(currentDescriptor);            /* optional- if you want to free  */
	}
}

void deleteAllPages(Font* font) {
	FontPage* page;
	for (page=(FontPage*)utarray_front(font -> pages);
    	page!=NULL;
    	page=(FontPage*)utarray_next(font -> pages,page)) {
		pvr_mem_free(page -> texture);
	}

	utarray_free(font -> pages);	
}

void destroy_font(Font* font) {
	font -> defChar = NULL;
	deleteAllDescriptors(&(font->characters));
	deleteAllPages(font);
}

void measureLine(Font* font, char* string, float scale, float* measuredWidth) {
	char* ptr = string;
	float accX = 0.0f;

	CharDescriptor** descriptors = &(font -> characters);
	CharDescriptor* descriptor;
	while (((*ptr) != 0) && ((*ptr) != '\n')) {
		uint32 id = (uint32)*ptr;

		HASH_FIND_INT(*descriptors, &id, descriptor);
		if (descriptor != NULL) {
			accX += (descriptor -> xAdv * scale);
		} else {
			accX += (font -> defChar -> xAdv * scale);
		}

		ptr++;
	}		

	*measuredWidth = accX;
}

void drawChar(CharDescriptor* descriptor, float xPos, float yPos, float zPos, int texWidth, int texHeight, uint32 color, float scale) {
    pvr_vertex_t    vert;
    float           u1, v1, u2, v2, yDatum;

	yDatum = yPos + (scale * (float)descriptor -> yOff);
    u1 = (float)descriptor -> srcX / (float)texWidth;
    v1 = (float)descriptor -> srcY / (float)texHeight;
    u2 = ((float)descriptor -> srcX + descriptor -> srcW) / (float)texWidth;
    v2 = ((float)descriptor -> srcY + descriptor -> srcH) / (float)texHeight;

    vert.flags = PVR_CMD_VERTEX;
    vert.x = xPos;
    vert.y = yDatum + ((float)descriptor -> srcH * scale);
    vert.z = zPos;
    vert.u = u1;
    vert.v = v2;
    vert.argb = color;
    vert.oargb = 0;
    pvr_prim(&vert, sizeof(vert));

    vert.y = yDatum;
    vert.u = u1;
    vert.v = v1;
    pvr_prim(&vert, sizeof(vert));

    vert.x = xPos + ((float)descriptor -> srcW * scale);
    vert.y = yDatum + ((float)descriptor -> srcH * scale);
    vert.u = u2;
    vert.v = v2;
    pvr_prim(&vert, sizeof(vert));

    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.y = yDatum;
    vert.u = u2;
    vert.v = v1;
    pvr_prim(&vert, sizeof(vert));	
}

void draw_string(Font* font, int currentList, char* string, 
	float xPos, float yPos, float zPos, float width, float height, 
	LineRenderMode lineRenderMode, TextAlignmentMode textAlignmentMode, uint32 color, float scale) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;

	char* ptr = string;
	FontPage* page = NULL;
	int currentPage = -1;
	float drawX = xPos;

	if (textAlignmentMode == RIGHT) {
		float measuredWidth;
		measureLine(font, string, scale, &measuredWidth);
		drawX = xPos + width - measuredWidth;
	}

	CharDescriptor** descriptors = &(font -> characters);
	CharDescriptor* descriptor;
	while ((*ptr) != 0) {
		uint32 id = (uint32)*ptr;

		HASH_FIND_INT(*descriptors, &id, descriptor);
		if (descriptor != NULL) {
			if (descriptor -> page != currentPage) {
				//printf("Selected texture at index %i\n", descriptor -> page);
				page = (FontPage*)utarray_eltptr(font -> pages, descriptor -> page);

				if (page != NULL) {
					currentPage = descriptor -> page;
					//printf("Load font texture of resolution %i x %i\n", (short)page -> width, (short)page -> height);
					pvr_poly_cxt_col(&cxt, currentList);
			    	pvr_poly_cxt_txr(&cxt, currentList, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_TWIDDLED, page -> width, page -> height, page -> texture, PVR_FILTER_BILINEAR);
			    	pvr_poly_compile(&hdr, &cxt);
    				pvr_prim(&hdr, sizeof(hdr));
			    } else {
			    	printf("Failed to load page at index %i\n", descriptor -> page);
			    }
			}

			if (currentPage != -1) {
				drawChar(descriptor, drawX, yPos, zPos, page -> width, page -> height, color, scale);
			}

			drawX += ((float)descriptor -> xAdv * scale);
		} else {
			drawX += ((float)font -> defChar -> xAdv * scale);
		}

		if (drawX >= xPos + width) {
			break;
		}

		ptr++;
	}	
}
