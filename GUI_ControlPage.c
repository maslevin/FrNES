/*===================================================================*/
/*                                                                   */
/*  GUI_ControlPage.c : GUI Page for Control Settings                */
/*                      Interface, Variables, and Event Handling     */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#include "GUI_ControlPage.h"
#include "macros.h"

#include "pNesX_System_DC.h"

//Control Options Text
char Options_Control[] = "Control Options";
char Options_P1Pad[] = "1st Pad";
char Options_P2Pad[] = "2nd Pad";
char Options_Analog_Checked[] = "<Check>Enable Analog,CBC_CHECKED";
char Options_Analog_Unchecked[] = "<Check>Enable Analog,CBC_UNCHECKED";
char Options_Select_A[] = "Select - A";
char Options_Select_B[] = "Select - B";
char Options_Select_X[] = "Select - X";
char Options_Select_Y[] = "Select - Y";
char Options_Select_LTrig[] = "Select - LTrig";
char Options_A_A[] = "NES A - A";
char Options_A_B[] = "NES A - B";
char Options_A_X[] = "NES A - X";
char Options_A_Y[] = "NES A - Y";
char Options_A_LTrig[] = "NES A - LTrig";
char Options_B_A[] = "NES B - A";
char Options_B_B[] = "NES B - B";
char Options_B_X[] = "NES B - X";
char Options_B_Y[] = "NES B - Y";
char Options_B_LTrig[] = "NES B - LTrig";
char* Control_Options[11];
const int Num_Control_Options = 11;

uint16* opt_P1AnalogEnabled;
uint16* opt_P2AnalogEnabled;
uint16* opt_P1SelectKey;
uint16* opt_P1AKey;
uint16* opt_P1BKey;
uint16* opt_P2SelectKey;
uint16* opt_P2AKey;
uint16* opt_P2BKey;

void Allocate_Control_Options()
{
	opt_P1AnalogEnabled = malloc(sizeof(uint16));
	opt_P2AnalogEnabled = malloc(sizeof(uint16));
	opt_P1SelectKey = malloc(sizeof(uint16));
	opt_P2SelectKey = malloc(sizeof(uint16));
	opt_P1AKey = malloc(sizeof(uint16));
	opt_P1BKey = malloc(sizeof(uint16));
	opt_P2AKey = malloc(sizeof(uint16));
	opt_P2BKey = malloc(sizeof(uint16));
}

void Free_Control_Options()
{
	free(opt_P1AnalogEnabled);
	free(opt_P2AnalogEnabled);
	free(opt_P1SelectKey);
	free(opt_P2SelectKey);
	free(opt_P1AKey);
	free(opt_P1BKey);
	free(opt_P2AKey);
	free(opt_P2BKey);
}

void Generate_Control_Options_List()
{
	Control_Options[0] = Options_P1Pad;

	switch(*opt_P1AnalogEnabled)
	{
		case 0:
			Control_Options[1] = Options_Analog_Unchecked;
			break;
		case 1:
			Control_Options[1] = Options_Analog_Checked;
			break;
	}
	switch(*opt_P1SelectKey)
	{
		case 0:
			Control_Options[2] = Options_Select_A;
			break;
		case 1:
			Control_Options[2] = Options_Select_B;
			break;
		case 2:
			Control_Options[2] = Options_Select_X;
			break;
		case 3:
			Control_Options[2] = Options_Select_Y;
			break;
		case 4:
			Control_Options[2] = Options_Select_LTrig;
			break;
	}
	switch(*opt_P1AKey)
	{
		case 0:
			Control_Options[3] = Options_A_A;
			break;
		case 1:
			Control_Options[3] = Options_A_B;
			break;
		case 2:
			Control_Options[3] = Options_A_X;
			break;
		case 3:
			Control_Options[3] = Options_A_Y;
			break;
		case 4:
			Control_Options[3] = Options_A_LTrig;
			break;
	}
	switch(*opt_P1BKey)
	{
		case 0:
			Control_Options[4] = Options_B_A;
			break;
		case 1:
			Control_Options[4] = Options_B_B;
			break;
		case 2:
			Control_Options[4] = Options_B_X;
			break;
		case 3:
			Control_Options[4] = Options_B_Y;
			break;
		case 4:
			Control_Options[4] = Options_B_LTrig;
			break;
	}

	Control_Options[5] = " ";

	Control_Options[6] = Options_P2Pad;

	switch(*opt_P2AnalogEnabled)
	{
		case 0:
			Control_Options[7] = Options_Analog_Unchecked;
			break;
		case 1:
			Control_Options[7] = Options_Analog_Checked;
			break;
	}
	switch(*opt_P2SelectKey)
	{
		case 0:
			Control_Options[8] = Options_Select_A;
			break;
		case 1:
			Control_Options[8] = Options_Select_B;
			break;
		case 2:
			Control_Options[8] = Options_Select_X;
			break;
		case 3:
			Control_Options[8] = Options_Select_Y;
			break;
		case 4:
			Control_Options[8] = Options_Select_LTrig;
			break;
	}
	switch(*opt_P2AKey)
	{
		case 0:
			Control_Options[9] = Options_A_A;
			break;
		case 1:
			Control_Options[9] = Options_A_B;
			break;
		case 2:
			Control_Options[9] = Options_A_X;
			break;
		case 3:
			Control_Options[9] = Options_A_Y;
			break;
		case 4:
			Control_Options[9] = Options_A_LTrig;
			break;
	}
	switch(*opt_P2BKey)
	{
		case 0:
			Control_Options[10] = Options_B_A;
			break;
		case 1:
			Control_Options[10] = Options_B_B;
			break;
		case 2:
			Control_Options[10] = Options_B_X;
			break;
		case 3:
			Control_Options[10] = Options_B_Y;
			break;
		case 4:
			Control_Options[10] = Options_B_LTrig;
			break;
	}
}

void setup_control_options_screen()
{
	//Set Up Window Data Features
	mydata.x = 208.0f;
	mydata.y = 32.0f;
	mydata.width = 400.0f;
	mydata.height = 416.0f;
	mydata.font = font;
	mydata.Header_Text = Options_Control;
	mydata.Data_Strings = Control_Options;
	mydata.Num_Strings = Num_Control_Options;
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

void Handle_Control_Interface(cont_state_t* my_state)
{
	//Down Key Hit and Key is Ready to be hit
	if ((my_state -> buttons & CONT_DPAD_DOWN) && 
		(mydata.Highlighted_Index < Num_Control_Options) && 
		(keyhit == 0))
	{
		mydata.Highlighted_Index++;
		if ((mydata.Highlighted_Index - mydata.Top_Index) >= mystyle.Max_Items)
			mydata.Top_Index++;
		keyhit = 1;
	}

	//Up Key Hit and Key is Ready to be hit
	if ((my_state -> buttons & CONT_DPAD_UP) && 
		(mydata.Highlighted_Index > 0) && 
		(keyhit == 0))
	{
		mydata.Highlighted_Index--;
		if (mydata.Top_Index > mydata.Highlighted_Index)
			mydata.Top_Index--;
		keyhit = 1;
	}

	//Handle the toggle boxes
	if ((my_state -> buttons & CONT_A) && 
		(invalida == 0))
	{
		switch(mydata.Highlighted_Index)
		{
			case 1:
				*opt_P1AnalogEnabled = 1 - *opt_P1AnalogEnabled;
				Generate_Control_Options_List();
				invalida = 1;
				break;
			case 7:
				*opt_P2AnalogEnabled = 1 - *opt_P2AnalogEnabled;
				Generate_Control_Options_List();
				invalida = 1;
				break;
		}
	}

	//Handle the modify boxes
	if ((my_state -> buttons & CONT_DPAD_LEFT) && 
		(xkeyhit == 0))
	{
		switch(mydata.Highlighted_Index)
		{
			case 2:
				if (*opt_P1SelectKey > 0)
				{
					(*opt_P1SelectKey)--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 3:
				if (*opt_P1AKey > 0)
				{
					(*opt_P1AKey)--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 4:
				if (*opt_P1BKey > 0)
				{
					(*opt_P1BKey)--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 8:
				if (*opt_P2SelectKey > 0)
				{
					(*opt_P2SelectKey)--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 9:
				if (*opt_P2AKey > 0)
				{
					(*opt_P2AKey)--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 10:
				if (*opt_P2BKey > 0)
				{
					(*opt_P2BKey)--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
		}
	}					

	if ((my_state -> buttons & CONT_DPAD_RIGHT) && 
		(xkeyhit == 0))
	{
		switch(mydata.Highlighted_Index)
		{
			case 2:
				if (*opt_P1SelectKey < 4)
				{
					(*opt_P1SelectKey)++;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 3:
				if (*opt_P1AKey < 4)
				{
					(*opt_P1AKey)++;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 4:
				if (*opt_P1BKey < 4)
				{
					(*opt_P1BKey)++;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 8:
				if (*opt_P2SelectKey < 4)
				{
					(*opt_P2SelectKey)++;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 9:
				if (*opt_P2AKey < 4)
				{
					(*opt_P2AKey)++;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 10:
				if (*opt_P2BKey < 4)
				{
					(*opt_P2BKey)++;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
		}
	}					

	// Handle Return to Main Menu
	if ((my_state -> buttons & CONT_B) && 
		(keyhit == 0))
	{
		setup_main_menu_screen();
		menuscreen = MENUNUM_MAIN;
	}
}