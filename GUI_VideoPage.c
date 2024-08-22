/*===================================================================*/
/*                                                                   */
/*  GUI_VideoPage.c : GUI Page for Video Settings                    */
/*                    Interface, Variables, and Event Handling       */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#include "GUI_VideoPage.h"
#include "macros.h"

#include "pNesX_System_Dc.h"

uint16* opt_Stretch;
uint16* opt_Filter;
uint32* opt_ClipVars;

char ClipLX_Buffer[50];
char ClipRX_Buffer[50];
char ClipTX_Buffer[50];
char ClipBX_Buffer[50];

const int MAX_CLIP_PIXELS = 64;

//Video Options Text
char Options_Video[] = "Video Options";
char Options_Stretch_Checked[] = "<Check>Stretch Video,CBC_CHECKED";
char Options_Stretch_Unchecked[] = "<Check>Stretch Video,CBC_UNCHECKED";
char Options_Filter_Checked[] = "<Check>Bilinear Filter,CBC_CHECKED";
char Options_Filter_Unchecked[] = "<Check>Bilinear Filter,CBC_UNCHECKED";
char Options_Clip_Left[] = "Clip Left Pixels";
char Options_Clip_Right[] = "Clip Right Pixels";
char Options_Clip_Top[] = "Clip Top Pixels";
char Options_Clip_Bottom[] = "Clip Bottom Pixels";
char* Video_Options[6];
const int Num_Video_Options = 6;

void Allocate_Video_Options() {
	opt_Stretch = malloc(sizeof(uint16));
	opt_Filter = malloc(sizeof(uint16));
	opt_ClipVars = malloc(4 * sizeof(uint32));
}

void Free_Video_Options() {
	free(opt_Stretch);
	free(opt_Filter);
	free(opt_ClipVars);
}

void setup_video_options_screen() {
	//Set Up Window Data Features
	mydata.x = 208.0f;
	mydata.y = 32.0f;
	mydata.width = 400.0f;
	mydata.height = 416.0f;
	mydata.font = font;
	mydata.Header_Text = Options_Video;
	mydata.Data_Strings = Video_Options;
	mydata.Num_Strings = Num_Video_Options;
	mydata.Highlighted_Index = 0;
	mydata.Top_Index = 0;

	//Set Up Window Style Features
	mystyle.Header_Text_Scale = 1.0f;
	mystyle.Text_Scale = 0.60f;
	mystyle.Border_Thickness = 5.0f;
	mystyle.Border_Color = GUI_OutsideWindowColor; //		MakeRGB(8, 20, 10);
	mystyle.Inside_Color = GUI_InsideWindowColor; //MakeRGB(8, 20, 32);
	mystyle.Left_Margin = 15;
	mystyle.Line_Spacing = 2.0f;
	mystyle.Header_Text_Color = GUI_TextColor; //0x8000;
	mystyle.Text_Color = GUI_TextColor;
	mystyle.Max_Items = (mydata.height - (mydata.font -> fontHeight * mystyle.Header_Text_Scale)) / ((float)mydata.font -> fontHeight * mystyle.Text_Scale);
	mystyle.Selected_Text_Color = GUI_SelectedTextColor;
	mystyle.Selected_Background_Color = GUI_SelectedTextColor; //MakeRGB(8, 18, 32);

	//Set Up Window Data Features
	helpdata.x = 32.0f;
	helpdata.y = 300.0f;
	helpdata.width = 160.0f;
	helpdata.height = 148.0f;
	helpdata.font = font;
	helpdata.Header_Text = " ";
	helpdata.Data_Strings = Options_Keys;
	helpdata.Num_Strings = Num_Options_Keys;
	helpdata.Highlighted_Index = Num_Options_Keys;
	helpdata.Top_Index = 0;

	//Set Up Window Style Features
	helpstyle.Border_Thickness = 5.0f;
	helpstyle.Border_Color = GUI_OutsideWindowColor;
	helpstyle.Inside_Color = GUI_InsideWindowColor;
	helpstyle.Left_Margin = 15;
	helpstyle.Line_Spacing = 0.0f;	
	helpstyle.Header_Text_Color = GUI_TextColor;
	helpstyle.Text_Color = GUI_TextColor;
	helpstyle.Max_Items = 10;
	helpstyle.Selected_Text_Color = GUI_SelectedTextColor;
	helpstyle.Selected_Background_Color = GUI_SelectedTextColor;//MakeRGB(31, 18, 8);
}

//Generates the Options GUI info from the variables in memory
void Generate_Video_Options_List() {
	switch(*opt_Stretch) {
		case 0:
			Video_Options[0] = Options_Stretch_Unchecked;
			break;
		case 1:
			Video_Options[0] = Options_Stretch_Checked;
			break;
	}
	switch(*opt_Filter) {
		case 0:
			Video_Options[1] = Options_Filter_Unchecked;
			break;
		case 1:
			Video_Options[1] = Options_Filter_Checked;
			break;
	}

	snprintf(ClipLX_Buffer, 50, "%s,%lu<RLAlign>", Options_Clip_Left, opt_ClipVars[0]);
	Video_Options[2] = ClipLX_Buffer;

	snprintf(ClipRX_Buffer, 50, "%s,%lu<RLAlign>", Options_Clip_Right, opt_ClipVars[1]);
	Video_Options[3] = ClipRX_Buffer;

	snprintf(ClipTX_Buffer, 50, "%s,%lu<RLAlign>", Options_Clip_Top, opt_ClipVars[2]);
	Video_Options[4] = ClipTX_Buffer;

	snprintf(ClipBX_Buffer, 50, "%s,%lu<RLAlign>", Options_Clip_Bottom, opt_ClipVars[3]);
	Video_Options[5] = ClipBX_Buffer;
}

void Handle_Video_Interface(cont_state_t* my_state) {
	//Down Key Hit and Key is Ready to be hit
	if ((my_state -> buttons & CONT_DPAD_DOWN) && 
		(mydata.Highlighted_Index < Num_Video_Options) && 
		(keyhit == 0)) {
		mydata.Highlighted_Index++;
		if ((mydata.Highlighted_Index - mydata.Top_Index) >= mystyle.Max_Items)
			mydata.Top_Index++;
		keyhit = 1;
	}

	//Up Key Hit and Key is Ready to be hit
	if ((my_state -> buttons & CONT_DPAD_UP) && 
		(mydata.Highlighted_Index > 0) && 
		(keyhit == 0)) {
		mydata.Highlighted_Index--;
		if (mydata.Top_Index > mydata.Highlighted_Index)
			mydata.Top_Index--;
		keyhit = 1;
	}

	//Handle the toggle boxes
	if ((my_state -> buttons & CONT_A) && 
		(invalida == 0)) {
		switch(mydata.Highlighted_Index) {
			case 0:
				*opt_Stretch = 1 - *opt_Stretch;
				Generate_Video_Options_List();
				invalida = 1;
				break;
			case 1:
				*opt_Filter = 1 - *opt_Filter;
				Generate_Video_Options_List();
				invalida = 1;
				break;
		}
	}

	//Handle the modify boxes
	if ((my_state -> buttons & CONT_DPAD_LEFT) && 
		(xkeyhit == 0)) {
		switch(mydata.Highlighted_Index) {
			case 2:
				if (opt_ClipVars[0] > 0) {
					(opt_ClipVars[0]) -= 8;
					Generate_Video_Options_List();
					xkeyhit = 1;
				}
				break;
			case 3:
				if (opt_ClipVars[1] > 0) {
					(opt_ClipVars[1]) -= 8;
					Generate_Video_Options_List();
					xkeyhit = 1;
				}
				break;
			case 4:
				if (opt_ClipVars[2] > 0) {
					(opt_ClipVars[2]) -= 8;
					Generate_Video_Options_List();
					xkeyhit = 1;
				}
				break;
			case 5:
				if (opt_ClipVars[3] > 0) {
					(opt_ClipVars[3]) -= 8;
					Generate_Video_Options_List();
					xkeyhit = 1;
				}
				break;
		}
	}					

	if ((my_state -> buttons & CONT_DPAD_RIGHT) && 
		(xkeyhit == 0)) {
		switch(mydata.Highlighted_Index) {
			case 2:
				if (opt_ClipVars[0] < MAX_CLIP_PIXELS) {
					(opt_ClipVars[0]) += 8;
					Generate_Video_Options_List();
					xkeyhit = 1;
				}
				break;
			case 3:
				if (opt_ClipVars[1] < MAX_CLIP_PIXELS) {
					(opt_ClipVars[1]) += 8;
					Generate_Video_Options_List();
					xkeyhit = 1;
				}
				break;
			case 4:
				if (opt_ClipVars[2] < MAX_CLIP_PIXELS) {
					(opt_ClipVars[2]) += 8;
					Generate_Video_Options_List();
					xkeyhit = 1;
				}
				break;
			case 5:
				if (opt_ClipVars[3] < MAX_CLIP_PIXELS) {
					(opt_ClipVars[3]) += 8;
					Generate_Video_Options_List();
					xkeyhit = 1;
				}
				break;
		}
	}					
	
	// Handle Return to Main Menu
	if ((my_state -> buttons & CONT_B) && 
		(keyhit == 0)) {
		setup_main_menu_screen();
		menuscreen = MENUNUM_MAIN;
	}
}