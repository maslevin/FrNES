/*===================================================================*/
/*                                                                   */
/*  GUI_GUIPage.c : GUI Page for GUI Customization Settings          */
/*                      Interface, Variables, and Event Handling     */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#include "GUI_GUIPage.h"
#include "macros.h"

#include "pNesX_System_DC.h"

char Options_GUI[] = "GUI Options";
char Options_OutsideWindowColor[] = "Window Border Color";
char Options_InsideWindowColor[] = "Inside Window Color";
char Options_TextColor[] = "Text Color";
char Options_SelectedTextColor[] = "Highlight Color";
char Options_BGColor[] = "Background Color";
char Options_RedComp[] = "Red";
char Options_GreenComp[] = "Green";
char Options_BlueComp[] = "Blue";
char* GUI_Options[39];
const int Num_GUI_Options = 39;

#define SLIDER_STRING_BUFFER_LENGTH 80

char Red_OWC_Buffer[SLIDER_STRING_BUFFER_LENGTH];
char Green_OWC_Buffer[SLIDER_STRING_BUFFER_LENGTH];
char Blue_OWC_Buffer[SLIDER_STRING_BUFFER_LENGTH];

char Red_IWC_Buffer[SLIDER_STRING_BUFFER_LENGTH];
char Green_IWC_Buffer[SLIDER_STRING_BUFFER_LENGTH];
char Blue_IWC_Buffer[SLIDER_STRING_BUFFER_LENGTH];

char Red_TC_Buffer[SLIDER_STRING_BUFFER_LENGTH];
char Green_TC_Buffer[SLIDER_STRING_BUFFER_LENGTH];
char Blue_TC_Buffer[SLIDER_STRING_BUFFER_LENGTH];

char Red_STC_Buffer[SLIDER_STRING_BUFFER_LENGTH];
char Green_STC_Buffer[SLIDER_STRING_BUFFER_LENGTH];
char Blue_STC_Buffer[SLIDER_STRING_BUFFER_LENGTH];

char Red_BG_Buffer[SLIDER_STRING_BUFFER_LENGTH];
char Green_BG_Buffer[SLIDER_STRING_BUFFER_LENGTH];
char Blue_BG_Buffer[SLIDER_STRING_BUFFER_LENGTH];

void setup_gui_options_screen() {
	//Set Up Window Data Features
	mydata.x = 208.0f;
	mydata.y = 32.0f;
	mydata.width = 400.0f;
	mydata.height = 416.0f;
	mydata.font = font;
	mydata.Header_Text = Options_GUI;
	mydata.Data_Strings = GUI_Options;
	mydata.Num_Strings = Num_GUI_Options;
	mydata.Highlighted_Index = 0;
	mydata.Top_Index = 0;

	//Set Up Window Style Features
	mystyle.Header_Text_Scale = 1.0f;
	mystyle.Text_Scale = 0.40f;	
	mystyle.Border_Thickness = 5.0f;
	mystyle.Border_Color = GUI_OutsideWindowColor; //		MakeRGB(8, 20, 10);
	mystyle.Inside_Color = GUI_InsideWindowColor; //MakeRGB(8, 20, 32);
	mystyle.Left_Margin = 15;
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
	helpstyle.Border_Color = GUI_OutsideWindowColor;
	helpstyle.Inside_Color = GUI_InsideWindowColor;
	helpstyle.Left_Margin = 15;
	helpstyle.Header_Text_Color = GUI_TextColor;
	helpstyle.Text_Color = GUI_TextColor;
	helpstyle.Max_Items = 10;
	helpstyle.Selected_Text_Color = GUI_TextColor;
	helpstyle.Selected_Background_Color = GUI_SelectedTextColor;//MakeRGB(31, 18, 8);
}

//Generates the Options GUI info from the variables in memory
void Generate_GUI_Options_List() {
	GUI_Options[0] = Options_OutsideWindowColor;

	GUI_Options[1] = Options_RedComp;
	snprintf(Red_OWC_Buffer, SLIDER_STRING_BUFFER_LENGTH, "<Slider> <SBC_MAX = %u> <SBC_MIN = %u> <SBC_VALUE = %lu>", 31, 0, ((GUI_OutsideWindowColor >> 10) & 0x1F));
	GUI_Options[2] = Red_OWC_Buffer;
	GUI_Options[3] = Options_GreenComp;
	snprintf(Green_OWC_Buffer, SLIDER_STRING_BUFFER_LENGTH, "<Slider> <SBC_MAX = %u> <SBC_MIN = %u> <SBC_VALUE = %lu>", 31, 0, ((GUI_OutsideWindowColor >> 5) & 0x1F));
	GUI_Options[4] = Green_OWC_Buffer;
	GUI_Options[5] = Options_BlueComp;
	snprintf(Blue_OWC_Buffer, SLIDER_STRING_BUFFER_LENGTH, "<Slider> <SBC_MAX = %u> <SBC_MIN = %u> <SBC_VALUE = %lu>", 31, 0, (GUI_OutsideWindowColor & 0x1F));
	GUI_Options[6] = Blue_OWC_Buffer;

	GUI_Options[7] = " ";
	GUI_Options[8] = Options_InsideWindowColor;
	GUI_Options[9] = Options_RedComp;
	snprintf(Red_IWC_Buffer, SLIDER_STRING_BUFFER_LENGTH, "<Slider> <SBC_MAX = %u> <SBC_MIN = %u> <SBC_VALUE = %lu>", 31, 0, ((GUI_InsideWindowColor >> 10) & 0x1F));
	GUI_Options[10] = Red_IWC_Buffer;
	GUI_Options[11] = Options_GreenComp;
	snprintf(Green_IWC_Buffer, SLIDER_STRING_BUFFER_LENGTH, "<Slider> <SBC_MAX = %u> <SBC_MIN = %u> <SBC_VALUE = %lu>", 31, 0, ((GUI_InsideWindowColor >> 5) & 0x1F));
	GUI_Options[12] = Green_IWC_Buffer;
	GUI_Options[13] = Options_BlueComp;
	snprintf(Blue_IWC_Buffer, SLIDER_STRING_BUFFER_LENGTH, "<Slider> <SBC_MAX = %u> <SBC_MIN = %u> <SBC_VALUE = %lu>", 31, 0, (GUI_InsideWindowColor & 0x1F));	
	GUI_Options[14] = Blue_IWC_Buffer;

	GUI_Options[15] = " ";
	GUI_Options[16] = Options_TextColor;
	GUI_Options[17] = Options_RedComp;
	snprintf(Red_TC_Buffer, SLIDER_STRING_BUFFER_LENGTH, "<Slider> <SBC_MAX = %u> <SBC_MIN = %u> <SBC_VALUE = %lu>", 31, 0, ((GUI_TextColor >> 10) & 0x1F));
	GUI_Options[18] = Red_TC_Buffer;
	GUI_Options[19] = Options_GreenComp;
	snprintf(Green_TC_Buffer, SLIDER_STRING_BUFFER_LENGTH, "<Slider> <SBC_MAX = %u> <SBC_MIN = %u> <SBC_VALUE = %lu>", 31, 0, ((GUI_TextColor >> 5) & 0x1F));
	GUI_Options[20] = Green_TC_Buffer;
	GUI_Options[21] = Options_BlueComp;
	snprintf(Blue_TC_Buffer, SLIDER_STRING_BUFFER_LENGTH, "<Slider> <SBC_MAX = %u> <SBC_MIN = %u> <SBC_VALUE = %lu>", 31, 0, (GUI_TextColor & 0x1F));
	GUI_Options[22] = Blue_TC_Buffer;

	GUI_Options[23] = " ";
	GUI_Options[24] = Options_SelectedTextColor;
	GUI_Options[25] = Options_RedComp;
	snprintf(Red_STC_Buffer, SLIDER_STRING_BUFFER_LENGTH, "<Slider> <SBC_MAX = %u> <SBC_MIN = %u> <SBC_VALUE = %lu>", 31, 0, ((GUI_SelectedTextColor >> 10) & 0x1F));
	GUI_Options[26] = Red_STC_Buffer;
	GUI_Options[27] = Options_GreenComp;
	snprintf(Green_STC_Buffer, SLIDER_STRING_BUFFER_LENGTH, "<Slider> <SBC_MAX = %u> <SBC_MIN = %u> <SBC_VALUE = %lu>", 31, 0, ((GUI_SelectedTextColor >> 5) & 0x1F));
	GUI_Options[28] = Green_STC_Buffer;
	GUI_Options[29] = Options_BlueComp;
	snprintf(Blue_STC_Buffer, SLIDER_STRING_BUFFER_LENGTH, "<Slider> <SBC_MAX = %u> <SBC_MIN = %u> <SBC_VALUE = %lu>", 31, 0, (GUI_SelectedTextColor & 0x1F));
	GUI_Options[30] = Blue_STC_Buffer;

	GUI_Options[31] = " ";
	GUI_Options[32] = Options_BGColor;
	GUI_Options[33] = Options_RedComp;
	snprintf(Red_BG_Buffer, SLIDER_STRING_BUFFER_LENGTH, "<Slider> <SBC_MAX = %u> <SBC_MIN = %u> <SBC_VALUE = %lu>", 255, 0, ((GUI_BGColor >> 16) & 0xFF));
	GUI_Options[34] = Red_BG_Buffer;
	GUI_Options[35] = Options_GreenComp;
	snprintf(Green_BG_Buffer, SLIDER_STRING_BUFFER_LENGTH, "<Slider> <SBC_MAX = %u> <SBC_MIN = %u> <SBC_VALUE = %lu>", 255, 0, ((GUI_BGColor >> 8) & 0xFF));
	GUI_Options[36] = Green_BG_Buffer;
	GUI_Options[37] = Options_BlueComp;
	snprintf(Blue_BG_Buffer, SLIDER_STRING_BUFFER_LENGTH, "<Slider> <SBC_MAX = %u> <SBC_MIN = %u> <SBC_VALUE = %lu>", 255, 0, (GUI_BGColor & 0xFF));
	GUI_Options[38] = Blue_BG_Buffer;
}

void Handle_GUI_Interface(cont_state_t* my_state) {
	uint16 temp;

	//Down Key Hit and Key is Ready to be hit
	if ((my_state -> buttons & CONT_DPAD_DOWN) && 
		(mydata.Highlighted_Index < Num_GUI_Options) && 
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

	//Handle the modify boxes
	if ((my_state -> buttons & CONT_DPAD_LEFT) && 
		(xkeyhit == 0)) {
		switch(mydata.Highlighted_Index) {
			case 2:
				temp = ((GUI_OutsideWindowColor >> 10) & 0x1F);
				if (temp > 0)
				{
					temp--;
					GUI_OutsideWindowColor = (GUI_OutsideWindowColor & 0x83FF) | (temp << 10);
					Generate_GUI_Options_List();
					mystyle.Border_Color = GUI_OutsideWindowColor;
					helpstyle.Border_Color = GUI_OutsideWindowColor;
				}
				break;
			case 4:
				temp = ((GUI_OutsideWindowColor >> 5) & 0x1F);
				if (temp > 0)
				{
					temp--;
					GUI_OutsideWindowColor = (GUI_OutsideWindowColor & 0xFC1F) | (temp << 5);
					Generate_GUI_Options_List();
					mystyle.Border_Color = GUI_OutsideWindowColor;
					helpstyle.Border_Color = GUI_OutsideWindowColor;
				}
				break;
			case 6:
				temp = (GUI_OutsideWindowColor & 0x1F);
				if (temp > 0)
				{
					temp--;
					GUI_OutsideWindowColor = (GUI_OutsideWindowColor & 0xFFE0) | (temp);
					Generate_GUI_Options_List();
					mystyle.Border_Color = GUI_OutsideWindowColor;
					helpstyle.Border_Color = GUI_OutsideWindowColor;
				}
				break;
			case 10:
				temp = ((GUI_InsideWindowColor >> 10) & 0x1F);
				if (temp > 0)
				{
					temp--;
					GUI_InsideWindowColor = (GUI_InsideWindowColor & 0x83FF) | (temp << 10);
					Generate_GUI_Options_List();
					mystyle.Inside_Color = GUI_InsideWindowColor;
					helpstyle.Inside_Color = GUI_InsideWindowColor;
				}
				break;
			case 12:
				temp = ((GUI_InsideWindowColor >> 5) & 0x1F);
				if (temp > 0)
				{
					temp--;
					GUI_InsideWindowColor = (GUI_InsideWindowColor & 0xFC1F) | (temp << 5);
					Generate_GUI_Options_List();
					mystyle.Inside_Color = GUI_InsideWindowColor;
					helpstyle.Inside_Color = GUI_InsideWindowColor;
				}
				break;
			case 14:
				temp = (GUI_InsideWindowColor & 0x1F);
				if (temp > 0)
				{
					temp--;
					GUI_InsideWindowColor = (GUI_InsideWindowColor & 0xFFE0) | (temp);
					Generate_GUI_Options_List();
					mystyle.Inside_Color = GUI_InsideWindowColor;
					helpstyle.Inside_Color = GUI_InsideWindowColor;
				}
				break;

			case 18:
				temp = ((GUI_TextColor >> 10) & 0x1F);
				if (temp > 0)
				{
					temp--;
					GUI_TextColor = (GUI_TextColor & 0x83FF) | (temp << 10);
					Generate_GUI_Options_List();
					mystyle.Text_Color = GUI_TextColor;
					helpstyle.Text_Color = GUI_TextColor;
					mystyle.Header_Text_Color = GUI_TextColor; //0x8000;
					helpstyle.Header_Text_Color = GUI_TextColor; //0x8000;
				}
				break;
			case 20:
				temp = ((GUI_TextColor >> 5) & 0x1F);
				if (temp > 0)
				{
					temp--;
					GUI_TextColor = (GUI_TextColor & 0xFC1F) | (temp << 5);
					Generate_GUI_Options_List();
					mystyle.Text_Color = GUI_TextColor;
					helpstyle.Text_Color = GUI_TextColor;
					mystyle.Header_Text_Color = GUI_TextColor; //0x8000;
					helpstyle.Header_Text_Color = GUI_TextColor; //0x8000;
				}
				break;
			case 22:
				temp = (GUI_TextColor & 0x1F);
				if (temp > 0)
				{
					temp--;
					GUI_TextColor = (GUI_TextColor & 0xFFE0) | (temp);
					Generate_GUI_Options_List();
					mystyle.Text_Color = GUI_TextColor;
					helpstyle.Text_Color = GUI_TextColor;
					mystyle.Header_Text_Color = GUI_TextColor; //0x8000;
					helpstyle.Header_Text_Color = GUI_TextColor; //0x8000;
				}
				break;

			case 26:
				temp = ((GUI_SelectedTextColor >> 10) & 0x1F);
				if (temp > 0)
				{
					temp--;
					GUI_SelectedTextColor = (GUI_SelectedTextColor & 0x83FF) | (temp << 10);
					Generate_GUI_Options_List();
					mystyle.Selected_Background_Color = GUI_SelectedTextColor;
					helpstyle.Selected_Background_Color = GUI_SelectedTextColor;
				}
				break;
			case 28:
				temp = ((GUI_SelectedTextColor >> 5) & 0x1F);
				if (temp > 0)
				{
					temp--;
					GUI_SelectedTextColor = (GUI_SelectedTextColor & 0xFC1F) | (temp << 5);
					Generate_GUI_Options_List();
					mystyle.Selected_Background_Color = GUI_SelectedTextColor;
					helpstyle.Selected_Background_Color = GUI_SelectedTextColor;
				}
				break;
			case 30:
				temp = (GUI_SelectedTextColor & 0x1F);
				if (temp > 0)
				{
					temp--;
					GUI_SelectedTextColor = (GUI_SelectedTextColor & 0xFFE0) | (temp);
					Generate_GUI_Options_List();
					mystyle.Selected_Background_Color = GUI_SelectedTextColor;
					helpstyle.Selected_Background_Color = GUI_SelectedTextColor;
				}
				break;

			case 34:
				temp = ((GUI_BGColor >> 16) & 0xFF);
				if (temp > 0)
				{
					temp--;
					GUI_BGColor = (GUI_BGColor & 0x0000FFFF) | (temp << 16);
					Generate_GUI_Options_List();
				}
				break;
			case 36:
				temp = ((GUI_BGColor >> 8) & 0xFF);
				if (temp > 0)
				{
					temp--;
					GUI_BGColor = (GUI_BGColor & 0x00FF00FF) | (temp << 8);
					Generate_GUI_Options_List();
				}
				break;
			case 38:
				temp = (GUI_BGColor & 0xFF);
				if (temp > 0)
				{
					temp--;
					GUI_BGColor = (GUI_BGColor & 0x00FFFF00) | (temp);
					Generate_GUI_Options_List();
				}
				break;
		}
	}					

	if ((my_state -> buttons & CONT_DPAD_RIGHT) && 
		(xkeyhit == 0)) {
		switch(mydata.Highlighted_Index)
		{
			case 2:
				temp = ((GUI_OutsideWindowColor >> 10) & 0x1F);
				if (temp < 31)
				{
					temp++;
					GUI_OutsideWindowColor = (GUI_OutsideWindowColor & 0x83FF) | (temp << 10);
					Generate_GUI_Options_List();
					mystyle.Border_Color = GUI_OutsideWindowColor;
					helpstyle.Border_Color = GUI_OutsideWindowColor;
				}
				break;
			case 4:
				temp = ((GUI_OutsideWindowColor >> 5) & 0x1F);
				if (temp < 31)
				{
					temp++;
					GUI_OutsideWindowColor = (GUI_OutsideWindowColor & 0xFC1F) | (temp << 5);
					Generate_GUI_Options_List();
					mystyle.Border_Color = GUI_OutsideWindowColor;
					helpstyle.Border_Color = GUI_OutsideWindowColor;
				}
				break;
			case 6:
				temp = (GUI_OutsideWindowColor & 0x1F);
				if (temp < 31)
				{
					temp++;
					GUI_OutsideWindowColor = (GUI_OutsideWindowColor & 0xFFE0) | (temp);
					Generate_GUI_Options_List();
					mystyle.Border_Color = GUI_OutsideWindowColor;
					helpstyle.Border_Color = GUI_OutsideWindowColor;
				}
				break;

			case 10:
				temp = ((GUI_InsideWindowColor >> 10) & 0x1F);
				if (temp < 31)
				{
					temp++;
					GUI_InsideWindowColor = (GUI_InsideWindowColor & 0x83FF) | (temp << 10);
					Generate_GUI_Options_List();
					mystyle.Inside_Color = GUI_InsideWindowColor;
					helpstyle.Inside_Color = GUI_InsideWindowColor;
				}
				break;
			case 12:
				temp = ((GUI_InsideWindowColor >> 5) & 0x1F);
				if (temp < 31)
				{
					temp++;
					GUI_InsideWindowColor = (GUI_InsideWindowColor & 0xFC1F) | (temp << 5);
					Generate_GUI_Options_List();
					mystyle.Inside_Color = GUI_InsideWindowColor;
					helpstyle.Inside_Color = GUI_InsideWindowColor;
				}
				break;
			case 14:
				temp = (GUI_InsideWindowColor & 0x1F);
				if (temp < 31)
				{
					temp++;
					GUI_InsideWindowColor = (GUI_InsideWindowColor & 0xFFE0) | (temp);
					Generate_GUI_Options_List();
					mystyle.Inside_Color = GUI_InsideWindowColor;
					helpstyle.Inside_Color = GUI_InsideWindowColor;
				}
				break;

			case 18:
				temp = ((GUI_TextColor >> 10) & 0x1F);
				if (temp < 31)
				{
					temp++;
					GUI_TextColor = (GUI_TextColor & 0x83FF) | (temp << 10);
					Generate_GUI_Options_List();
					mystyle.Text_Color = GUI_TextColor;
					helpstyle.Text_Color = GUI_TextColor;
					mystyle.Header_Text_Color = GUI_TextColor; //0x8000;
					helpstyle.Header_Text_Color = GUI_TextColor; //0x8000;
				}
				break;
			case 20:
				temp = ((GUI_TextColor >> 5) & 0x1F);
				if (temp < 31)
				{
					temp++;
					GUI_TextColor = (GUI_TextColor & 0xFC1F) | (temp << 5);
					Generate_GUI_Options_List();
					mystyle.Text_Color = GUI_TextColor;
					helpstyle.Text_Color = GUI_TextColor;
					mystyle.Header_Text_Color = GUI_TextColor; //0x8000;
					helpstyle.Header_Text_Color = GUI_TextColor; //0x8000;
				}
				break;
			case 22:
				temp = (GUI_TextColor & 0x1F);
				if (temp < 31)
				{
					temp++;
					GUI_TextColor = (GUI_TextColor & 0xFFE0) | (temp);
					Generate_GUI_Options_List();
					mystyle.Text_Color = GUI_TextColor;
					helpstyle.Text_Color = GUI_TextColor;
					mystyle.Header_Text_Color = GUI_TextColor; //0x8000;
					helpstyle.Header_Text_Color = GUI_TextColor; //0x8000;
				}
				break;

			case 26:
				temp = ((GUI_SelectedTextColor >> 10) & 0x1F);
				if (temp < 31)
				{
					temp++;
					GUI_SelectedTextColor = (GUI_SelectedTextColor & 0x83FF) | (temp << 10);
					Generate_GUI_Options_List();
					mystyle.Selected_Background_Color = GUI_SelectedTextColor;
					helpstyle.Selected_Background_Color = GUI_SelectedTextColor;
				}
				break;
			case 28:
				temp = ((GUI_SelectedTextColor >> 5) & 0x1F);
				if (temp < 31)
				{
					temp++;
					GUI_SelectedTextColor = (GUI_SelectedTextColor & 0xFC1F) | (temp << 5);
					Generate_GUI_Options_List();
					mystyle.Selected_Background_Color = GUI_SelectedTextColor;
					helpstyle.Selected_Background_Color = GUI_SelectedTextColor;
				}
				break;
			case 30:
				temp = (GUI_SelectedTextColor & 0x1F);
				if (temp < 31)
				{
					temp++;
					GUI_SelectedTextColor = (GUI_SelectedTextColor & 0xFFE0) | (temp);
					Generate_GUI_Options_List();
					mystyle.Selected_Background_Color = GUI_SelectedTextColor;
					helpstyle.Selected_Background_Color = GUI_SelectedTextColor;
				}
				break;

			case 34:
				temp = ((GUI_BGColor >> 16) & 0xFF);
				if (temp < 255)
				{
					temp++;
					GUI_BGColor = (GUI_BGColor & 0x0000FFFF) | (temp << 16);
					Generate_GUI_Options_List();
				}
				break;
			case 36:
				temp = ((GUI_BGColor >> 8) & 0xFF);
				if (temp < 255)
				{
					temp++;
					GUI_BGColor = (GUI_BGColor & 0x00FF00FF) | (temp << 8);
					Generate_GUI_Options_List();
				}
				break;
			case 38:
				temp = (GUI_BGColor & 0xFF);
				if (temp < 255)
				{
					temp++;
					GUI_BGColor = (GUI_BGColor & 0x00FFFF00) | (temp);
					Generate_GUI_Options_List();
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

