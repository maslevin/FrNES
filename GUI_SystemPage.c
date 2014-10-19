/*===================================================================*/
/*                                                                   */
/*  GUI_SystemPage.c : GUI Page for System Settings                  */
/*                     Interface, Variables, and Event Handling      */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#include "GUI_SystemPage.h"
#include "pNesX_System_DC.h"
#include "pNesX_Utils.h"

char Frameskip_Buffer[50];

//System Options Text
char Options_System[] = "System Options";
char Options_Frameskip[] = "Frameskip";
char Options_Sound_Checked[] = "<Check>Enable Sound,CBC_CHECKED";
char Options_Sound_Unchecked[] = "<Check>Enable Sound,CBC_UNCHECKED";
char Options_CDFormat_FrNES[] = "Use CD Format:,FrNES<RLAlign>";
char Options_CDFormat_NesterDC[] = "Use CD Format:,NesterDC<RLAlign>";
char Options_VMU[] = "VMU Options";
char Options_Port[] = "Use VMU Port";
char Options_CXXX[] = "No VMU Detected";
char Options_C1S1[] = "Controller 1 Slot 1";
char Options_C1S2[] = "Controller 1 Slot 2";
char Options_C2S1[] = "Controller 2 Slot 1";
char Options_C2S2[] = "Controller 2 Slot 2";
char Options_C3S1[] = "Controller 3 Slot 1";
char Options_C3S2[] = "Controller 3 Slot 2";
char Options_C4S1[] = "Controller 4 Slot 1";
char Options_C4S2[] = "Controller 4 Slot 2";
char Options_SRAM_Checked[] = "<Check>SRAM Enable,CBC_CHECKED";
char Options_SRAM_Unchecked[] = "<Check>SRAM Enable,CBC_UNCHECKED";
char Options_LoadVMU[] = "Load From VMU";
char Options_SaveVMU[] = "Save From VMU";
char Options_Rescan[] = "Rescan Maple Bus";

char* System_Options[10];
const int Num_System_Options = 10;

//Options Variables
uint16* opt_SoundEnabled;
uint16* opt_FrameSkip;
uint16* opt_AutoFrameSkip;
uint16* opt_DiscFormat;
int16* opt_VMUPort;
uint16* opt_SRAM;

void Allocate_System_Options()
{
	opt_SoundEnabled = malloc(sizeof(uint16));
	opt_FrameSkip = malloc(sizeof(uint16));
	opt_AutoFrameSkip = malloc(sizeof(uint16));
	opt_DiscFormat = malloc(sizeof(uint16));
	opt_VMUPort = malloc(sizeof(int16));
	opt_SRAM = malloc(sizeof(uint16));
}

void Free_System_Options()
{
	free(opt_SoundEnabled);
	free(opt_FrameSkip);
	free(opt_AutoFrameSkip);
	free(opt_DiscFormat);
	free(opt_VMUPort);
	free(opt_SRAM);
}

void setup_system_options_screen()
{
	//Set Up Window Style Features
	mystyle.Border_Inside_Color = GUI_OutsideWindowColor; //		MakeRGB(8, 20, 10);
	mystyle.Border_Outside_Color = 0x8000;
	mystyle.Inside_Color = GUI_InsideWindowColor; //MakeRGB(8, 20, 32);
	mystyle.Left_Margin = 15;
	mystyle.Header_Text_Color = GUI_TextColor; //0x8000;
	mystyle.Text_Color = GUI_TextColor;
	mystyle.Max_Items = (mydata.height - (35 + mydata.Header_Font[0].height)) / (mydata.Item_Font[0].height);
	mystyle.Selected_Text_Color = GUI_TextColor;
	mystyle.Selected_Background_Color = GUI_SelectedTextColor; //MakeRGB(8, 18, 32);

	//Set Up Window Data Features
	mydata.x = 0;
	mydata.y = 0;
	mydata.width = 400;
	mydata.height = 460 - (2 * title_offset_y);
	mydata.Target_Width = 512;
	mydata.Header_Font = largefont;
	mydata.Item_Font = medfont;
	mydata.Header_Text = Options_System;
	mydata.Data_Strings = System_Options;
	mydata.Num_Strings = Num_System_Options;
	mydata.Highlighted_Index = 0;
	mydata.Top_Index = 0;
	mydata.Target_Buffer = PVR_MainWindow_Offset;

	//Set Up Window Style Features
	helpstyle.Border_Inside_Color = GUI_OutsideWindowColor;
	helpstyle.Border_Outside_Color = 0x8000;
	helpstyle.Inside_Color = GUI_InsideWindowColor;
	helpstyle.Left_Margin = 15;
	helpstyle.Header_Text_Color = GUI_TextColor;
	helpstyle.Text_Color = GUI_TextColor;
	helpstyle.Max_Items = 10;
	helpstyle.Selected_Text_Color = GUI_TextColor;
	helpstyle.Selected_Background_Color = GUI_SelectedTextColor;//MakeRGB(31, 18, 8);

	//Set Up Window Data Features
	helpdata.x = 0;
	helpdata.y = 0;
	helpdata.width = 160;
	helpdata.height = 160;
	helpdata.Target_Width = 512;
	helpdata.Header_Font = smallfont;
	helpdata.Item_Font = smallfont;
	helpdata.Header_Text = " ";
	helpdata.Data_Strings = Options_Keys;
	helpdata.Num_Strings = Num_Options_Keys;
	helpdata.Highlighted_Index = Num_Options_Keys;
	helpdata.Top_Index = 0;
	helpdata.Target_Buffer = PVR_SmallWindow_Offset;
}

void Generate_System_Options_List()
{
	char tempbuffer[255];

	switch(*opt_SoundEnabled)
	{
		case 0:
			System_Options[0] = Options_Sound_Unchecked;
			break;
		case 1:
			System_Options[0] = Options_Sound_Checked;
			break;
	}

	switch(*opt_AutoFrameSkip)
	{
		case 0:
			pNesX_itoa(*opt_FrameSkip, tempbuffer);
			strcpy(Frameskip_Buffer, Options_Frameskip);
			strcat(Frameskip_Buffer, ",");
			strcat(Frameskip_Buffer, tempbuffer);
			strcat(Frameskip_Buffer, "<RLAlign>");
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

	switch(*opt_DiscFormat)
	{
		case 0:
			System_Options[2] = Options_CDFormat_FrNES;
			break;
		case 1:
			System_Options[2] = Options_CDFormat_NesterDC;
			break;
	}

	System_Options[3] = Options_VMU;
	System_Options[4] = Options_Port;
	
	switch(*opt_VMUPort)
	{
		case -1:
			System_Options[5] = Options_CXXX;
			break;
		case 0:
			System_Options[5] = Options_C1S1;
			break;
		case 1:
			System_Options[5] = Options_C1S2;
			break;
		case 2:
			System_Options[5] = Options_C2S1;
			break;
		case 3:
			System_Options[5] = Options_C2S2;
			break;
		case 4:
			System_Options[5] = Options_C3S1;
			break;
		case 5:
			System_Options[5] = Options_C3S2;
			break;
		case 6:
			System_Options[5] = Options_C4S1;
			break;
		case 7:
			System_Options[5] = Options_C4S2;
			break;
	}


	switch(*opt_SRAM)
	{
		case 0:
			System_Options[6] = Options_SRAM_Unchecked;
			break;
		case 1:
			System_Options[6] = Options_SRAM_Checked;
			break;
	}

	System_Options[7] = Options_LoadVMU;
	System_Options[8] = Options_SaveVMU;
	System_Options[9] = Options_Rescan;
}

void Handle_System_Interface(cont_cond_t my_cond)
{
	int i;

	//Down Key Hit and Key is Ready to be hit
	if ((!(my_cond.buttons & CONT_DPAD_DOWN)) && (mydata.Highlighted_Index < Num_System_Options) && (keyhit == 0))
	{
		mydata.Highlighted_Index++;
		if ((mydata.Highlighted_Index - mydata.Top_Index) >= mystyle.Max_Items)
			mydata.Top_Index++;
		keyhit = 1;
		isMainChanged = 1;
	}

	//Up Key Hit and Key is Ready to be hit
	if ((!(my_cond.buttons & CONT_DPAD_UP)) && (mydata.Highlighted_Index > 0) && (keyhit == 0))
	{
		mydata.Highlighted_Index--;
		if (mydata.Top_Index > mydata.Highlighted_Index)
			mydata.Top_Index--;
		keyhit = 1;
		isMainChanged = 1;
	}

	//Handle the toggle boxes
	if (!(my_cond.buttons & CONT_A) && (invalida == 0))
	{
		switch(mydata.Highlighted_Index)
		{
			case 0:
				*opt_SoundEnabled = 1 - (*opt_SoundEnabled);
				Generate_System_Options_List();
				isMainChanged = 1;
				invalida = 1;
				break;
			case 6:
				*opt_SRAM  = 1 - (*opt_SRAM);
				Generate_System_Options_List();
				isMainChanged = 1;
				invalida = 1;
				break;
			// Load From VMU
			case 7:
				Load_VMU_Options();
				invalida = 1;
				break;
			// Save To VMU
			case 8:
				Save_VMU_Options();
				invalida = 1;
				break;
			// Scan Maple Bus
			case 9:
				initialize_controllers();
				rescan_controllers();

				//If the chosen Memory card isn't present
				if (VMUs[*opt_VMUPort] == 0)
				{
					//Choose the first one available
					for (i = 0; i < 8; i++)
					{
						if (VMUs[i] != 0)
						{
							*opt_VMUPort = i;
							break;
						}
						else
							//If one wasn't found, turn off memcard support
							*opt_VMUPort = -1;
					}
				}
				invalida = 1;
				break;
		}
	}

	//Handle the modify boxes
	if (!(my_cond.buttons & CONT_DPAD_LEFT) && (xkeyhit == 0))
	{
		switch(mydata.Highlighted_Index)
		{
			case 1:
				if ((*opt_FrameSkip) > 0)
				{
					(*opt_FrameSkip)--;
					Generate_System_Options_List();
					isMainChanged = 1;
					xkeyhit = 1;
				}
				else
				if (*opt_FrameSkip == 0)
				{
					(*opt_AutoFrameSkip) = 1;
					Generate_System_Options_List();
					isMainChanged = 1;
					xkeyhit = 1;
				}
				break;

			case 2:
				if ((*opt_DiscFormat) > 0)
				{
					(*opt_DiscFormat)--;
					Generate_System_Options_List();
					isMainChanged = 1;
					xkeyhit = 1;
				}
				break;
			case 5:
				if ((*opt_VMUPort) > 0)
				{
					for (i = (*opt_VMUPort) - 1; i >= 0; i--)
						if (VMUs[i] != 0)
						{
							(*opt_VMUPort) = i;
							break;
						}
					Generate_System_Options_List();
					isMainChanged = 1;
					xkeyhit = 1;
				}
				break;
		}
	}					

	if (!(my_cond.buttons & CONT_DPAD_RIGHT) && (xkeyhit == 0))
	{
		switch(mydata.Highlighted_Index)
		{
			case 1:
				if ((*opt_AutoFrameSkip) == 1)
				{
					(*opt_AutoFrameSkip) = 0;
					Generate_System_Options_List();
					isMainChanged = 1;
					xkeyhit = 1;
				}
				else
				if ((*opt_FrameSkip) < Max_Frameskip)
				{
					(*opt_FrameSkip)++;
					Generate_System_Options_List();
					isMainChanged = 1;
					xkeyhit = 1;
				}
				break;

			case 2:
				if ((*opt_DiscFormat) < 1)
				{
					(*opt_DiscFormat)++;
					Generate_System_Options_List();
					isMainChanged = 1;
					xkeyhit = 1;
				}
				break;
			case 5:
				if ((*opt_VMUPort < 7) && (*opt_VMUPort != -1))
				{
					for (i = (*opt_VMUPort) + 1; i < 8; i++)
						if (VMUs[i] != 0)
						{
							*opt_VMUPort = i;
							break;
						}
					Generate_System_Options_List();
					isMainChanged = 1;
					xkeyhit = 1;
				}
				break;
		}
	}					


	// Handle Return to Main Menu
	if (!(my_cond.buttons & CONT_B) && (keyhit == 0))
	{
		setup_main_menu();
		menuscreen = MENUNUM_MAIN;
		isMainChanged = 1;
		isSmallChanged = 1;
	}
}
