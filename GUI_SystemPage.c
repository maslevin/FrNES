/*===================================================================*/
/*                                                                   */
/*  GUI_SystemPage.c : GUI Page for System Settings                  */
/*                     Interface, Variables, and Event Handling      */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#include "GUI_SystemPage.h"
#include "macros.h"

#include "pNesX_System_DC.h"

char Frameskip_Buffer[50];
char Recording_Mode_Buffer[50];

//System Options Text
const char Options_System[] = "System Options";
const char Options_Frameskip[] = "Frameskip";
const char Options_Sound_Checked[] = "<Check>Enable Sound,CBC_CHECKED";
const char Options_Sound_Unchecked[] = "<Check>Enable Sound,CBC_UNCHECKED";
const char Options_Show_Framerate_Checked[] = "<Check>Show Framerate,CBC_CHECKED";
const char Options_Show_Framerate_Unchecked[] = "<Check>Show Framerate,CBC_UNCHECKED";
const char Options_SRAM_Checked[] = "<Check>SRAM Enable,CBC_CHECKED";
const char Options_SRAM_Unchecked[] = "<Check>SRAM Enable,CBC_UNCHECKED";
const char Options_LoadVMU[] = "Load Options From VMU";
const char Options_SaveVMU[] = "Save Options To VMU";
const char Options_DeleteVMU[] = "Delete Options From VMU";
const char Options_Recording_Mode[] = "Input Recording Mode";
const char Options_Recording_Mode_Disabled[] = "Disabled";
const char Options_Recording_Mode_Record[] = "Record";
const char Options_Recording_Mode_Playback[] = "Playback";

char* System_Options[8];
const int Num_System_Options = 8;

void setup_system_options_screen() {
	//Set Up Window Data Features
	mydata.x = 208.0f;
	mydata.y = 32.0f;
	mydata.width = 400.0f;
	mydata.height = 416.0f;
	mydata.font = font;
	mydata.Header_Text = Options_System;
	mydata.Data_Strings = System_Options;
	mydata.Num_Strings = Num_System_Options;
	mydata.Highlighted_Index = 0;
	mydata.Top_Index = 0;

	//Set Up Window Style Features
	mystyle.Header_Text_Scale = 1.0f;
	mystyle.Text_Scale = 0.60f;			
	mystyle.Border_Thickness = 5.0f;
	mystyle.Border_Color = options.GUI_OutsideWindowColor; //		MakeRGB(8, 20, 10);
	mystyle.Inside_Color = options.GUI_InsideWindowColor; //MakeRGB(8, 20, 32);
	mystyle.Left_Margin = 15;
	mystyle.Line_Spacing = 2.0f;
	mystyle.Header_Text_Color = options.GUI_TextColor; //0x8000;
	mystyle.Text_Color = options.GUI_TextColor;
	mystyle.Max_Items = (mydata.height - (mydata.font -> fontHeight * mystyle.Header_Text_Scale)) / ((float)mydata.font -> fontHeight * mystyle.Text_Scale);
	mystyle.Selected_Text_Color = options.GUI_SelectedTextColor;
	mystyle.Selected_Background_Color = options.GUI_SelectedTextColor; //MakeRGB(8, 18, 32);

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
	helpstyle.Border_Color = options.GUI_OutsideWindowColor;
	helpstyle.Inside_Color = options.GUI_InsideWindowColor;
	helpstyle.Left_Margin = 15;
	helpstyle.Line_Spacing = 0.0f;	
	helpstyle.Header_Text_Color = options.GUI_TextColor;
	helpstyle.Text_Color = options.GUI_TextColor;
	helpstyle.Max_Items = 10;
	helpstyle.Selected_Text_Color = options.GUI_SelectedTextColor;
	helpstyle.Selected_Background_Color = options.GUI_SelectedTextColor;//MakeRGB(31, 18, 8);
}

void Generate_System_Options_List() {
	switch(options.opt_SoundEnabled) {
		case 0:
			System_Options[0] = Options_Sound_Unchecked;
			break;
		case 1:
			System_Options[0] = Options_Sound_Checked;
			break;
	}

	switch(options.opt_AutoFrameSkip) {
		case 0:
			snprintf(Frameskip_Buffer, 50, "%s,%u<RLAlign>", Options_Frameskip, options.opt_FrameSkip);
			System_Options[1] = Frameskip_Buffer;
			break;
		case 1:
			strcpy(Frameskip_Buffer, "AutoFrameSkip");
			strcat(Frameskip_Buffer, ",");
			strcat(Frameskip_Buffer, "ON");
			strcat(Frameskip_Buffer, "<RLAlign>");
			System_Options[1] = Frameskip_Buffer;
			break;
	}

	switch(options.opt_ShowFrameRate) {
		case 0:
			System_Options[2] = Options_Show_Framerate_Unchecked;
			break;
		case 1:
			System_Options[2] = Options_Show_Framerate_Checked;
			break;
	}

	switch(options.opt_SRAM) {
		case 0:
			System_Options[3] = Options_SRAM_Unchecked;
			break;
		case 1:
			System_Options[3] = Options_SRAM_Checked;
			break;
	}

	System_Options[4] = Options_LoadVMU;
	System_Options[5] = Options_SaveVMU;
	System_Options[6] = Options_DeleteVMU;

	switch(recordingMode) {
		case 0:
			snprintf(Recording_Mode_Buffer, 50, "%s,%s<RLAlign>", Options_Recording_Mode, Options_Recording_Mode_Disabled);
			break;
		case 1:
			snprintf(Recording_Mode_Buffer, 50, "%s,%s<RLAlign>", Options_Recording_Mode, Options_Recording_Mode_Record);
			break;
		case 2:
			snprintf(Recording_Mode_Buffer, 50, "%s,%s<RLAlign>", Options_Recording_Mode, Options_Recording_Mode_Playback);
			break;
	}
	System_Options[7] = Recording_Mode_Buffer;
}

void Handle_System_Interface(cont_state_t* my_state) {
	//Down Key Hit and Key is Ready to be hit
	if ((my_state -> buttons & CONT_DPAD_DOWN) && 
		(mydata.Highlighted_Index < Num_System_Options) && 
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
				options.opt_SoundEnabled = 1 - options.opt_SoundEnabled;
				Generate_System_Options_List();
				invalida = 1;
				break;
			case 2:
				options.opt_ShowFrameRate = 1 - options.opt_ShowFrameRate;
				Generate_System_Options_List();
				invalida = 1;
				break;				
			case 3:
				options.opt_SRAM  = 1 - options.opt_SRAM;
				Generate_System_Options_List();
				invalida = 1;
				break;
			// Load From VMU
			case 4:
				load_options_from_VMU();
				invalida = 1;
				break;
			// Save To VMU
			case 5:
				save_options_to_VMU();
				invalida = 1;
				break;
			// Delete options from VMU
			case 6:
				delete_options_from_VMU();
				invalida = 1;
				break;
		}
	}

	//Handle the modify boxes
	if ((my_state -> buttons & CONT_DPAD_LEFT) && 
		(xkeyhit == 0)) {
		switch(mydata.Highlighted_Index) {
			case 1: {
				if ((options.opt_FrameSkip) > 0) {
					(options.opt_FrameSkip)--;
					Generate_System_Options_List();
					xkeyhit = 1;
				} else if (options.opt_FrameSkip == 0) {
					(options.opt_AutoFrameSkip) = 1;
					Generate_System_Options_List();
					xkeyhit = 1;
				}
			} break;

			case 7: {
				if (recordingMode != RECORDING_MODE_DISABLED) {
					recordingMode--;
					Generate_System_Options_List();
					xkeyhit = 1;					
				}
			} break;
		}
	}					

	if ((my_state -> buttons & CONT_DPAD_RIGHT) && 
		(xkeyhit == 0)) {
		switch(mydata.Highlighted_Index) {
			case 1: {
				if ((options.opt_AutoFrameSkip) == 1) {
					(options.opt_AutoFrameSkip) = 0;
					Generate_System_Options_List();
					xkeyhit = 1;
				} else if ((options.opt_FrameSkip) < Max_Frameskip) {
					(options.opt_FrameSkip)++;
					Generate_System_Options_List();
					xkeyhit = 1;
				}
			} break;

			case 7: {
				if (recordingMode != RECORDING_MODE_PLAYBACK) {
					recordingMode++;
					Generate_System_Options_List();
					xkeyhit = 1;					
				}
			} break;				
		}
	}					

	// Handle Return to Main Menu
	if ((my_state -> buttons & CONT_B) && 
		(keyhit == 0)) {
		setup_main_menu_screen();
		menuscreen = MENUNUM_MAIN;
	}
}
