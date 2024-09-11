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

void draw_scrollbar(uint32 list, float xPos, float yPos, float zPos, float width, float height, uint32 bordercolor, uint32 insidecolor, int min, int max, int value) {

	draw_cornered_layer(list, xPos, yPos + (height / 2) - 3.0f, zPos, width, 6.0f, 5.0f, bordercolor);

	float sliderRadius = height / 2;
	float sliderX = xPos + (sliderRadius) + ((width - (2 * sliderRadius)) * (float)value / (float)(max - min));
	float sliderY = yPos + sliderRadius;

	float theta = 0.0f;
	float increment = -2.0f * F_PI / ((float)CIRCLE_TRIANGLES);

	pvr_poly_hdr_t hdr;
	pvr_poly_cxt_t cxt;
	pvr_vertex_t my_vertex;		

	pvr_poly_cxt_col(&cxt, list);
	pvr_poly_compile(&hdr, &cxt);
	pvr_prim(&hdr, sizeof(hdr));

	for (int i = 0; i < CIRCLE_TRIANGLES; i++) {
		my_vertex.flags = PVR_CMD_VERTEX;
		my_vertex.x = sliderX + sliderRadius * cosf(theta);
		my_vertex.y = sliderY + sliderRadius * sinf(theta);
		my_vertex.z = zPos + 0.1f;
		my_vertex.argb = insidecolor;
		my_vertex.oargb = 0;		
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.x = sliderX;
		my_vertex.y = sliderY;
		pvr_prim(&my_vertex, sizeof(my_vertex));

		theta += increment;

		my_vertex.flags = PVR_CMD_VERTEX_EOL;
		my_vertex.x = sliderX + sliderRadius * cosf(theta);
		my_vertex.y = sliderY + sliderRadius * sinf(theta);
		pvr_prim(&my_vertex, sizeof(my_vertex));
	}
}

void draw_checktext(uint32 list, float xPos, float yPos, float zPos, float width, float height, float thickness, uint32 frameColor, uint32 innerColor, bool checked) {
	draw_cornered_layer(list, xPos, yPos, zPos, width, height, 4.0f, frameColor);
	if (checked) {
		pvr_poly_hdr_t hdr;
		pvr_poly_cxt_t cxt;
		pvr_vertex_t my_vertex;		

		pvr_poly_cxt_col(&cxt, list);
		pvr_poly_compile(&hdr, &cxt);
		pvr_prim(&hdr, sizeof(hdr));

		//Draw top triangle
		my_vertex.flags = PVR_CMD_VERTEX;
		my_vertex.x = xPos + (width / 2);
		my_vertex.y = yPos + (height / 2) - thickness;
		my_vertex.z = zPos + 0.1f;
		my_vertex.argb = innerColor;
		my_vertex.oargb = 0;
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.x = xPos + (2.0f * thickness);
		my_vertex.y = yPos + thickness;
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.flags = PVR_CMD_VERTEX_EOL;
		my_vertex.x = xPos + width - (2.0f * thickness);
		pvr_prim(&my_vertex, sizeof(my_vertex));

		//Draw right triangle
		my_vertex.flags = PVR_CMD_VERTEX;
		my_vertex.x = xPos + (width / 2) + thickness;
		my_vertex.y = yPos + (height / 2);
		my_vertex.oargb = 0;
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.x = xPos + width - thickness;
		my_vertex.y = yPos + (2.0f * thickness);
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.flags = PVR_CMD_VERTEX_EOL;
		my_vertex.y = yPos + height - (2.0f * thickness);		
		pvr_prim(&my_vertex, sizeof(my_vertex));

		//Draw bottom triangle
		my_vertex.flags = PVR_CMD_VERTEX;
		my_vertex.x = xPos + (width / 2);
		my_vertex.y = yPos + (height / 2) + thickness;
		my_vertex.argb = innerColor;
		my_vertex.oargb = 0;
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.x = xPos + width - (2.0f * thickness);
		my_vertex.y = yPos + height - thickness;
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.flags = PVR_CMD_VERTEX_EOL;
		my_vertex.x = xPos + (2.0f * thickness);
		pvr_prim(&my_vertex, sizeof(my_vertex));

		//Draw left triangle
		my_vertex.flags = PVR_CMD_VERTEX;
		my_vertex.x = xPos + (width / 2) - thickness;
		my_vertex.y = yPos + (height / 2);
		my_vertex.oargb = 0;
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.x = xPos + thickness;
		my_vertex.y = yPos + height - (2.0f * thickness);
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.flags = PVR_CMD_VERTEX_EOL;
		my_vertex.y = yPos + (2.0f * thickness);
		pvr_prim(&my_vertex, sizeof(my_vertex));
	} else {
		draw_cornered_layer(list, xPos + thickness, yPos + thickness, zPos + 0.1f, width - (2 * thickness), height - (2 * thickness), 4.0f, innerColor);
	}
}

void handle_checkbox(char* text, uint32 i, Window_Data* windata, Window_Style* winstyle, uint32 list, float yposition) {
	//printf("win_draw_textwindow: drawing checkbox\n");
	int index;
	int startindex;
	char* tempptr;
	char tempbuffer[255];

	tempptr = text;
	//Parse the text field
	index = 0;
	while (tempptr[index] != '>')
		index++;
	index++;
	startindex = index;
	//Tempbuffer will contain the message text
	while (tempptr[index] != ',') {
		tempbuffer[index - startindex] = tempptr[index];
		index++;
	}
	tempbuffer[index - startindex] = '\0';

	//If checked was found
	if (strstr(tempptr, CHECKBOX_CHECKED) != NULL)
		index = 1;
	else if (strstr(tempptr, CHECKBOX_UNCHECKED) != NULL)
		index = 0;
	else
		index = 0;

	float checkHeight = (windata -> font -> fontHeight * winstyle -> Text_Scale);
	if (i + windata -> Top_Index != windata -> Highlighted_Index) {
		draw_string(windata -> font, list, tempbuffer, windata -> x + winstyle -> Left_Margin, yposition, 35.0f, windata -> width - (2 * winstyle -> Left_Margin) - (2 * winstyle -> Border_Thickness), windata -> height, SINGLE, LEFT, winstyle -> Text_Color, winstyle -> Text_Scale);
		draw_checktext(list, windata -> x + windata -> width - winstyle -> Left_Margin - 25.0f, yposition, 35.0f, checkHeight, checkHeight, 2.0f, winstyle -> Text_Color, winstyle -> Inside_Color, index);
	} else {
		draw_string(windata -> font, list, tempbuffer, windata -> x + winstyle -> Left_Margin, yposition, 35.0f, windata -> width - (2 * winstyle -> Left_Margin) - (2 * winstyle -> Border_Thickness), windata -> height, SINGLE, LEFT, winstyle -> Selected_Text_Color, winstyle -> Text_Scale);
		draw_checktext(list, windata -> x + windata -> width - winstyle -> Left_Margin - 25.0f, yposition, 35.0f, checkHeight, checkHeight, 2.0f, winstyle -> Selected_Text_Color, winstyle -> Inside_Color, index);
	}
}

void handle_ralign(char* text, uint32 i, Window_Data* windata, Window_Style* winstyle, uint32 list, float yposition) {
	//printf("win_draw_textwindow: drawing checkbox\n");
	int index;
	int startindex;
	char* tempptr;
	char s1buffer[255];
	char s2buffer[255];

	tempptr = text;
	//Parse the text field
	index = 0;
	startindex = index;
	//Tempbuffer will contain the message text
	while (tempptr[index] != ',') {
		s1buffer[index - startindex] = tempptr[index];
		index++;
	}
	s1buffer[index - startindex] = '\0';
	index++;
	startindex = index;

	while (tempptr[index] != '<') {
		s2buffer[index - startindex] = tempptr[index];
		index++;
	}
	s2buffer[index - startindex] = '\0';

	float leftWidth = (windata -> width - (2 * winstyle -> Left_Margin) - (2 * winstyle -> Border_Thickness)) * 0.80f;
	float rightWidth = (windata -> width - (2 * winstyle -> Left_Margin) - (2 * winstyle -> Border_Thickness)) * 0.20f;
	if (i + windata -> Top_Index != windata -> Highlighted_Index) {
		draw_string(windata -> font, list, s1buffer, windata -> x + winstyle -> Left_Margin, yposition, 35.0f, leftWidth, windata -> height, SINGLE, LEFT, winstyle -> Text_Color, winstyle -> Text_Scale);
		draw_string(windata -> font, list, s2buffer, windata -> x + winstyle -> Left_Margin + leftWidth, yposition, 35.0f, rightWidth, windata -> height, SINGLE, RIGHT, winstyle -> Text_Color, winstyle -> Text_Scale);
	} else {
		draw_string(windata -> font, list, s1buffer, windata -> x + winstyle -> Left_Margin, yposition, 35.0f, leftWidth, windata -> height, SINGLE, LEFT, winstyle -> Selected_Text_Color, winstyle -> Text_Scale);
		draw_string(windata -> font, list, s2buffer, windata -> x + winstyle -> Left_Margin + leftWidth, yposition, 35.0f, rightWidth, windata -> height, SINGLE, RIGHT, winstyle -> Selected_Text_Color, winstyle -> Text_Scale);
	}	
}

void handle_slider(char* text, uint32 i, Window_Data* windata, Window_Style* winstyle, uint32 list, float yposition) {
	//printf("win_draw_textwindow: drawing slider\n");			
	int max;
	int min;
	int value;
	int index;

	char* tempptr;
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
		numbuffer[index] = '\0';
		max = atoi(numbuffer);
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
		numbuffer[index] = '\0';
		min = atoi(numbuffer);
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
		numbuffer[index] = '\0';
		value = atoi(numbuffer);
	}

	if (i + windata -> Top_Index != windata -> Highlighted_Index) {
        draw_scrollbar(list, windata -> x + winstyle -> Border_Thickness + winstyle -> Left_Margin, yposition, 35.0f, windata -> width - 30.0f - (winstyle -> Left_Margin * 2), windata -> font -> fontHeight * winstyle -> Text_Scale, 0xFF000000, winstyle -> Text_Color, min, max, value);
	} else {
        draw_scrollbar(list, windata -> x + winstyle -> Border_Thickness + winstyle -> Left_Margin, yposition, 35.0f, windata -> width - 30.0f - (winstyle -> Left_Margin * 2), windata -> font -> fontHeight * winstyle -> Text_Scale, 0xFF000000, winstyle -> Selected_Text_Color, min, max, value);
	}
}

void win_draw_textwindow(Window_Data* windata, Window_Style* winstyle, uint32 list) {
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
			yposition = windata -> y + 10.0 + (windata -> font -> fontHeight * winstyle -> Header_Text_Scale);
			float barheight = (winstyle -> Max_Items * ((windata -> font -> fontHeight * winstyle -> Text_Scale) + winstyle -> Line_Spacing));
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
				if (strstr(text, Tag_CheckBox) != NULL) {
					handle_checkbox(text, i, windata, winstyle, list, yposition);
				} else if (strstr(text, Tag_Slider) != NULL) {
					handle_slider(text, i, windata, winstyle, list, yposition);
				} else if (strstr(text, Tag_RLAlign) != NULL) {
					handle_ralign(text, i, windata, winstyle, list, yposition);
				} else {
					//printf("win_draw_textwindow: drawing regular line of text\n");						
					if (i + windata -> Top_Index != windata -> Highlighted_Index) {
						draw_string(windata -> font, list, windata -> Data_Strings[i + windata -> Top_Index], windata -> x + winstyle -> Left_Margin, yposition, 35.0f, windata -> width - (2 * winstyle -> Left_Margin) - (2 * winstyle -> Border_Thickness), windata -> height, SINGLE, LEFT, winstyle -> Text_Color, winstyle -> Text_Scale);
					} else {
						draw_string(windata -> font, list, windata -> Data_Strings[i + windata -> Top_Index], windata -> x + winstyle -> Left_Margin, yposition, 35.0f, windata -> width - (2 * winstyle -> Left_Margin) - (2 * winstyle -> Border_Thickness), windata -> height, SINGLE, LEFT, winstyle -> Selected_Text_Color, winstyle -> Text_Scale);
					}
				}
				yposition += (windata -> font -> fontHeight * winstyle -> Text_Scale) + winstyle -> Line_Spacing;
			}
		}
	}
}
