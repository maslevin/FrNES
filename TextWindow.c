/* 
File:        TextWindow.c
Project:     Platform Development Tools
Date:        December. 18th, 2000
Programmer:  ReGex
Description: A set of functions to draw a text window suitable for a GUI
Updated:     December 7th, 2001 - Added Scoll Bar Control to code
*/

#include "TextWindow.h"
#include "pNesX_Utils.h"

//Corner of the window - Gets automatically flipped and whatever
const unsigned char WindowCorner[49] = 
{
0, 0, 1, 1, 1, 1, 1,
0, 1, 2, 2, 2, 2, 2,
1, 2, 2, 2, 2, 2, 2,
1, 2, 2, 2, 2, 2, 2,
1, 2, 2, 2, 2, 1, 1,
1, 2, 2, 2, 1, 1, 3,
1, 2, 2, 2, 1, 3, 3
};

//Checkbox, 16 x 16
const unsigned char CheckBoxOpen[256] =
{
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};


//Checkbox, 16 x 16
const unsigned char CheckBoxClosed[256] =
{
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1,
1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1,
1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1,
1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1,
1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

//Sliderbar slider, 16 x 16
const unsigned char Slider[] =
{
0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 
0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 
0, 0, 1, 1, 2, 1, 0, 0, 0, 0, 1, 2, 1, 1, 0, 0, 
0, 0, 1, 2, 2, 1, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 
0, 0, 1, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 1, 0, 0, 
0, 0, 1, 2, 2, 1, 2, 2, 2, 2, 1, 2, 2, 1, 0, 0, 
0, 0, 1, 2, 2, 1, 2, 2, 2, 2, 1, 2, 2, 1, 0, 0, 
0, 0, 1, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 1, 0, 0, 
0, 0, 1, 2, 2, 1, 2, 2, 2, 2, 1, 2, 2, 1, 0, 0, 
0, 0, 1, 2, 2, 1, 2, 2, 2, 2, 1, 2, 2, 1, 0, 0, 
0, 0, 1, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 1, 0, 0, 
0, 0, 1, 2, 2, 1, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 
0, 0, 1, 2, 2, 1, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 
0, 0, 1, 1, 2, 1, 1, 1, 1, 1, 1, 2, 1, 1, 0, 0, 
0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 
0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0
};

//Sliderbar element, 6 x 7
const unsigned char SliderSide[] = 
{
0, 0, 1, 1, 1, 1,
0, 1, 2, 2, 2, 2,
1, 2, 2, 2, 2, 2,
1, 1, 1, 1, 1, 1,
1, 2, 2, 2, 2, 2,
0, 1, 2, 2, 2, 2,
0, 0, 1, 1, 1, 1
};

//gradient symbol 8 x 16
const unsigned char GradSymbol[] = 
{
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 
0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 
0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 
0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 
0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
};

//To draw a checkbox, pass a string to the textbox in the format
const char Tag_CheckBox[] = "<Check>";
const char CHECKBOX_CHECKED[] = "CBC_CHECKED";
const char CHECKBOX_UNCHECKED[] = "CBC_UNCHECKED";

//To draw a gradient symbol, pass a string to the textbox with the format
const char Tag_Gradient[] = "<Gradient>";

//To right and left align
const char Tag_RLAlign[] = "<RLAlign>";

//To draw a slider box, pass a string to the textbox with the format
const char Tag_Slider[] = "<Slider>";
const char SLIDERBOX_MAX[] = "<SBC_MAX = ";
const char SLIDERBOX_MIN[] = "<SBC_MIN = ";
const char SLIDERBOX_VALUE[] = "<SBC_VALUE = ";

void win_draw_scrollbar(uint16 bordercolor, uint16 insidecolor, uint16 bgcolor, uint16* render_area, uint16 render_width, int maxwidth, int max, int min, int value, int highlighted)
{
	int i;
	int j;
	int startsliderx;
	int endsliderx;
	int midsliderx;
	uint16 color;

	startsliderx = 16;
	endsliderx = maxwidth - 16;
	midsliderx = (int)((float)(startsliderx + endsliderx) * ((float)value / (max - min)));

	//Draw left block
	for (i = 0; i < 6; i++)
	{
		if (highlighted)
			for (j = 0; j < 5; j++)
				render_area[((j * render_width) + i)] = bgcolor;

		for (j = 0; j < 7; j++)
		{
			switch (SliderSide[(j * 6) + i])
			{
				case 2:
					render_area[(((j + 5) * render_width) + i)] = insidecolor;
					break;
				case 1:
					render_area[(((j + 5) * render_width) + i)] = bordercolor;
					break;
				case 0:
				default:
					if (highlighted)
						render_area[(((j + 5) * render_width) + i)] = bgcolor;
			}
		}
		if (highlighted)
			for (j = 0; j < 4; j++)
				render_area[(((j + 12) * render_width) + i)] = bgcolor;
	}

	//Draw right block
	for (i = 0; i < 6; i++)
	{
		if (highlighted)
			for (j = 0; j < 5; j++)
				render_area[maxwidth - 6 + ((j * render_width) + i)] = bgcolor;
		for (j = 0; j < 7; j++)
		{
			switch (SliderSide[(j * 6) + (6 - i)])
			{
				case 2:
					render_area[maxwidth - 6 + (((j + 5) * render_width) + i)] = insidecolor;
					break;
				case 1:
					render_area[maxwidth - 6 + (((j + 5) * render_width) + i)] = bordercolor;
					break;
				case 0:
				default:
					if (highlighted)
						render_area[maxwidth - 6 + (((j + 5) * render_width) + i)] = bgcolor;
					break;					
			}
		}
		if (highlighted)
			for (j = 0; j < 4; j++)
				render_area[maxwidth - 6 + (((j + 12) * render_width) + i)] = bgcolor;
	}

	if (highlighted)
		for (j = 0; j < 5; j++)
			for (i = 6; i < maxwidth - 6; i++)
				render_area[((j * render_width) + i)] = bgcolor;			

	//Draw the bar
	for (j = 0; j < 7; j++)
	{
		switch (SliderSide[(6 * j) + 5])
		{
			case 2:
				color = insidecolor;
				break;
			case 1:
				color = bordercolor;
				break;
			case 0:
			default:
				if (highlighted)
					color = bgcolor;
				else 
					color = insidecolor;
				break;
		}

		for (i = 6; i < maxwidth - 6; i++)
			render_area[(((j + 5) * render_width) + i)] = color;
	}

	if (highlighted)
		for (j = 0; j < 4; j++)
			for (i = 6; i < maxwidth - 6; i++)
				render_area[(((j + 12) * render_width) + i)] = bgcolor;			

	//Draw the slider
	for (j = 0; j < 16; j++)
		for (i = 0; i < 16; i++)
		{
			switch (Slider[(j*16) + i])
			{
				case 2:
					render_area[(j * render_width) + (midsliderx - 8) + i] = insidecolor;
					break;
				case 1:
					render_area[(j * render_width) + (midsliderx - 8) + i] = bordercolor;
					break;
				case 0:
				default:
					if (highlighted)
						render_area[(j * render_width) + (midsliderx - 8) + i] = bgcolor;
					break;
			}
		}
}

void win_draw_checktext(BMF_Character* font, uint16 textcolor, uint16 bgcolor, uint16* render_area, uint16 render_width, char* str, int maxwidth, int check, int highlighted)
{
	int i;
	int j;

	if (!(highlighted))
		bf_draw_str_widthclip(font, textcolor, render_area, render_width, str, maxwidth - 20);
	else
		bf_bgdraw_str_widthclip(font, textcolor, bgcolor, render_area, render_width, str, maxwidth - 20);

	for (i = 0; i < 16; i++)
		for (j = 0; j < 16; j++)
			if (check)
			{
				if (CheckBoxClosed[i + j * 16])
					render_area[maxwidth - 20 + i + j * render_width] = textcolor;
				else
					render_area[maxwidth - 20 + i + j * render_width] = bgcolor;
			}
			else
			{
				if (CheckBoxOpen[i + j * 16])
					render_area[maxwidth - 20 + i + j * render_width] = textcolor;
				else
					render_area[maxwidth - 20 + i + j * render_width] = bgcolor;					
			}

	for (j = 0; j < 16; j++)
		for (i = 0; i < 4; i++)
			render_area[(j * render_width) + maxwidth - 4 + i] = bgcolor;
}

void win_draw_textwindow (Window_Data* windata, Window_Style* winstyle)
{
	int i;
	int j;
	uint16 tempcolor;
	int tempscanline;
	int yposition;
	int xposition;
	char tempbuffer[255];
	char* tempptr;
	int index;
	int startindex;

	//Draw the window itself
	for (j = 0; j < windata -> height; j++)
	{
		if (j < 7)
		//Draw one of the Header Scanlines
		{
			//Draw part of the topleft corner
			for (i = 0; i < 7; i++)
			{
				switch (WindowCorner[i + (j * 7)])
				{
					case 0:
						windata -> Target_Buffer[windata -> x + i + ((windata -> y + j) * windata -> Target_Width)] = 0x0000;
						break;
					case 1:
						windata -> Target_Buffer[windata -> x + i + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Border_Outside_Color;
						break;
					case 2:
						windata -> Target_Buffer[windata -> x + i + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Border_Inside_Color;
						break;
					case 3:
						windata -> Target_Buffer[windata -> x + i + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Inside_Color;
						break;
					default:
						break;
				}
			}

			//Determine the color of the scanline excluding the corner
			switch (j)
			{
				case 0:
				case 4:
					tempcolor = winstyle -> Border_Outside_Color;
					break;
				case 1:
				case 2:
				case 3:
					tempcolor = winstyle -> Border_Inside_Color;
					break;
				default:
					tempcolor = winstyle -> Inside_Color;
					break;
			}

			//Draw the new scanline
			for (i = 7; i < (windata -> width - 14); i++)
				windata -> Target_Buffer[windata -> x + i + ((windata -> y + j) * windata -> Target_Width)] = tempcolor;
			
			//Draw part of the topright corner
			for (i = 0; i < 7; i++)
			{
				switch (WindowCorner[i + (j * 7)])
				{
					case 0:
						windata -> Target_Buffer[windata -> x + windata -> width - 14 + (6 - i) + ((windata -> y + j) * windata -> Target_Width)] = 0x0000;
						break;
					case 1:
						windata -> Target_Buffer[windata -> x + windata -> width - 14 + (6 - i) + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Border_Outside_Color;
						break;
					case 2:
						windata -> Target_Buffer[windata -> x + windata -> width - 14 + (6 - i) + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Border_Inside_Color;
						break;
					case 3:
						windata -> Target_Buffer[windata -> x + windata -> width - 14 + (6 - i) + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Inside_Color;
						break;
					default:
						break;
				}
			}

		}
		//Draw one of the Footer Scanlines
		else if ((windata -> height - j) <= 7)
		{
			//tempscanline is the index of the scanline in the constant buffer
			tempscanline = (windata -> height - j) - 1;
			//Draw part of the bottomleft corner
			for (i = 0; i < 7; i++)
			{
				switch (WindowCorner[i + (tempscanline * 7)])
				{
					case 0:
						windata -> Target_Buffer[windata -> x + i + ((windata -> y + j) * windata -> Target_Width)] = 0x0000;
						break;
					case 1:
						windata -> Target_Buffer[windata -> x + i + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Border_Outside_Color;
						break;
					case 2:
						windata -> Target_Buffer[windata -> x + i + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Border_Inside_Color;
						break;
					case 3:
						windata -> Target_Buffer[windata -> x + i + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Inside_Color;
						break;
					default:
						break;
				}
			}

			//Determine the color of the scanline excluding the corner
			switch (tempscanline)
			{
				case 0:
				case 4:
					tempcolor = winstyle -> Border_Outside_Color;
					break;
				case 1:
				case 2:
				case 3:
					tempcolor = winstyle -> Border_Inside_Color;
					break;
				default:
					tempcolor = winstyle -> Inside_Color;
					break;
			}

			//Draw the new scanline
			for (i = 7; i < (windata -> width - 14); i++)
				windata -> Target_Buffer[windata -> x + i + ((windata -> y + j) * windata -> Target_Width)] = tempcolor;
			
			//Draw part of the bottomright corner
			for (i = 0; i < 7; i++)
			{
				switch (WindowCorner[i + (7 * tempscanline)])
				{
					case 0:
						windata -> Target_Buffer[windata -> x + windata -> width - 14 + (6 - i) + ((windata -> y + j) * windata -> Target_Width)] = 0x0000;
						break;
					case 1:
						windata -> Target_Buffer[windata -> x + windata -> width - 14 + (6 - i) + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Border_Outside_Color;
						break;
					case 2:
						windata -> Target_Buffer[windata -> x + windata -> width - 14 + (6 - i) + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Border_Inside_Color;
						break;
					case 3:
						windata -> Target_Buffer[windata -> x + windata -> width - 14 + (6 - i) + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Inside_Color;
						break;
					default:
						break;
				}
			}
		}
		//Draw a regular Scanline
		else 
		{
			for (i = 0; i < 7; i++)
			{
				switch (WindowCorner[i + 42])
				{
					case 0:
						windata -> Target_Buffer[windata -> x + i + ((windata -> y + j) * windata -> Target_Width)] = 0x0000;
						break;
					case 1:
						windata -> Target_Buffer[windata -> x + i + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Border_Outside_Color;
						break;
					case 2:
						windata -> Target_Buffer[windata -> x + i + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Border_Inside_Color;
						break;
					case 3:
						windata -> Target_Buffer[windata -> x + i + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Inside_Color;
						break;
					default:
						break;
				}
			}

			for (i = 7; i < (windata -> width - 14); i++)
				windata -> Target_Buffer[windata -> x + i + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Inside_Color;

			for (i = 0; i < 7; i++)
			{
				switch (WindowCorner[i + 42])
				{
					case 0:
						windata -> Target_Buffer[windata -> x + windata -> width - 14 + (6 - i) + ((windata -> y + j) * windata -> Target_Width)] = 0x0000;
						break;
					case 1:
						windata -> Target_Buffer[windata -> x + windata -> width - 14 + (6 - i) + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Border_Outside_Color;
						break;
					case 2:
						windata -> Target_Buffer[windata -> x + windata -> width - 14 + (6 - i) + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Border_Inside_Color;
						break;
					case 3:
						windata -> Target_Buffer[windata -> x + windata -> width - 14 + (6 - i) + ((windata -> y + j) * windata -> Target_Width)] = winstyle -> Inside_Color;
						break;
					default:
						break;
				}
			}
		}
	}

	//Draw the Header Text in the Header Font
	yposition = 15;
	xposition = winstyle -> Left_Margin;
	bf_draw_str(windata -> Header_Font, winstyle -> Header_Text_Color, windata -> Target_Buffer + windata -> x + (xposition) + ((windata -> y + yposition) * windata -> Target_Width), windata -> Target_Width, windata -> Header_Text);
	yposition += bf_str_height(windata -> Header_Font, "A");

	//Now Draw the Text In the Window
	for (i = 0; (i < winstyle -> Max_Items) && (i < windata -> Num_Strings); i++)
	{
		//But if the tag was found, pass it to the Checkbox Control Module
		if (strstr((windata -> Data_Strings)[i + windata -> Top_Index], Tag_CheckBox) != NULL)
		{
			tempptr = (windata -> Data_Strings)[i + windata -> Top_Index];
			//Parse the text field
			index = 0;
		    while (tempptr[index] != '>')
				index++;
			index++;
			startindex = index;
			//Tempbuffer will contain the message text
			while (tempptr[index] != ',')
			{
				tempbuffer[index - startindex] = tempptr[index];
				index++;
			}
			tempbuffer[index - startindex] = (char)NULL;

			//If checked was found
			if (strstr(tempptr, CHECKBOX_CHECKED) != NULL)
				index = 1;
			else if (strstr(tempptr, CHECKBOX_UNCHECKED) != NULL)
				index = 0;
			else
				index = 0;

			if (i + windata -> Top_Index != windata -> Highlighted_Index)
			{
				win_draw_checktext(windata -> Item_Font, winstyle -> Text_Color, winstyle -> Inside_Color, windata -> Target_Buffer + windata -> x + (xposition) + ((windata -> y + yposition) * windata -> Target_Width), windata -> Target_Width, tempbuffer, windata -> width - 50, index, 0);
			}
			else
			{
				win_draw_checktext(windata -> Item_Font, winstyle -> Selected_Text_Color, winstyle -> Selected_Background_Color, windata -> Target_Buffer + windata -> x + (xposition) + ((windata -> y + yposition) * windata -> Target_Width), windata -> Target_Width, tempbuffer, windata -> width - 50, index, 1);
			}

			yposition += bf_str_height(windata -> Item_Font, "A");
		}
		//Check for the Slider Tag
		else
		if (strstr((windata -> Data_Strings)[i + windata -> Top_Index], Tag_Slider) != NULL)
		{
			int max;
			int min;
			int value;
			char* substrval;
			char numbuffer[255];

			tempptr = (windata -> Data_Strings)[i + windata -> Top_Index];

			//Parse to find the Max value for the sliderbox
			max = 0;
			substrval = strstr(tempptr, SLIDERBOX_MAX);
			if (substrval != NULL)
			{
				index = 0;
				substrval += strlen(SLIDERBOX_MAX);
				while (substrval[index] != '>')
				{
					numbuffer[index] = substrval[index];
					index++;
				}
				numbuffer[index] = (char)NULL;
				max = pNesX_atoi(numbuffer);
			}

			//Parse to find the Min value for the sliderbox
			min = 0;
			substrval = strstr(tempptr, SLIDERBOX_MIN);
			if (substrval != NULL)
			{
				index = 0;
				substrval += strlen(SLIDERBOX_MIN);
				while (substrval[index] != '>')
				{
					numbuffer[index] = substrval[index];
					index++;
				}
				numbuffer[index] = (char)NULL;
				min = pNesX_atoi(numbuffer);
			}


			//Parse to find the Min value for the sliderbox
			value = 0;
			substrval = strstr(tempptr, SLIDERBOX_VALUE);
			if (substrval != NULL)
			{
				index = 0;
				substrval += strlen(SLIDERBOX_VALUE);
				while (substrval[index] != '>')
				{
					numbuffer[index] = substrval[index];
					index++;
				}
				numbuffer[index] = (char)NULL;
				value = pNesX_atoi(numbuffer);
			}

			if (i + windata -> Top_Index != windata -> Highlighted_Index)
			{
				win_draw_scrollbar(winstyle -> Text_Color, winstyle -> Inside_Color, winstyle -> Selected_Background_Color, windata -> Target_Buffer + windata -> x + (xposition) + ((windata -> y + yposition) * windata -> Target_Width), windata -> Target_Width, windata -> width - 50, max, min, value, 0);
			}
			else
			{
				win_draw_scrollbar(winstyle -> Text_Color, winstyle -> Inside_Color, winstyle -> Selected_Background_Color, windata -> Target_Buffer + windata -> x + (xposition) + ((windata -> y + yposition) * windata -> Target_Width), windata -> Target_Width, windata -> width - 50, max, min, value, 1);
			}

			yposition += bf_str_height(windata -> Item_Font, "A");

		}
		//If the Checkbox Tag wasn't found in the string -> Bitmap it like normal
		else
		if (strstr((windata -> Data_Strings)[i + windata -> Top_Index], Tag_RLAlign) != NULL)
		{
			if (i + windata -> Top_Index != windata -> Highlighted_Index)
			{
				bf_draw_str_rlalign(windata -> Item_Font, winstyle -> Text_Color, windata -> Target_Buffer + windata -> x + (xposition) + ((windata -> y + yposition) * windata -> Target_Width), windata -> Target_Width, (windata -> Data_Strings)[i + windata -> Top_Index], windata -> width - 50);
			}
			else
			{
				bf_bgdraw_str_rlalign(windata -> Item_Font, winstyle -> Selected_Text_Color, winstyle -> Selected_Background_Color, windata -> Target_Buffer + windata -> x + (xposition) + ((windata -> y + yposition) * windata -> Target_Width), windata -> Target_Width, (windata -> Data_Strings)[i + windata -> Top_Index], windata -> width - 50);
			}
			yposition += bf_str_height(windata -> Item_Font, "A");
		}
		else
		{
			if (i + windata -> Top_Index != windata -> Highlighted_Index)
			{
				bf_draw_str_widthclip(windata -> Item_Font, winstyle -> Text_Color, windata -> Target_Buffer + windata -> x + (xposition) + ((windata -> y + yposition) * windata -> Target_Width), windata -> Target_Width, (windata -> Data_Strings)[i + windata -> Top_Index], windata -> width - 50);
			}
			else
			{
				bf_bgdraw_str_widthclip(windata -> Item_Font, winstyle -> Selected_Text_Color, winstyle -> Selected_Background_Color, windata -> Target_Buffer + windata -> x + (xposition) + ((windata -> y + yposition) * windata -> Target_Width), windata -> Target_Width, (windata -> Data_Strings)[i + windata -> Top_Index], windata -> width - 50);
			}
			yposition += bf_str_height(windata -> Item_Font, "A");
		}
	}
}
