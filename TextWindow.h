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
#include "BMFFile.h"

typedef struct
{
	//Visual Style Options
	int Border_Thickness;
	int Max_Items;
	int Use_Header;

	//Colors to Use
	uint16 Underline_Color_Start;
	uint16 Underline_Color_End;
	uint16 Border_Inside_Color;
	uint16 Border_Outside_Color;

	uint16 Inside_Color;
	uint16 Header_Text_Color;
	uint16 Selected_Text_Color;
	uint16 Selected_Background_Color;
	uint16 Text_Color;

	//Margin Info
	int Left_Margin;
	
	//Not Implemented Yet -- Probably Never Will Be Either :D
	int IsTransparent;
	int IsAnimating;
	int Animation_Mode;
} Window_Style;

typedef struct
{
	//Physical Window Position Attributes
	int x;
	int y;
	int width;
	int height;

	//Data Attributes
	char* Header_Text;
	char** Data_Strings;

	//The Top Index of the Window
	int Top_Index;
	int Num_Strings;

	//Index of the highlighted string
	int Highlighted_Index;

	//Font Attributes
	BMF_Character* Header_Font;
	BMF_Character* Item_Font;

	//Target Attributes
	uint16* Target_Buffer;
	int Target_Width;
} Window_Data;

void win_draw_textwindow (Window_Data* windata, Window_Style* winstyle, pvr_ptr_t target);

#endif