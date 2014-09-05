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
void bf_load_file_pvr(char* filename, uint32 PVR_Offset, uint32 MaxBytesPerChar, unsigned char* WidthsArray, unsigned char* HeightsArray)
{
	BMF_Character TempFont[NUM_CHARS];
	uint16 TexBuffer[64*64];
	uint32 i;
	uint32 j;
	uint32 k;

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

		ta_txr_load(PVR_Offset + (i * MaxBytesPerChar), TexBuffer, MaxBytesPerChar);
		WidthsArray[i] = TempFont[i].width;
		HeightsArray[i] = TempFont[i].height;
	
		//Flash Temp Ram
		for (j = 0; j < 64*64; j++)
			TexBuffer[j] = 0x0000;
	}

	bf_free_font(TempFont);
}

//WidthsArray must be NUM_CHARS bytes long
//HeightsArray must be NUM_CHARS bytes long
void bf_load_file_pvr_colors(char* filename, uint32 PVR_Offset, uint32 MaxBytesPerChar, unsigned char* WidthsArray, unsigned char* HeightsArray, uint16 fontcolor, uint16 bgcolor)
{
	BMF_Character TempFont[NUM_CHARS];
	uint16 TexBuffer[64*64];
	uint32 i;
	uint32 j;
	uint32 k;

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
						TexBuffer[j + (64 * k)] = bgcolor;
					else
						TexBuffer[j + (64 * k)] = fontcolor;
		}

		ta_txr_load(PVR_Offset + (i * MaxBytesPerChar), TexBuffer, 64 * 64 * 2);
		WidthsArray[i] = TempFont[i].width;
		HeightsArray[i] = TempFont[i].height;
	
		//Flash Temp Ram
		for (j = 0; j < 64*64; j++)
			TexBuffer[j] = 0x0000;
	}

	bf_free_font(TempFont);
}


void bf_ta_submit_character(uint32 PVR_Offset, float xposition, float yposition, float zposition, float screenwidth, float screenheight, unsigned char width, unsigned char height)
{
	poly_hdr_t my_pheader;
	vertex_ot_t my_vertex;
	ta_poly_hdr_txr(&my_pheader, TA_TRANSLUCENT, TA_ARGB1555, 64, 64, PVR_Offset, TA_BILINEAR_FILTER);
	ta_commit_poly_hdr(&my_pheader);

	my_vertex.flags = TA_VERTEX_NORMAL;
	my_vertex.x = xposition;
	my_vertex.y = yposition + screenheight;
	my_vertex.z = zposition;
	my_vertex.u = 0.0f;
	my_vertex.v = ((float) height) / 64.0f;
	my_vertex.a = 1.0f;
	my_vertex.r = 1.0f;
	my_vertex.g = 1.0f;
	my_vertex.b = 1.0f;
	my_vertex.oa = 1.0f;
	my_vertex.or = 1.0f;
	my_vertex.og = 1.0f;
	my_vertex.ob = 1.0f;
	my_vertex.dummy1 = 0;
	my_vertex.dummy2 = 0;
	ta_commit_vertex(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = TA_VERTEX_NORMAL;
	my_vertex.y = yposition;
	my_vertex.u = 0.0f;
	my_vertex.v = 0.0f;
	my_vertex.a = 1.0f;
	my_vertex.r = 1.0f;
	my_vertex.g = 1.0f;
	my_vertex.b = 1.0f;
	my_vertex.oa = 1.0f;
	my_vertex.or = 1.0f;
	my_vertex.og = 1.0f;
	my_vertex.ob = 1.0f;
	ta_commit_vertex(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = TA_VERTEX_NORMAL;
	my_vertex.x = xposition + screenwidth;
	my_vertex.y = yposition + screenheight;
	my_vertex.u = ((float) width) / 64.0f;
	my_vertex.v = ((float) height) / 64.0f;
	my_vertex.a = 1.0f;
	my_vertex.r = 1.0f;
	my_vertex.g = 1.0f;
	my_vertex.b = 1.0f;
	my_vertex.oa = 1.0f;
	my_vertex.or = 1.0f;
	my_vertex.og = 1.0f;
	my_vertex.ob = 1.0f;
	ta_commit_vertex(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = TA_VERTEX_EOL;
	my_vertex.y = yposition;
	my_vertex.u = ((float) width) / 64.0f;
	my_vertex.v = 0.0f;
	my_vertex.a = 1.0f;
	my_vertex.r = 1.0f;
	my_vertex.g = 1.0f;
	my_vertex.b = 1.0f;
	my_vertex.oa = 1.0f;
	my_vertex.or = 1.0f;
	my_vertex.og = 1.0f;
	my_vertex.ob = 1.0f;
	ta_commit_vertex(&my_vertex, sizeof(my_vertex));
}

void bf_ta_submit_string(uint32 PVR_Offset, float xposition, float yposition, float zposition, float screenwidth, float screenheight, unsigned char* fontwidths, unsigned char* fontheights, char* my_string)
{
	int largestwidth;
	int tempwidth;
	int totalwidth;
	int tempindex;
	int i;
	float xstart;
	float xwidth;

	largestwidth = 0;
	totalwidth = 0;
	for (i = 0; i < strlen(my_string); i++)
	{
		tempwidth = fontwidths[bf_char_index(my_string[i])];
		totalwidth += tempwidth;
		if (largestwidth < tempwidth)
			largestwidth = tempwidth;
	}
	
	xstart = xposition;
	for (i = 0;	i < strlen(my_string); i++)
	{
		tempindex = bf_char_index(my_string[i]);
		xwidth = ((float) fontwidths[tempindex]) / totalwidth * screenwidth;
		if ((tempindex == 27) && (my_string[i] != (char) 98))
		{
			xstart += xwidth;		
		}
		else
		{
			bf_ta_submit_character(PVR_Offset + (tempindex * 64 * 64 * 2), xstart, yposition, zposition, xwidth, screenheight, fontwidths[tempindex], fontheights[tempindex]);
			xstart += (xwidth - 2);
		}
	}
}


void bf_ta_submit_character_alpha(uint32 PVR_Offset, float xposition, float yposition, float zposition, float screenwidth, float screenheight, unsigned char width, unsigned char height, float alpha)
{
	poly_hdr_t my_pheader;
	vertex_ot_t my_vertex;
	ta_poly_hdr_txr(&my_pheader, TA_TRANSLUCENT, TA_ARGB1555, 64, 64, PVR_Offset, TA_BILINEAR_FILTER);
	ta_commit_poly_hdr(&my_pheader);

	my_vertex.flags = TA_VERTEX_NORMAL;
	my_vertex.x = xposition;
	my_vertex.y = yposition + screenheight;
	my_vertex.z = zposition;
	my_vertex.u = 0.0f;
	my_vertex.v = ((float) height) / 64.0f;
	my_vertex.a = alpha;
	my_vertex.r = 1.0f;
	my_vertex.g = 1.0f;
	my_vertex.b = 1.0f;
	my_vertex.oa = 1.0f;
	my_vertex.or = 1.0f;
	my_vertex.og = 1.0f;
	my_vertex.ob = 1.0f;
	my_vertex.dummy1 = 0;
	my_vertex.dummy2 = 0;
	ta_commit_vertex(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = TA_VERTEX_NORMAL;
	my_vertex.y = yposition;
	my_vertex.u = 0.0f;
	my_vertex.v = 0.0f;
	my_vertex.a = alpha;
	my_vertex.r = 1.0f;
	my_vertex.g = 1.0f;
	my_vertex.b = 1.0f;
	my_vertex.oa = 1.0f;
	my_vertex.or = 1.0f;
	my_vertex.og = 1.0f;
	my_vertex.ob = 1.0f;
	ta_commit_vertex(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = TA_VERTEX_NORMAL;
	my_vertex.x = xposition + screenwidth;
	my_vertex.y = yposition + screenheight;
	my_vertex.u = ((float) width) / 64.0f;
	my_vertex.v = ((float) height) / 64.0f;
	my_vertex.a = alpha;
	my_vertex.r = 1.0f;
	my_vertex.g = 1.0f;
	my_vertex.b = 1.0f;
	my_vertex.oa = 1.0f;
	my_vertex.or = 1.0f;
	my_vertex.og = 1.0f;
	my_vertex.ob = 1.0f;
	ta_commit_vertex(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = TA_VERTEX_EOL;
	my_vertex.y = yposition;
	my_vertex.u = ((float) width) / 64.0f;
	my_vertex.v = 0.0f;
	my_vertex.a = alpha;
	my_vertex.r = 1.0f;
	my_vertex.g = 1.0f;
	my_vertex.b = 1.0f;
	my_vertex.oa = 1.0f;
	my_vertex.or = 1.0f;
	my_vertex.og = 1.0f;
	my_vertex.ob = 1.0f;
	ta_commit_vertex(&my_vertex, sizeof(my_vertex));
}

void bf_ta_submit_string_alpha(uint32 PVR_Offset, float xposition, float yposition, float zposition, float screenwidth, float screenheight, unsigned char* fontwidths, unsigned char* fontheights, char* my_string, float alpha)
{
	int largestwidth;
	int tempwidth;
	int totalwidth;
	int tempindex;
	int i;
	float xstart;
	float xwidth;

	largestwidth = 0;
	totalwidth = 0;
	for (i = 0; i < strlen(my_string); i++)
	{
		tempwidth = fontwidths[bf_char_index(my_string[i])];
		totalwidth += tempwidth;
		if (largestwidth < tempwidth)
			largestwidth = tempwidth;
	}
	
	xstart = xposition;
	for (i = 0;	i < strlen(my_string); i++)
	{
		tempindex = bf_char_index(my_string[i]);
		xwidth = ((float) fontwidths[tempindex]) / totalwidth * screenwidth;
		if ((tempindex == 27) && (my_string[i] != (char) 98))
		{
			xstart += xwidth;		
		}
		else
		{
			bf_ta_submit_character_alpha(PVR_Offset + (tempindex * 64 * 64 * 2), xstart, yposition, zposition, xwidth, screenheight, fontwidths[tempindex], fontheights[tempindex], alpha);
			xstart += (xwidth - 2);
		}
	}
}
