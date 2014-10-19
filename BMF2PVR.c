/*===================================================================*/
/*                                                                   */
/*  BMF2PVR.c : Turn a BitmapFont into PVR textures                  */
/*              Not especially efficient... but works                */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#include "BMF2PVR.h"

extern const int NUM_CHARS;

int bf_char_index(char str)
{
	int chartodraw;
	switch ((int) str)
	{
		case 65:
			chartodraw = 0;
			break;

		case 66:
			chartodraw = 1;
			break;

		case 67:
			chartodraw = 2;
			break;

		case 68:
			chartodraw = 3;
			break;

		case 69:
			chartodraw = 4;
			break;

		case 70:
			chartodraw = 5;
			break;

		case 71:
			chartodraw = 6;
			break;

		case 72:
			chartodraw = 7;
			break;

		case 73:
			chartodraw = 8;
			break;

		case 74:
			chartodraw = 9;
			break;

		case 75:
			chartodraw = 10;
			break;

		case 76:
			chartodraw = 11;
			break;

		case 77:
			chartodraw = 12;
			break;

		case 78:
			chartodraw = 13;
			break;

		case 79:
			chartodraw = 14;
			break;

		case 80:
			chartodraw = 15;
			break;

		case 81:
			chartodraw = 16;
			break;

		case 82:
			chartodraw = 17;
			break;

		case 83:
			chartodraw = 18;
			break;

		case 84:
			chartodraw = 19;
			break;

		case 85:
			chartodraw = 20;
			break;

		case 86:
			chartodraw = 21;
			break;

		case 87:
			chartodraw = 22;
			break;

		case 88:
			chartodraw = 23;
			break;

		case 89:
			chartodraw = 24;
			break;

		case 90:
			chartodraw = 25;
			break;

		case 97:
			chartodraw = 26;
			break;

		case 98:
			chartodraw = 27;
			break;

		case 99:
			chartodraw = 28;
			break;

		case 100:
			chartodraw = 29;
			break;

		case 101:
			chartodraw = 30;
			break;

		case 102:
			chartodraw = 31;
			break;

		case 103:
			chartodraw = 32;
			break;

		case 104:
			chartodraw = 33;
			break;

		case 105:
			chartodraw = 34;
			break;

		case 106:
			chartodraw = 35;
			break;

		case 107:
			chartodraw = 36;
			break;

		case 108:
			chartodraw = 37;
			break;

		case 109:
			chartodraw = 38;
			break;

		case 110:
			chartodraw = 39;
			break;

		case 111:
			chartodraw = 40;
			break;

		case 112:
			chartodraw = 41;
			break;

		case 113:
			chartodraw = 42;
			break;

		case 114:
			chartodraw = 43;
			break;

		case 115:
			chartodraw = 44;
			break;

		case 116:
			chartodraw = 45;
			break;

		case 117:
			chartodraw = 46;
			break;

		case 118:
			chartodraw = 47;
			break;

		case 119:
			chartodraw = 48;
			break;

		case 120:
			chartodraw = 49;
			break;

		case 121:
			chartodraw = 50;
			break;

		case 122:
			chartodraw = 51;
			break;

		case 48:
			chartodraw = 52;
			break;

		case 49:
			chartodraw = 53;
			break;

		case 50:
			chartodraw = 54;
			break;
		
		case 51:
			chartodraw = 55;
			break;

		case 52:
			chartodraw = 56;
			break;

		case 53:
			chartodraw = 57;
			break;

		case 54:
			chartodraw = 58;
			break;

		case 55:
			chartodraw = 59;
			break;

		case 56:
			chartodraw = 60;
			break;

		case 57:
			chartodraw = 61;
			break;

		case 46:
			chartodraw = 62;
			break;

		case 44:
			chartodraw = 63;
			break;

		case 33:
			chartodraw = 64;
			break;

		case 63:
			chartodraw = 65;
			break;

		default:
			chartodraw = 27;
			break;
	}
	return chartodraw;
}

//WidthsArray must be NUM_CHARS bytes long
//HeightsArray must be NUM_CHARS bytes long
void bf_load_file_pvr(char* filename, void* PVR_Offset, uint32 MaxBytesPerChar, unsigned char* WidthsArray, unsigned char* HeightsArray, pvr_poly_hdr_t* hdr)
{
	BMF_Character TempFont[NUM_CHARS];
	uint16 TexBuffer[64*64];
	uint32 i;
	uint32 j;
	uint32 k;
	pvr_poly_cxt_t tmp;

	//Flash temp ram
	for (i = 0; i < (64*64); i++)
		TexBuffer[i] = 0x0000;

	bf_load_file(filename, TempFont);

	for (i = 0; i < NUM_CHARS; i++)
	{
		if (TempFont[i].bitdata != NULL)
		{
			for (j = 0; j < (TempFont[i].width); j++)
				for (k = 0; k < (TempFont[i].height); k++)
					if (TempFont[i].bitdata[j + (TempFont[i].width * k)] == 1)
						TexBuffer[j + (64 * k)] = 0xFFFF;
					else
						TexBuffer[j + (64 * k)] = 0x0000;
		}

		pvr_txr_load(PVR_Offset + (i * MaxBytesPerChar), TexBuffer, MaxBytesPerChar);
		WidthsArray[i] = TempFont[i].width;
		HeightsArray[i] = TempFont[i].height;
	
		//Flash Temp Ram
		for (j = 0; j < 64*64; j++)
			TexBuffer[j] = 0x0000;
	}

	bf_free_font(TempFont);

    pvr_poly_cxt_txr(&tmp, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
                     8192, 64, PVR_Offset, PVR_FILTER_NONE);
    pvr_poly_compile(hdr, &tmp);	
}

//WidthsArray must be NUM_CHARS bytes long
//HeightsArray must be NUM_CHARS bytes long
void bf_load_file_pvr_colors(char* filename, void* PVR_Offset, uint32 MaxBytesPerChar, unsigned char* WidthsArray, unsigned char* HeightsArray, uint16 fontcolor, uint16 bgcolor, pvr_poly_hdr_t* hdr)
{
/*	BMF_Character TempFont[NUM_CHARS];
	uint16* texptr = (uint16*)PVR_Offset;
	uint32 i;
	uint32 j;
	uint32 k;
	pvr_poly_cxt_t tmp;

	//Flash temp ram
	for (i = 0; i < (64*64*66); i++)
		texptr[i] = 0x0000;

	bf_load_file(filename, TempFont);

	for (i = 0; i < NUM_CHARS; i++)
	{
		if (TempFont[i].bitdata != NULL)
		{
			for (j = 0; j < (TempFont[i].width); j++)
				for (k = 0; k < (TempFont[i].height); k++)
					if (TempFont[i].bitdata[j + (TempFont[i].width * k)] == 1)
						texptr[(i * 64) + j + (8192 * k)] = bgcolor;
					else
						texptr[(i * 64) + j + (8192 * k)] = fontcolor;
		}

		WidthsArray[i] = TempFont[i].width;
		HeightsArray[i] = TempFont[i].height;
	}

	bf_free_font(TempFont);

    pvr_poly_cxt_txr(&tmp, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
                     8192, 64, PVR_Offset, PVR_FILTER_NONE);
    pvr_poly_compile(hdr, &tmp);	*/
}

void bf_ta_submit_character_alpha(float xposition, float yposition, float zposition, float screenwidth, float screenheight, unsigned char width, unsigned char height, float alpha, float u)
{
/*	pvr_vertex_t vert;

	vert.flags = PVR_CMD_VERTEX;
	vert.x = xposition;
	vert.y = yposition + screenheight;
	vert.z = zposition;
	vert.u = u;
	vert.v = ((float) height) / 64.0f;
	vert.argb = PVR_PACK_COLOR(alpha, 1.0f, 1.0f, 1.0f);
	vert.oargb = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
	pvr_prim(&vert, sizeof(vert));

	vert.y = yposition;
	vert.v = 0;
	pvr_prim(&vert, sizeof(vert));

	vert.x = xposition + screenwidth;
	vert.y = yposition + screenheight;
	vert.u = u + ((float) width) / 64.0f;	
	vert.v = ((float) height) / 64.0f;	
	pvr_prim(&vert, sizeof(vert));

	vert.flags = PVR_CMD_VERTEX_EOL;
	vert.y = yposition;
	vert.v = 0;
	pvr_prim(&vert, sizeof(vert));*/
}

void bf_ta_submit_string_alpha(pvr_poly_hdr_t* font_header, float xposition, float yposition, float zposition, float screenwidth, float screenheight, unsigned char* fontwidths, unsigned char* fontheights, char* my_string, float alpha)
{
/*	int largestwidth;
	int tempwidth;
	int totalwidth;
	int tempindex;
	int i;
	float xstart;
	float xwidth;
	float u;

	largestwidth = 0;
	totalwidth = 0;
	for (i = 0; i < strlen(my_string); i++)
	{
		tempwidth = fontwidths[bf_char_index(my_string[i])];
		totalwidth += tempwidth;
		if (largestwidth < tempwidth)
			largestwidth = tempwidth;
	}

	pvr_prim(font_header, sizeof(pvr_poly_hdr_t));
	
	xstart = xposition;
	for (i = 0;	i < strlen(my_string); i++)
	{
		tempindex = bf_char_index(my_string[i]);
		u = ((float)tempindex / 66.0f) * 0.515625f;
		xwidth = ((float) fontwidths[tempindex]) / totalwidth * screenwidth;
		if ((tempindex == 27) && (my_string[i] != (char) 98))
		{
			xstart += xwidth;		
		}
		else
		{
			bf_ta_submit_character_alpha(xstart, yposition, zposition, xwidth, screenheight, fontwidths[tempindex], fontheights[tempindex], alpha, u);
			xstart += (xwidth - 2);
		}
	}*/
}

void bf_ta_submit_string(pvr_poly_hdr_t* font_header, float xposition, float yposition, float zposition, float screenwidth, float screenheight, unsigned char* fontwidths, unsigned char* fontheights, char* my_string)
{
//	bf_ta_submit_string_alpha(font_header, xposition, yposition, zposition, screenwidth, screenheight, fontwidths, fontheights, my_string, 1.0f);
}
