/* 
File:        TextWindow.h
Project:     Platform Development Tools
Date:        December 18th, 2000
             May 5th, 2015
Programmer:  Matt Slevinsky
Description: A set of functions to draw a text window suitable for a GUI
*/

#ifndef __TEXT_WINDOW_H
#define __TEXT_WINDOW_H

#include <kos.h>
#include <string.h>
#include "BMFont.h"

typedef struct {
	//Visual Style Options
	int borderThickness;
	int maxItems;
	int useHeader;

	//Colors to Use
	uint16 backgroundColor;
	uint32 textColor;	
	uint32 headerTextColor;
	uint32 selectedTextColor;
	uint16 selectedBackgroundColor;

	uint16 borderInsideColor;
	uint16 borderOutsideColor;

	//Margin Info
	int leftMargin;
} WindowStyle;

typedef struct {
	//Physical Window Position Attributes
	int x;
	int y;
	int width;
	int height;

	//Data Attributes
	char* headerText;
	char** dataStrings;
	int numStrings;

	//The Top Index of the Window
	int topIndex;

	//Index of the highlighted string
	int selectionIndex;

	//Font Attributes
	BMFont* headerFont;
	BMFont* itemFont;

	//Target Attributes
	uint16* targetBuffer;
	int targetWidth;
} WindowData;

void win_draw_textwindow (WindowData* windata, WindowStyle* winstyle);

#endif