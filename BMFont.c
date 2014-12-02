/*
File:        BMFont.c
Project:     Platform Development Tools
Date:        Sept. 24, 2014
Author:      Matt Slevinsky
			 Based on http://www.angelcode.com/dev/bmfonts/ 
Description: Bitmap Font Library that is compatible with angelcode's BMF compiler
*/

#include <png/png.h>
#include <stdio.h>
#include <assert.h>
#include "BMFont.h"

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
    char* fontName;
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

void readInfoBlock(uint32 fd, BMFont* font, int blockSize, int* outlineThickness) {
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
  BMFPage *dst = (BMFPage*)_dst, *src = (BMFPage*)_src;
  dst->width = src->width;
  dst->height = src->height;
  dst->texture = src->texture;
  //TODO : Reference count and release texture when refcount = 0
}

void bmfpage_dtor(void *_elt) {
  BMFPage *elt = (BMFPage*)_elt;
//  if (elt->s) free(elt->s);
}

UT_icd bmfpage_icd = {sizeof(BMFPage), NULL, bmfpage_copy, bmfpage_dtor};

void readCommonBlock(uint32 fd, BMFont* font, int blockSize, int* outlineThickness) {
	assert(blockSize == sizeof(CommonBlock));

	CommonBlock* commonBlock = malloc(sizeof(CommonBlock));
	fs_read(fd, commonBlock, sizeof(CommonBlock));

	font -> fontHeight = commonBlock -> lineHeight;
	font -> base = commonBlock -> base;
	font -> scaleW = commonBlock -> scaleW;
	font -> scaleH = commonBlock -> scaleH;
	utarray_new(font -> pages, &bmfpage_icd);
/*	int n;
	for (n = 0; n < commonBlock -> pages; n++) {
		utarray_push_back(font -> pages, NULL);
	}*/

	if ((commonBlock -> packed != 0) && 
		(*outlineThickness != 0))
		font -> hasOutline = true;

	free(commonBlock);
}

//---BEGIN Copied from libpng kos port
/* load an n x n texture from a png */
#define LOAD565(r, g, b) (((r>>3)<<11) | ((g>>2)<<5) | ((b>>3)))
#define LOAD1555(r, g, b, a) (((a>>7)<<15)|((r>>3)<<10)|((g>>3)<<5)|((b>>3)))
#define LOAD4444(r, g, b, a) (((a>>4)<<12)|((r>>4)<<8)|((g>>4)<<4)|((b>>4)))

/* not to be used outside of here */
void png_copy_texture(uint8 *buffer, uint16 *temp_tex,
                      uint32 channels, uint32 stride,
                      uint32 mask, uint32 w, uint32 h) {
  uint32 i,j;
  uint16 *ourbuffer;
  uint8 *pRow;

  if (channels == 3) {
  	printf("Detected 3 color channel mode\n");
  } else if (channels == 4) {
  	printf("Detected 4 color channel mode\n");
  }
  
  for(i = 0; i < h; i++)
  {
    pRow = &buffer[i*stride];
    ourbuffer = &temp_tex[i*w];
    
    if (channels == 3)
    {
      if (mask == PNG_NO_ALPHA)
        for(j = 0; j < w; j++)
          ourbuffer[j] = LOAD565(pRow[j*3], pRow[j*3+1], pRow[j*3+2]);
      else if (mask == PNG_MASK_ALPHA)
        for(j = 0; j < w; j++)
          ourbuffer[j] = LOAD1555(pRow[j*3],pRow[j*3+1],pRow[j*3+2],255);
      else if (mask == PNG_FULL_ALPHA)
        for(j = 0; j < w; j++)
          ourbuffer[j] = LOAD4444(pRow[j*3],pRow[j*3+1],pRow[j*3+2],255);
          
    }
    else if (channels == 4)
    {
      if (mask == PNG_NO_ALPHA)
      {
        for(j = 0; j < w; j++)
          ourbuffer[j] = LOAD565(pRow[j*4], pRow[j*4+1], pRow[j*4+2]);
      }
      else if (mask == PNG_MASK_ALPHA)
        for(j = 0; j < w; j++) {
          ourbuffer[j] = LOAD1555(pRow[j*4],pRow[j*4+1],pRow[j*4+2],pRow[j*4+3]);
        }
      else if (mask == PNG_FULL_ALPHA)
        for(j = 0; j < w; j++) {
        	if (pRow[j*4+3] != 0) {
          		ourbuffer[j] = LOAD4444(pRow[j*4],pRow[j*4+1],pRow[j*4+2],pRow[j*4+3]);
          	} else {
          		ourbuffer[j] = 0x0000;
          	}
//          printf("RGBA : %i %i %i %i converted to %04x\n", pRow[j*4],pRow[j*4+1],pRow[j*4+2],pRow[j*4+3], ourbuffer[j]);          
         }
    }
  }
}

//--END

void readPage(BMFont* font, char* pagePath) {
	uint8 *buffer;            /* Output row buffer */
	uint32 row_stride;   /* physical row width in output buffer */
	uint32 channels;           /* 3 for RGB 4 for RGBA */
	uint32 w,h;
	uint32 mask = PNG_FULL_ALPHA;

	FILE* fd = fopen(pagePath, "r");
	if (fd != NULL) {
		void* pngStructs = readpng_init(fd);
		if (pngStructs != NULL) {
  			buffer = readpng_get_image(pngStructs, &channels, &row_stride, &w, &h);

  			uint16* textureBuffer = malloc(2*w*h);

    		png_copy_texture(buffer, textureBuffer, channels, row_stride, mask, w, h); 				

    		BMFPage* page = malloc(sizeof(BMFPage));
    		page -> width = (short)w;
    		page -> height = (short)h;
    		page -> texture = pvr_mem_malloc(w*h*2);
    		printf("Loading Image Into %i x %i 16BPP Texture\n", (short)w, (short)h);
    		printf("Page -> texture is 0x%08x\n", page -> texture);

    		pvr_txr_load(textureBuffer, page -> texture, 2 * w * h);    		    		

    		free(buffer);
    		readpng_cleanup(pngStructs);

    		fclose(fd);
  			free(textureBuffer);

  			utarray_push_back(font -> pages, page);
		}
		fclose(fd);
	}
}

void readPagesBlock(uint32 fd, BMFont* font, int blockSize) {
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
	} while (bytesProcessed < blockSize);

	free(pageNames);
}

void addCharacter(BMFont* font, CharInfo charInfo) {
	printf("Adding character mapping for character %c\n", (char)charInfo.id);

	BMFCharDescriptor* bmfChar = malloc(sizeof(BMFCharDescriptor));
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

	BMFCharDescriptor** characters = &(font -> characters);
	HASH_ADD_INT(*characters, id, bmfChar);
}

void readCharsBlock(uint32 fd, BMFont* font, int blockSize) {
	CharInfo* charInfos = malloc(blockSize);
	fs_read(fd, charInfos, blockSize);

	font -> characters = NULL;

	int i;
	for (i = 0; (i * sizeof(CharInfo)) < blockSize; i++) {
		addCharacter(font, charInfos[i]);
	}

	free(charInfos);
}

void addKerningPair(BMFont* font, KerningPair kerningPair) {
	if (kerningPair.first >= 0 && kerningPair.first < 256) {
		BMFCharDescriptor** characters = &(font -> characters);
		BMFCharDescriptor* descriptor;
		HASH_FIND_INT(*characters, &(kerningPair.first), descriptor);
		if (descriptor != NULL) {
			printf("Adding kerning pair for character %c\n", (char)descriptor -> id);
			utarray_push_back(descriptor -> kerningPairs, &kerningPair.second);
			utarray_push_back(descriptor -> kerningPairs, &kerningPair.amount);
		}
	}	
}

void readKerningPairsBlock(uint32 fd, BMFont* font, int blockSize) {
	KerningPair* kerningPairs = malloc(blockSize);
	fs_read(fd, kerningPairs, blockSize);

	int i;
	for (i = 0; (i * sizeof(KerningPair)) < blockSize; i++) {
		addKerningPair(font, kerningPairs[i]);
	}

	free(kerningPairs);
}

BMFont* load_BMF(const char* filepath) {
	uint32 fd;
	BMFont* font;
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

 			font = malloc(sizeof(BMFont));

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

void deleteAllDescriptors(BMFCharDescriptor** root) {
	BMFCharDescriptor* currentDescriptor;
	BMFCharDescriptor* tmp;

	HASH_ITER(hh, *root, currentDescriptor, tmp) {
		utarray_free(currentDescriptor->kerningPairs);
		HASH_DEL(*root,currentDescriptor);  /* delete; users advances to next */
		free(currentDescriptor);            /* optional- if you want to free  */
	}
}

void deleteAllPages(BMFont* font) {
	BMFPage* page;
	for (page=(BMFPage*)utarray_front(font -> pages);
    	page!=NULL;
    	page=(BMFPage*)utarray_next(font -> pages,page)) {
		pvr_mem_free(page -> texture);
	}

	utarray_free(font -> pages);	
}

void destroy_BMF(BMFont* font) {
	font -> defChar = NULL;
	deleteAllDescriptors(&(font->characters));
	deleteAllPages(font);
}

void drawChar(BMFCharDescriptor* descriptor, float xPos, float yPos, int texWidth, int texHeight, int baseline, uint32 color) {
    pvr_vertex_t    vert;
    float           u1, v1, u2, v2;

    u1 = (float)descriptor -> srcX / (float)texWidth;
    v1 = (float)descriptor -> srcY / (float)texHeight;
    u2 = ((float)descriptor -> srcX + (float)descriptor -> srcW) / (float)texWidth;
    v2 = ((float)descriptor -> srcY + (float)descriptor -> srcH) / (float)texHeight;

    vert.flags = PVR_CMD_VERTEX;
    vert.x = xPos;
    vert.y = yPos + baseline;
    vert.z = 514.0f;
    vert.u = u1;
    vert.v = v2;
    vert.argb = color;
    vert.oargb = 0;
    pvr_prim(&vert, sizeof(vert));

    vert.y = yPos + baseline - descriptor -> srcH;
    vert.u = u1;
    vert.v = v1;
    pvr_prim(&vert, sizeof(vert));

    vert.x = xPos + descriptor -> srcW;
    vert.y = yPos + baseline;
    vert.u = u2;
    vert.v = v2;
    pvr_prim(&vert, sizeof(vert));

    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.y = yPos + baseline - descriptor -> srcH;
    vert.u = u2;
    vert.v = v1;
    pvr_prim(&vert, sizeof(vert));	
}

void drawString(BMFont* font, int currentList, char* string, float xPos, float yPos, uint32 color) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;

	char* ptr = string;
	BMFPage* page;
	int currentPage = -1;
	float drawX = xPos;

	while ((*ptr) != 0) {
		uint32 id = (uint32)*ptr;
		BMFCharDescriptor** descriptors = &(font -> characters);
		BMFCharDescriptor* descriptor;

		HASH_FIND_INT(*descriptors, &id, descriptor);
		if (descriptor != NULL) {
			if (descriptor -> page != currentPage) {
				//printf("Selected texture at index %i\n", descriptor -> page);

				page = (BMFPage*)utarray_eltptr(font -> pages, descriptor -> page);

				if (page != NULL) {
					currentPage = descriptor -> page;
					//printf("Load font texture of resolution %i x %i\n", (short)page -> width, (short)page -> height);
					pvr_poly_cxt_col(&cxt, currentList);
			    	pvr_poly_cxt_txr(&cxt, currentList, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, page -> width, page -> height, page -> texture, PVR_FILTER_BILINEAR);
			    	pvr_poly_compile(&hdr, &cxt);
    				pvr_prim(&hdr, sizeof(hdr));
			    } else {
			    	printf("Failed to load page at index %i\n", descriptor -> page);
			    }
			}

			if (currentPage != -1) {
				drawChar(descriptor, drawX, yPos, page -> width, page -> height, (int)font -> base, color);
			}

			drawX += descriptor -> xAdv;
		} else {
			drawX += font -> defChar -> xAdv;
		}

		ptr++;
	}	
}





