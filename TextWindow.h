/* 
File:        TextWindow.h
Project:     Platform Development Tools
Date:        December. 18th, 2000
Programmer:  ReGex
Description: A set of functions to draw a text window suitable for a GUI
*/

#ifndef __TEXT_WINDOW_H
#define __TEXT_WINDOW_H

#include <kos.h>
#include <string.h>
#include "font.h"

#define CIRCLE_TRIANGLES 64
#define CORNER_TRIANGLES 16
#define CORNER_RADIUS 16

typedef struct __attribute__ ((packed, aligned(4))) Window_Style_s {
	//Visual Style Options
	float Border_Thickness;
	float Header_Text_Scale;
	float Text_Scale;
	int Max_Items;
	int Use_Header;

	//Colors to Use
	uint32 Border_Color;
	uint32 Inside_Color;
	uint32 Header_Text_Color;
	uint32 Selected_Text_Color;
	uint32 Selected_Background_Color;
	uint32 Text_Color;

	//Margin Info
	float Left_Margin;
	float Line_Spacing;
	
	//Not Implemented Yet -- Probably Never Will Be Either :D
	int IsTransparent;
	int IsAnimating;
	int Animation_Mode;
} Window_Style;

typedef struct __attribute__ ((packed, aligned(4))) Window_Data_s {
	//Physical Window Position Attributes
	float x;
	float y;
	float width;
	float height;

	//Data Attributes
	char* Header_Text;
	char** Data_Strings;

	//The Top Index of the Window
	int Top_Index;
	int Num_Strings;

	//Index of the highlighted string
	int Highlighted_Index;

	//Font Attributes
	Font* font;
} Window_Data;

void win_draw_textwindow (Window_Data* windata, Window_Style* winstyle, uint32 list);

#endif