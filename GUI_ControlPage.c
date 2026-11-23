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

char Player_1_Option_A_Buffer[50];
char Player_1_Option_B_Buffer[50];
char Player_1_Option_Select_Buffer[50];

char Player_2_Option_A_Buffer[50];
char Player_2_Option_B_Buffer[50];
char Player_2_Option_Select_Buffer[50];

char Player_3_Option_A_Buffer[50];
char Player_3_Option_B_Buffer[50];
char Player_3_Option_Select_Buffer[50];

char Player_4_Option_A_Buffer[50];
char Player_4_Option_B_Buffer[50];
char Player_4_Option_Select_Buffer[50];

//Control Options Text
const char Options_Control[] = "Control Options";
const char Options_P1[] = "1st Controller";
const char Options_P2[] = "2nd Controller";
const char Options_P3[] = "3rd Controller";
const char Options_P4[] = "4th Controller";
const char Options_Analog_Checked[] = "<Check>Enable Analog,CBC_CHECKED";
const char Options_Analog_Unchecked[] = "<Check>Enable Analog,CBC_UNCHECKED";
const char Options_A_Button[] = "A Button";
const char Options_B_Button[] = "B Button";
const char Options_Select_Button[] = "Select";
const char Options_A[] = "A";
const char Options_B[] = "B";
const char Options_X[] = "X";
const char Options_Y[] = "Y";
const char Options_LTrig[] = "Left Trigger";

char* Control_Options[20];
const int Num_Control_Options = 20;

const char* ButtonOptions[5] = {
	Options_A,
	Options_B,
	Options_X,
	Options_Y,
	Options_LTrig
};

void Generate_Control_Options_List() {
	Control_Options[0] = Options_P1;

	Control_Options[1] = options.controllerSettings[0].analogEnabled ?
		Options_Analog_Checked :
 		Options_Analog_Unchecked;

	snprintf(Player_1_Option_Select_Buffer, 50, "%s,%s<RLAlign>", Options_Select_Button, ButtonOptions[options.controllerSettings[0].selectKey]);
	Control_Options[2] = Player_1_Option_Select_Buffer;

	snprintf(Player_1_Option_A_Buffer, 50, "%s,%s<RLAlign>", Options_A_Button, ButtonOptions[options.controllerSettings[0].aKey]);
	Control_Options[3] = Player_1_Option_A_Buffer;	

	snprintf(Player_1_Option_B_Buffer, 50, "%s,%s<RLAlign>", Options_B_Button, ButtonOptions[options.controllerSettings[0].bKey]);
	Control_Options[4] = Player_1_Option_B_Buffer;

	Control_Options[5] = Options_P2;

	Control_Options[6] = options.controllerSettings[1].analogEnabled ?
		Options_Analog_Checked :
 		Options_Analog_Unchecked;

	snprintf(Player_2_Option_Select_Buffer, 50, "%s,%s<RLAlign>", Options_Select_Button, ButtonOptions[options.controllerSettings[1].selectKey]);
	Control_Options[7] = Player_2_Option_Select_Buffer;

	snprintf(Player_2_Option_A_Buffer, 50, "%s,%s<RLAlign>", Options_A_Button, ButtonOptions[options.controllerSettings[1].aKey]);
	Control_Options[8] = Player_2_Option_A_Buffer;	

	snprintf(Player_2_Option_B_Buffer, 50, "%s,%s<RLAlign>", Options_B_Button, ButtonOptions[options.controllerSettings[1].bKey]);
	Control_Options[9] = Player_2_Option_B_Buffer;

	Control_Options[10] = Options_P3;

	Control_Options[11] = options.controllerSettings[2].analogEnabled ?
		Options_Analog_Checked :
 		Options_Analog_Unchecked;

	snprintf(Player_3_Option_Select_Buffer, 50, "%s,%s<RLAlign>", Options_Select_Button, ButtonOptions[options.controllerSettings[2].selectKey]);
	Control_Options[12] = Player_3_Option_Select_Buffer;

	snprintf(Player_3_Option_A_Buffer, 50, "%s,%s<RLAlign>", Options_A_Button, ButtonOptions[options.controllerSettings[2].aKey]);
	Control_Options[13] = Player_3_Option_A_Buffer;	

	snprintf(Player_3_Option_B_Buffer, 50, "%s,%s<RLAlign>", Options_B_Button, ButtonOptions[options.controllerSettings[2].bKey]);
	Control_Options[14] = Player_3_Option_B_Buffer;

	Control_Options[15] = Options_P4;

	Control_Options[16] = options.controllerSettings[3].analogEnabled ?
		Options_Analog_Checked :
 		Options_Analog_Unchecked;

	snprintf(Player_4_Option_Select_Buffer, 50, "%s,%s<RLAlign>", Options_Select_Button, ButtonOptions[options.controllerSettings[3].selectKey]);
	Control_Options[17] = Player_4_Option_Select_Buffer;

	snprintf(Player_4_Option_A_Buffer, 50, "%s,%s<RLAlign>", Options_A_Button, ButtonOptions[options.controllerSettings[3].aKey]);
	Control_Options[18] = Player_4_Option_A_Buffer;	

	snprintf(Player_4_Option_B_Buffer, 50, "%s,%s<RLAlign>", Options_B_Button, ButtonOptions[options.controllerSettings[3].bKey]);
	Control_Options[19] = Player_4_Option_B_Buffer;
}

void setup_control_options_screen() {
	//Set Up Window Data Features
	mydata.x = 208.0f;
	mydata.y = 32.0f;
	mydata.width = 400.0f;
	mydata.height = 416.0f;
	mydata.font = font;
	mydata.Header_Text = Options_Control;
	mydata.Data_Strings = Control_Options;
	mydata.Num_Strings = Num_Control_Options;
	mydata.Highlighted_Index = 1;
	mydata.Top_Index = 0;

	//Set Up Window Style Features
	mystyle.Header_Text_Scale = 1.0f;
	mystyle.Text_Scale = 0.50f;		
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

void Handle_Control_Interface(cont_state_t* my_state) {
	//Down Key Hit and Key is Ready to be hit
	if ((my_state -> buttons & CONT_DPAD_DOWN) && 
		(mydata.Highlighted_Index < (Num_Control_Options - 1)) && 
		(keyhit == 0)) {
		mydata.Highlighted_Index++;
		if (mydata.Highlighted_Index % 5 == 0) {
			mydata.Highlighted_Index++;
		}
		if ((mydata.Highlighted_Index - mydata.Top_Index) >= mystyle.Max_Items)
			mydata.Top_Index++;
		keyhit = 1;
	}

	//Up Key Hit and Key is Ready to be hit
	if ((my_state -> buttons & CONT_DPAD_UP) && 
		(mydata.Highlighted_Index > 1) && 
		(keyhit == 0)) {
		mydata.Highlighted_Index--;
		if (mydata.Highlighted_Index % 5 == 0) {
			mydata.Highlighted_Index--;
		}		
		if (mydata.Top_Index > mydata.Highlighted_Index)
			mydata.Top_Index--;
		keyhit = 1;
	}

	//Handle the toggle boxes
	if ((my_state -> buttons & CONT_A) && 
		(invalida == 0)) {
		switch(mydata.Highlighted_Index) {
			case 1:
				options.controllerSettings[0].analogEnabled = !options.controllerSettings[0].analogEnabled;
				Generate_Control_Options_List();
				invalida = 1;
				break;
			case 6:
				options.controllerSettings[1].analogEnabled = !options.controllerSettings[1].analogEnabled;
				Generate_Control_Options_List();
				invalida = 1;				
				break;
			case 11:
				options.controllerSettings[2].analogEnabled = !options.controllerSettings[2].analogEnabled;
				Generate_Control_Options_List();
				invalida = 1;				
				break;			
			case 16:
				options.controllerSettings[3].analogEnabled = !options.controllerSettings[3].analogEnabled;
				Generate_Control_Options_List();
				invalida = 1;				
				break;
		}
	}

	//Handle the modify boxes
	if ((my_state -> buttons & CONT_DPAD_LEFT) && 
		(xkeyhit == 0)) {
		switch(mydata.Highlighted_Index) {
			case 2:
				if (options.controllerSettings[0].selectKey > 0) {
					options.controllerSettings[0].selectKey--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 3:
				if (options.controllerSettings[0].aKey > 0) {
					options.controllerSettings[0].aKey--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 4:
				if (options.controllerSettings[0].bKey > 0) {
					options.controllerSettings[0].bKey--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 7:
				if (options.controllerSettings[1].selectKey > 0) {
					options.controllerSettings[1].selectKey--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 8:
				if (options.controllerSettings[1].aKey > 0) {
					options.controllerSettings[1].aKey--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 9:
				if (options.controllerSettings[1].bKey > 0) {
					options.controllerSettings[1].bKey--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 12:
				if (options.controllerSettings[2].selectKey > 0) {
					options.controllerSettings[2].selectKey--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 13:
				if (options.controllerSettings[2].aKey > 0) {
					options.controllerSettings[2].aKey--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 14:
				if (options.controllerSettings[2].bKey > 0) {
					options.controllerSettings[2].bKey--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 17:
				if (options.controllerSettings[3].selectKey > 0) {
					options.controllerSettings[3].selectKey--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 18:
				if (options.controllerSettings[3].aKey > 0) {
					options.controllerSettings[3].aKey--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 19:
				if (options.controllerSettings[3].bKey > 0) {
					options.controllerSettings[3].bKey--;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
		}
	}					

	if ((my_state -> buttons & CONT_DPAD_RIGHT) && 
		(xkeyhit == 0)) {
		switch(mydata.Highlighted_Index) {
			case 2:
				if (options.controllerSettings[0].selectKey < 4) {
					options.controllerSettings[0].selectKey++;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 3:
				if (options.controllerSettings[0].aKey < 4) {
					options.controllerSettings[0].aKey++;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 4:
				if (options.controllerSettings[0].bKey < 4) {
					options.controllerSettings[0].bKey++;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 7:
				if (options.controllerSettings[1].selectKey < 4) {
					options.controllerSettings[1].selectKey++;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 8:
				if (options.controllerSettings[1].aKey < 4) {
					options.controllerSettings[1].aKey++;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 9:
				if (options.controllerSettings[1].bKey < 4) {
					options.controllerSettings[1].bKey++;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 12:
				if (options.controllerSettings[2].selectKey < 4) {
					options.controllerSettings[2].selectKey++;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 13:
				if (options.controllerSettings[2].aKey < 4) {
					options.controllerSettings[2].aKey++;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 14:
				if (options.controllerSettings[2].bKey < 4) {
					options.controllerSettings[2].bKey++;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 17:
				if (options.controllerSettings[3].selectKey < 4) {
					options.controllerSettings[3].selectKey++;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 18:
				if (options.controllerSettings[3].aKey < 4) {
					options.controllerSettings[3].aKey++;
					Generate_Control_Options_List();
					xkeyhit = 1;
				}
				break;
			case 19:
				if (options.controllerSettings[3].bKey < 4) {
					options.controllerSettings[3].bKey++;
					Generate_Control_Options_List();
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