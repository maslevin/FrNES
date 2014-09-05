/*
File:        BMFFile.c
Project:     Platform Development Tools
Date:        December. 11th, 2000
Programmer:  ReGex
Description: File that contains a single bitmapped font face
Updates:  October 9th, 2001 - Fix for Dynamic Memory Allocation Under KOS
*/

#include "BMFFile.h"
#include "BMFConsts.h"

int filesize;
unsigned char* my_cd_buffer;
unsigned char* my_file_pointer;
const int BUFFER_SIZE = 1048575;
const int NUM_CHARS = 66;
//Maximum Height or Width of a Character
const int MAX_DIMENSION = 64;
uint32 myfd;

int bf_open(char* filename)
{
	filesize = 0;
	myfd = iso_open(filename, O_RDONLY);
	//Read up to 1 meg. of file
	if (myfd >= 0)
	{
		filesize = iso_read(myfd, (void *) my_cd_buffer, BUFFER_SIZE );
		my_file_pointer = my_cd_buffer;
		return 1;
	}
	else
		return 0;
}

void bf_close()
{
	iso_close(myfd);
	my_cd_buffer = NULL;
	my_file_pointer = NULL;
}

uint16 bf_read16()
{
	uint16 tempval;
	tempval = (((uint8*) my_file_pointer)[0]);
	my_file_pointer += 2;
	return tempval;
}

BMF_Character* bf_char_ptr(BMF_Character* font, char str)
{
	BMF_Character* chartodraw;
	switch ((int) str)
	{
		case 65:
			chartodraw = &font[0];
			break;

		case 66:
			chartodraw = &font[1];
			break;

		case 67:
			chartodraw = &font[2];
			break;

		case 68:
			chartodraw = &font[3];
			break;

		case 69:
			chartodraw = &font[4];
		    break;

		case 70:
			chartodraw = &font[5];
			break;

		case 71:
			chartodraw = &font[6];
			break;

		case 72:
			chartodraw = &font[7];
			break;

		case 73:
			chartodraw = &font[8];
			break;

		case 74:
			chartodraw = &font[9];
			break;

		case 75:
			chartodraw = &font[10];
			break;

		case 76:
			chartodraw = &font[11];
			break;

		case 77:
			chartodraw = &font[12];
			break;

		case 78:
			chartodraw = &font[13];
			break;

		case 79:
			chartodraw = &font[14];
			break;

		case 80:
			chartodraw = &font[15];
			break;

		case 81:
			chartodraw = &font[16];
			break;

		case 82:
			chartodraw = &font[17];
			break;

		case 83:
			chartodraw = &font[18];
			break;

		case 84:
			chartodraw = &font[19];
			break;

		case 85:
			chartodraw = &font[20];
			break;

		case 86:
			chartodraw = &font[21];
			break;

		case 87:
			chartodraw = &font[22];
			break;

		case 88:
			chartodraw = &font[23];
			break;

		case 89:
			chartodraw = &font[24];
			break;

		case 90:
			chartodraw = &font[25];
			break;

		case 97:
			chartodraw = &font[26];
			break;

		case 98:
			chartodraw = &font[27];
			break;

		case 99:
			chartodraw = &font[28];
			break;

		case 100:
			chartodraw = &font[29];
			break;

		case 101:
			chartodraw = &font[30];
			break;

		case 102:
			chartodraw = &font[31];
			break;

		case 103:
			chartodraw = &font[32];
			break;

		case 104:
			chartodraw = &font[33];
			break;

		case 105:
			chartodraw = &font[34];
			break;

		case 106:
			chartodraw = &font[35];
			break;

		case 107:
			chartodraw = &font[36];
			break;

		case 108:
			chartodraw = &font[37];
			break;

		case 109:
			chartodraw = &font[38];
			break;

		case 110:
			chartodraw = &font[39];
			break;

		case 111:
			chartodraw = &font[40];
			break;

		case 112:
			chartodraw = &font[41];
			break;

		case 113:
			chartodraw = &font[42];
			break;

		case 114:
			chartodraw = &font[43];
			break;

		case 115:
			chartodraw = &font[44];
			break;

		case 116:
			chartodraw = &font[45];
			break;

		case 117:
			chartodraw = &font[46];
			break;

		case 118:
			chartodraw = &font[47];
			break;

		case 119:
			chartodraw = &font[48];
			break;

		case 120:
			chartodraw = &font[49];
			break;

		case 121:
			chartodraw = &font[50];
			break;

		case 122:
			chartodraw = &font[51];
			break;

		case 48:
			chartodraw = &font[52];
			break;

		case 49:
			chartodraw = &font[53];
			break;

		case 50:
			chartodraw = &font[54];
			break;
		
		case 51:
			chartodraw = &font[55];
			break;

		case 52:
			chartodraw = &font[56];
			break;

		case 53:
			chartodraw = &font[57];
			break;

		case 54:
			chartodraw = &font[58];
			break;

		case 55:
			chartodraw = &font[59];
			break;

		case 56:
			chartodraw = &font[60];
			break;

		case 57:
			chartodraw = &font[61];
			break;

		case 46:
			chartodraw = &font[62];
			break;

		case 44:
			chartodraw = &font[63];
			break;

		case 33:
			chartodraw = &font[64];
			break;

		case 63:
			chartodraw = &font[65];
			break;

		default:
			chartodraw = &font[27];
			break;
	}
	return chartodraw;
}

void bf_read_bitdata(int numbytes, unsigned char* buffer)
{
	int i;
	for (i = 0; i < numbytes; i++)
	{
		buffer[i] = my_file_pointer[i];
	}
	my_file_pointer += numbytes;
}

int bf_str_width(BMF_Character* font, char* str)
{
	int temp;
	int i;
	BMF_Character* tempchar;
	
	temp = 0;
	for (i = 0; i < strlen(str); i++)
	{
		tempchar = bf_char_ptr(font, str[i]);
		temp += tempchar -> width;
	}
	return temp;
}

int bf_str_height(BMF_Character* font, char* str)
{
	BMF_Character* tempchar;
	tempchar = bf_char_ptr(font, str[0]);
	return tempchar -> height;
}

int bf_load_file(char* filename, BMF_Character* mychars)
{
	unsigned char cd_buffer[BUFFER_SIZE];
	int i;
	int notperfect;

	my_cd_buffer = &cd_buffer[0];

	notperfect = 0;

	if (bf_open(filename) == 1)
	{
		if (((char*) my_file_pointer)[0] == 'B')
		{
			i = 0;
			my_file_pointer += 6;

			while (i < NUM_CHARS)
			{
				mychars[i].width = bf_read16();
				mychars[i].height = bf_read16();

				if ((mychars[i].height > MAX_DIMENSION) || (mychars[i].width > MAX_DIMENSION))
				{
					mychars[i].bitdata = NULL;
					notperfect = 1;
					my_file_pointer += (mychars[i].width * mychars[i].height);
				}
				else
				{
					mychars[i].bitdata = malloc(mychars[i].width * mychars[i].height);
					bf_read_bitdata((mychars[i].width * mychars[i].height), &mychars[i].bitdata[0]);
				}

				i++;
			}

			bf_close();

			if (notperfect)
				//Some Invalid Dimension Was Found
				return 4;
			else
				//Otherwise Good Load
				return 1;
		}
		else
		{
			//Not a BMF File
			bf_close();
			return 2;
		}
	}
	else
		//File Not Found
		return 0;
}

void bf_free_font(BMF_Character* font)
{
	int i;

	for (i = 0; i < NUM_CHARS; i++)
		free(font[i].bitdata);
}

void bf_draw_str(BMF_Character* font, uint16 color, uint16* buffer, int bufwidth, char* str)
{
	int i;
	int j;
	int k;
	int xoff;
	BMF_Character* chartodraw;

	xoff = 0;

	for (i = 0; i < strlen(str); i++)
	{
		chartodraw = bf_char_ptr(font, str[i]);
		if (chartodraw -> bitdata != NULL)
		{
			if ((chartodraw == &font[27]) && (str[i] != (char) 98))
			{
				xoff += chartodraw -> width;
			}
			else
			{
				for (k = 0; k < (chartodraw -> height); k++)
					for (j = 0; j < (chartodraw -> width); j++)
					{
						if (chartodraw -> bitdata[j + (k * chartodraw -> width)] == 1)
							buffer[j + xoff + (k * bufwidth)] = color;
					}
				xoff += chartodraw -> width;
			}
		}
	}
}

void bf_bgdraw_str(BMF_Character* font, uint16 color, uint16 bgcolor, uint16* buffer, int bufwidth, char* str)
{
	int i;
	int j;
	int k;
	int xoff;
	BMF_Character* chartodraw;

	xoff = 0;

	for (i = 0; i < strlen(str); i++)
	{
		chartodraw = bf_char_ptr(font, str[i]);
		if (chartodraw -> bitdata != NULL)
		{
			if ((chartodraw == &font[27]) && (str[i] != (char) 98))
			{
				for (k = 0; k < (chartodraw -> height); k++)
					for (j = 0; j < (chartodraw -> width); j++)
					{
						buffer[j + xoff + (k * bufwidth)] = bgcolor;
					}			
				xoff += chartodraw -> width;
			}
			else
			{
				for (k = 0; k < (chartodraw -> height); k++)
					for (j = 0; j < (chartodraw -> width); j++)
					{
						if (chartodraw -> bitdata[j + (k * chartodraw -> width)] == 1)
							buffer[j + xoff + (k * bufwidth)] = color;
						else
							buffer[j + xoff + (k * bufwidth)] = bgcolor;
					}
				xoff += chartodraw -> width;
			}
		}
	}
}

void bf_draw_str_widthclip(BMF_Character* font, uint16 color, uint16* buffer, int bufwidth, char* str, int maxwidth)
{
	int i;
	int j;
	int k;
	int xoff;
	BMF_Character* chartodraw;

	xoff = 0;

	for (i = 0; ((i < strlen(str)) && (xoff < maxwidth)); i++)
	{
		chartodraw = bf_char_ptr(font, str[i]);
		if (chartodraw -> bitdata != NULL)
		{
			if ((chartodraw == &font[27]) && (str[i] != (char) 98))
			{
				xoff += chartodraw -> width;
			}
			else
			{
				for (k = 0; k < (chartodraw -> height); k++)
					for (j = 0; j < (chartodraw -> width); j++)
					{
						if (chartodraw -> bitdata[j + (k * chartodraw -> width)] == 1)
							buffer[j + xoff + (k * bufwidth)] = color;
					}
				xoff += chartodraw -> width;
			}
		}
	}
}

void bf_bgdraw_str_widthclip(BMF_Character* font, uint16 color, uint16 bgcolor, uint16* buffer, int bufwidth, char* str, int maxwidth)
{
	int i;
	int j;
	int k;
	int xoff;
	BMF_Character* chartodraw;

	xoff = 0;

	for (i = 0; ((i < strlen(str)) && (xoff < maxwidth)); i++)
	{
		chartodraw = bf_char_ptr(font, str[i]);
		if (chartodraw -> bitdata != NULL)
		{
			if ((chartodraw == &font[27]) && (str[i] != (char) 98))
			{
				for (k = 0; k < (chartodraw -> height); k++)
					for (j = 0; j < (chartodraw -> width); j++)
					{
						buffer[j + xoff + (k * bufwidth)] = bgcolor;
					}			
				xoff += chartodraw -> width;
			}
			else
			{
				for (k = 0; k < (chartodraw -> height); k++)
					for (j = 0; j < (chartodraw -> width); j++)
					{
						if (chartodraw -> bitdata[j + (k * chartodraw -> width)] == 1)
							buffer[j + xoff + (k * bufwidth)] = color;
						else
							buffer[j + xoff + (k * bufwidth)] = bgcolor;
					}
				xoff += chartodraw -> width;
			}
		}
	}
	while (xoff < maxwidth)
	{
		for (j = 0; j < chartodraw -> height; j++)
		{
			buffer[(j * bufwidth)+ xoff] = bgcolor;
		}
		xoff++;
	}
}

void bf_draw_str_rlalign(BMF_Character* font, uint16 color, uint16* buffer, int bufwidth, char* str, int maxwidth)
{
	int i;
	int j;
	int k;
	int xoff;
	int midoff;
	BMF_Character* chartodraw;
	int S2Index;
	int S2EndIndex;
	char tbuffer[256];

	for (i = 0; (i < strlen(str)) && (str[i] != ',');  i++);
	if (str[i] == ',')
		S2Index = i+1;
	else
		return;

	for (i = 0; (i < strlen(str)) && (str[i] != '<');  i++);
	if (str[i] == '<')
		S2EndIndex = i-1;
	else
		return;

	for (i = S2Index; i <= S2EndIndex; i++)
		tbuffer[i - S2Index] = str[i];
	tbuffer[i - S2Index] = (char)NULL;

	xoff = 0;

	midoff = bf_str_width(font, tbuffer);

	for (i = 0; (i < (S2Index - 1)); i++)
	{
		chartodraw = bf_char_ptr(font, str[i]);
		if (chartodraw -> bitdata != NULL)
		{
			if ((chartodraw == &font[27]) && (str[i] != (char) 98))
			{
				xoff += chartodraw -> width;
			}
			else
			{
				for (k = 0; k < (chartodraw -> height); k++)
					for (j = 0; j < (chartodraw -> width); j++)
					{
						if (chartodraw -> bitdata[j + (k * chartodraw -> width)] == 1)
							buffer[j + xoff + (k * bufwidth)] = color;
					}
				xoff += chartodraw -> width;
			}
		}
	}

	while (xoff < (maxwidth - midoff))
	{
		xoff++;
	}

	for (i = S2Index; (i <= S2EndIndex); i++)
	{
		chartodraw = bf_char_ptr(font, str[i]);
		if (chartodraw -> bitdata != NULL)
		{
			if ((chartodraw == &font[27]) && (str[i] != (char) 98))
			{
				xoff += chartodraw -> width;
			}
			else
			{
				for (k = 0; k < (chartodraw -> height); k++)
					for (j = 0; j < (chartodraw -> width); j++)
					{
						if (chartodraw -> bitdata[j + (k * chartodraw -> width)] == 1)
							buffer[j + xoff + (k * bufwidth)] = color;
					}
				xoff += chartodraw -> width;
			}
		}
	}

	while (xoff < maxwidth)
	{
		xoff++;
	}
}

void bf_bgdraw_str_rlalign(BMF_Character* font, uint16 color, uint16 bgcolor, uint16* buffer, int bufwidth, char* str, int maxwidth)
{
	int i;
	int j;
	int k;
	int xoff;
	int midoff;
	BMF_Character* chartodraw;
	int S2Index;
	int S2EndIndex;
	char tbuffer[256];

	for (i = 0; (i < strlen(str)) && (str[i] != ',');  i++);
	if (str[i] == ',')
		S2Index = i+1;
	else
		return;

	for (i = 0; (i < strlen(str)) && (str[i] != '<');  i++);
	if (str[i] == '<')
		S2EndIndex = i - 1;
	else
		return;

	for (i = S2Index; i <= S2EndIndex; i++)
		tbuffer[i - S2Index] = str[i];
	tbuffer[i - S2Index] = (char)NULL;

	xoff = 0;

	midoff = bf_str_width(font, tbuffer);

	for (i = 0; (i < (S2Index - 1)); i++)
	{
		chartodraw = bf_char_ptr(font, str[i]);
		if (chartodraw -> bitdata != NULL)
		{
			if ((chartodraw == &font[27]) && (str[i] != (char) 98))
			{
				for (k = 0; k < (chartodraw -> height); k++)
					for (j = 0; j < (chartodraw -> width); j++)
					{
						buffer[j + xoff + (k * bufwidth)] = bgcolor;
					}			
				xoff += chartodraw -> width;
			}
			else
			{
				for (k = 0; k < (chartodraw -> height); k++)
					for (j = 0; j < (chartodraw -> width); j++)
					{
						if (chartodraw -> bitdata[j + (k * chartodraw -> width)] == 1)
							buffer[j + xoff + (k * bufwidth)] = color;
						else
							buffer[j + xoff + (k * bufwidth)] = bgcolor;
					}
				xoff += chartodraw -> width;
			}
		}
	}

	while (xoff < (maxwidth - midoff))
	{
		for (j = 0; j < chartodraw -> height; j++)
		{
			buffer[(j * bufwidth)+ xoff] = bgcolor;
		}
		xoff++;
	}

	for (i = S2Index; (i <= S2EndIndex); i++)
	{
		chartodraw = bf_char_ptr(font, str[i]);
		if (chartodraw -> bitdata != NULL)
		{
			if ((chartodraw == &font[27]) && (str[i] != (char) 98))
			{
				for (k = 0; k < (chartodraw -> height); k++)
					for (j = 0; j < (chartodraw -> width); j++)
					{
						buffer[j + xoff + (k * bufwidth)] = bgcolor;
					}			
				xoff += chartodraw -> width;
			}
			else
			{
				for (k = 0; k < (chartodraw -> height); k++)
					for (j = 0; j < (chartodraw -> width); j++)
					{
						if (chartodraw -> bitdata[j + (k * chartodraw -> width)] == 1)
							buffer[j + xoff + (k * bufwidth)] = color;
						else
							buffer[j + xoff + (k * bufwidth)] = bgcolor;
					}
				xoff += chartodraw -> width;
			}
		}
	}

	while (xoff < maxwidth)
	{
		for (j = 0; j < chartodraw -> height; j++)
		{
			buffer[(j * bufwidth)+ xoff] = bgcolor;
		}
		xoff++;
	}
}

