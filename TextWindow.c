/* 
File:        TextWindow.c
Project:     Platform Development Tools
Date:        December. 18th, 2000
Programmer:  ReGex
Description: A set of functions to draw a text window suitable for a GUI
Updated:     December 7th, 2001 - Added Scoll Bar Control to code
*/

#include "TextWindow.h"

#include <math.h>
#include "macros.h"

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

/*
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
*/

void draw_cornered_layer(uint32 list, float xPos, float yPos, float zPos, float width, float height, float corner_radius, uint32 color) {
	pvr_poly_hdr_t hdr;
	pvr_poly_cxt_t cxt;
	pvr_vertex_t my_vertex;		

	pvr_poly_cxt_col(&cxt, list);
	pvr_poly_compile(&hdr, &cxt);
	pvr_prim(&hdr, sizeof(hdr));

	//Draw central rect
	my_vertex.flags = PVR_CMD_VERTEX;
	my_vertex.x = xPos + corner_radius;
	my_vertex.y = yPos + height;
	my_vertex.z = zPos;
	my_vertex.argb = color;
	my_vertex.oargb = 0;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.y = yPos;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.x = xPos + width - corner_radius;
	my_vertex.y = yPos + height;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = PVR_CMD_VERTEX_EOL;
	my_vertex.y = yPos;
	pvr_prim(&my_vertex, sizeof(my_vertex));				

	//Draw left rect
	my_vertex.flags = PVR_CMD_VERTEX;
	my_vertex.x = xPos;
	my_vertex.y = yPos + height - corner_radius;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.y = yPos + corner_radius;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.x = xPos + corner_radius;
	my_vertex.y = yPos + height - corner_radius;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = PVR_CMD_VERTEX_EOL;
	my_vertex.y = yPos + corner_radius;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	//Draw right rect						
	my_vertex.flags = PVR_CMD_VERTEX;
	my_vertex.x = xPos + width - corner_radius;
	my_vertex.y = yPos + height - corner_radius;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.y = yPos + corner_radius;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.x = xPos + width;
	my_vertex.y = yPos + height - corner_radius;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = PVR_CMD_VERTEX_EOL;
	my_vertex.y = yPos + corner_radius;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	float theta = 0.0f;
	float increment = -F_PI / (2.0f * CORNER_TRIANGLES);
	float startX, startY;	
	int i;	

	//Q0
	startX = xPos + width - corner_radius;
	startY = yPos + corner_radius;
	for (i = 0; i < CORNER_TRIANGLES; i++) {

		my_vertex.flags = PVR_CMD_VERTEX;
		my_vertex.x = startX + corner_radius * cosf(theta);
		my_vertex.y = startY + corner_radius * sinf(theta);
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.x = startX;
		my_vertex.y = startY;
		pvr_prim(&my_vertex, sizeof(my_vertex));

		theta += increment;

		my_vertex.flags = PVR_CMD_VERTEX_EOL;
		my_vertex.x = startX + corner_radius * cosf(theta);
		my_vertex.y = startY + corner_radius * sinf(theta);
		pvr_prim(&my_vertex, sizeof(my_vertex));
	}

	//Q1
	startX = xPos + corner_radius;
	startY = yPos + corner_radius;
	for (i = 0; i < CORNER_TRIANGLES; i++) {

		my_vertex.flags = PVR_CMD_VERTEX;
		my_vertex.x = startX + corner_radius * cosf(theta);
		my_vertex.y = startY + corner_radius * sinf(theta);
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.x = startX;
		my_vertex.y = startY;
		pvr_prim(&my_vertex, sizeof(my_vertex));

		theta += increment;

		my_vertex.flags = PVR_CMD_VERTEX_EOL;
		my_vertex.x = startX + corner_radius * cosf(theta);
		my_vertex.y = startY + corner_radius * sinf(theta);
		pvr_prim(&my_vertex, sizeof(my_vertex));
	}

	//Q2
	startX = xPos + corner_radius;
	startY = yPos + height - corner_radius;
	for (i = 0; i < CORNER_TRIANGLES; i++) {

		my_vertex.flags = PVR_CMD_VERTEX;
		my_vertex.x = startX + corner_radius * cosf(theta);
		my_vertex.y = startY + corner_radius * sinf(theta);
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.x = startX;
		my_vertex.y = startY;
		pvr_prim(&my_vertex, sizeof(my_vertex));

		theta += increment;

		my_vertex.flags = PVR_CMD_VERTEX_EOL;
		my_vertex.x = startX + corner_radius * cosf(theta);
		my_vertex.y = startY + corner_radius * sinf(theta);
		pvr_prim(&my_vertex, sizeof(my_vertex));
	}	

	//Q3
	startX = xPos + width - corner_radius;
	startY = yPos + height - corner_radius;
	for (i = 0; i < CORNER_TRIANGLES; i++) {

		my_vertex.flags = PVR_CMD_VERTEX;
		my_vertex.x = startX + corner_radius * cosf(theta);
		my_vertex.y = startY + corner_radius * sinf(theta);
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.x = startX;
		my_vertex.y = startY;
		pvr_prim(&my_vertex, sizeof(my_vertex));

		theta += increment;

		my_vertex.flags = PVR_CMD_VERTEX_EOL;
		my_vertex.x = startX + corner_radius * cosf(theta);
		my_vertex.y = startY + corner_radius * sinf(theta);
		pvr_prim(&my_vertex, sizeof(my_vertex));
	}
}

void win_draw_textwindow (Window_Data* windata, Window_Style* winstyle, uint32 list) {
	int i;
	int yposition;
	//printf("win_draw_textwindow: starting text window draw\n");

	if (list == PVR_LIST_OP_POLY) {
		draw_cornered_layer(list,
			windata -> x, 
			windata -> y, 
			30.0f,
			windata -> width,
			windata -> height,
			CORNER_RADIUS,
			winstyle -> Border_Color);

		draw_cornered_layer(list,
			windata -> x + winstyle -> Border_Thickness, 
			windata -> y + winstyle -> Border_Thickness, 
			31.0f,
			windata -> width - (2 * winstyle -> Border_Thickness),
			windata -> height - (2 * winstyle -> Border_Thickness),
			CORNER_RADIUS - winstyle -> Border_Thickness,
			winstyle -> Inside_Color);

		// Draw right side scroll bar if there are enough strings on screen
		if (windata -> Num_Strings > winstyle -> Max_Items) {
			pvr_poly_hdr_t hdr;
			pvr_poly_cxt_t cxt;
			pvr_vertex_t my_vertex;		

			pvr_poly_cxt_col(&cxt, list);
			pvr_poly_compile(&hdr, &cxt);
			pvr_prim(&hdr, sizeof(hdr));

			//Draw scroll bar right side rect
			yposition = windata -> y + 15.0 + (windata -> font -> fontHeight * winstyle -> Header_Text_Scale);
			float barheight = (winstyle -> Max_Items * windata -> font -> fontHeight * winstyle -> Text_Scale);
			float xposition = windata -> x + windata -> width - (3 * winstyle -> Border_Thickness);
			my_vertex.flags = PVR_CMD_VERTEX;
			my_vertex.x = xposition;
			my_vertex.y = yposition + barheight;
			my_vertex.z = 32.0f;
			my_vertex.argb = 0xFFFFFFFF;
			my_vertex.oargb = 0;
			pvr_prim(&my_vertex, sizeof(my_vertex));

			my_vertex.y = yposition;
			pvr_prim(&my_vertex, sizeof(my_vertex));

			my_vertex.x = xposition + (2 * winstyle -> Border_Thickness);
			my_vertex.y = yposition + barheight;
			pvr_prim(&my_vertex, sizeof(my_vertex));

			my_vertex.flags = PVR_CMD_VERTEX_EOL;
			my_vertex.y = yposition;
			pvr_prim(&my_vertex, sizeof(my_vertex));

			//Draw scroll bar indicator rect
			float indicatorheight = (yposition + (winstyle -> Border_Thickness / 2)) + (((barheight - (2 * winstyle -> Border_Thickness)) * windata -> Highlighted_Index) / windata -> Num_Strings);
			my_vertex.flags = PVR_CMD_VERTEX;
			my_vertex.x = xposition + (winstyle -> Border_Thickness / 2);
			my_vertex.y = indicatorheight + (winstyle -> Border_Thickness);
			my_vertex.z = 33.0f;
			my_vertex.argb = 0x00000000;
			my_vertex.oargb = 0;
			pvr_prim(&my_vertex, sizeof(my_vertex));

			my_vertex.y = indicatorheight;
			pvr_prim(&my_vertex, sizeof(my_vertex));

			my_vertex.x = xposition + (3 * winstyle -> Border_Thickness / 2);
			my_vertex.y = indicatorheight + (winstyle -> Border_Thickness);
			pvr_prim(&my_vertex, sizeof(my_vertex));

			my_vertex.flags = PVR_CMD_VERTEX_EOL;
			my_vertex.y = indicatorheight;
			pvr_prim(&my_vertex, sizeof(my_vertex));			
		}
	}

	//printf("cornered_layer: [%f, %f, %f, %f]\n", windata -> x, windata -> y, windata -> width, windata -> height);

	if (list == PVR_LIST_TR_POLY) {
		draw_string(windata -> font,
			list,
			windata -> Header_Text,
			windata -> x + winstyle -> Left_Margin,
			windata -> y + 10.0f,
			35.0f,
			windata -> width - (2 * winstyle -> Left_Margin),
			windata -> height,
			SINGLE,
			LEFT,
			0xFF00FF00,
			winstyle -> Header_Text_Scale
		);

		//Now Draw the Text In the Window
		yposition = windata -> y + 15.0 + (windata -> font -> fontHeight * winstyle -> Header_Text_Scale);
		for (i = 0; (i < winstyle -> Max_Items) && (i < windata -> Num_Strings); i++) {
			char* text = windata -> Data_Strings[i + windata -> Top_Index];
			if ((text != NULL) && (text[0] != '\0')) {
				//printf("win_draw_textwindow: drawing regular line of text\n");						
				if (i + windata -> Top_Index != windata -> Highlighted_Index) {
					draw_string(windata -> font, list, windata -> Data_Strings[i + windata -> Top_Index], windata -> x + winstyle -> Left_Margin, yposition, 35.0f, windata -> width - (2 * winstyle -> Left_Margin) - (2 * winstyle -> Border_Thickness), windata -> height, SINGLE, LEFT, winstyle -> Text_Color, winstyle -> Text_Scale);
				} else {
					draw_string(windata -> font, list, windata -> Data_Strings[i + windata -> Top_Index], windata -> x + winstyle -> Left_Margin, yposition, 35.0f, windata -> width - (2 * winstyle -> Left_Margin) - (2 * winstyle -> Border_Thickness), windata -> height, SINGLE, LEFT, winstyle -> Selected_Text_Color, winstyle -> Text_Scale);
				}
				yposition += (windata -> font -> fontHeight * winstyle -> Text_Scale);
			}
		}

			//printf("win_draw_textwindow: drawing line [%i]\n", i);
			//But if the tag was found, pass it to the Checkbox Control Module
/*			
			if (strstr(text, Tag_CheckBox) != NULL)
			{
				printf("win_draw_textwindow: drawing checkbox\n");
				tempptr = text;
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
				tempbuffer[index - startindex] = ((char)NULL);

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

				yposition += windata -> font -> fontHeight;
			}
			//Check for the Slider Tag
			else
			if (strstr(text, Tag_Slider) != NULL)
			{
				printf("win_draw_textwindow: drawing slider\n");			
				int max;
				int min;
				int value;
				char* substrval;
				char numbuffer[255];

				tempptr = text;

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
			if (strstr(text, Tag_RLAlign) != NULL)
			{				
				printf("win_draw_textwindow: drawing regular line of text right aligned\n");
				if (i + windata -> Top_Index != windata -> Highlighted_Index)
				{
					bf_draw_str_rlalign(windata -> Item_Font, winstyle -> Text_Color, windata -> Target_Buffer + windata -> x + (xposition) + ((windata -> y + yposition) * windata -> Target_Width), windata -> Target_Width, text, windata -> width - 50);
				}
				else
				{
					bf_bgdraw_str_rlalign(windata -> Item_Font, winstyle -> Selected_Text_Color, winstyle -> Selected_Background_Color, windata -> Target_Buffer + windata -> x + (xposition) + ((windata -> y + yposition) * windata -> Target_Width), windata -> Target_Width, text, windata -> width - 50);
				}
				yposition += bf_str_height(windata -> Item_Font, "A");
			}
			else
			{
*/			
/*
				printf("win_draw_textwindow: drawing regular line of text\n");						
				if (i + windata -> Top_Index != windata -> Highlighted_Index) {
					draw_string(windata -> font, list, windata -> Data_Strings[windata -> Highlighted_Index], windata -> x + winstyle -> Left_Margin, windata -> y + yposition, 16.0f, windata -> width, windata -> height, SINGLE, LEFT, winstyle -> Text_Color);
				} else {
					draw_string(windata -> font, list, windata -> Data_Strings[windata -> Highlighted_Index], windata -> x + winstyle -> Left_Margin, windata -> y + yposition, 16.0f, windata -> width, windata -> height, SINGLE, LEFT, winstyle -> Selected_Text_Color);
				}
				yposition += windata -> font -> fontHeight;
*/				
//			}
	}
}
