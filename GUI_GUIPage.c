/*===================================================================*/
/*                                                                   */
/*  GUI_GUIPage.c : GUI Page for GUI Customization Settings          */
/*                      Interface, Variables, and Event Handling     */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#include "GUI_GUIPage.h"
#include "pNesX_System_DC.h"
#include "pNesX_Utils.h"

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

char Red_OWC_Buffer[50];
char Green_OWC_Buffer[50];
char Blue_OWC_Buffer[50];

char Red_IWC_Buffer[50];
char Green_IWC_Buffer[50];
char Blue_IWC_Buffer[50];

char Red_TC_Buffer[50];
char Green_TC_Buffer[50];
char Blue_TC_Buffer[50];

char Red_STC_Buffer[50];
char Green_STC_Buffer[50];
char Blue_STC_Buffer[50];

char Red_BG_Buffer[75];
char Green_BG_Buffer[75];
char Blue_BG_Buffer[75];

void setup_gui_options_screen()
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
	mydata.Header_Text = Options_GUI;
	mydata.Data_Strings = GUI_Options;
	mydata.Num_Strings = Num_GUI_Options;
	mydata.Highlighted_Index = 0;
	mydata.Top_Index = 0;
	mydata.Target_Buffer = (uint16*)PVR_MainWindow_Offset;

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
	helpdata.Target_Buffer = (uint16*)PVR_SmallWindow_Offset;
}

//Generates the Options GUI info from the variables in memory
void Generate_GUI_Options_List()
{
	char numbuffer[16];

	GUI_Options[0] = Options_OutsideWindowColor;

	GUI_Options[1] = Options_RedComp;
	strcpy(Red_OWC_Buffer, Tag_Slider);
	strcat(Red_OWC_Buffer, SLIDERBOX_MAX);
	strcat(Red_OWC_Buffer, "31>");
	strcat(Red_OWC_Buffer, SLIDERBOX_MIN);
	strcat(Red_OWC_Buffer, "0>");
	strcat(Red_OWC_Buffer, SLIDERBOX_VALUE);
	pNesX_itoa(((GUI_OutsideWindowColor >> 10) & 0x1F), numbuffer);
	strcat(Red_OWC_Buffer, numbuffer);
	strcat(Red_OWC_Buffer, ">");
	GUI_Options[2] = Red_OWC_Buffer;
	GUI_Options[3] = Options_GreenComp;
	strcpy(Green_OWC_Buffer, Tag_Slider);
	strcat(Green_OWC_Buffer, SLIDERBOX_MAX);
	strcat(Green_OWC_Buffer, "31>");
	strcat(Green_OWC_Buffer, SLIDERBOX_MIN);
	strcat(Green_OWC_Buffer, "0>");
	strcat(Green_OWC_Buffer, SLIDERBOX_VALUE);
	pNesX_itoa(((GUI_OutsideWindowColor >> 5) & 0x1F), numbuffer);
	strcat(Green_OWC_Buffer, numbuffer);
	strcat(Green_OWC_Buffer, ">");
	GUI_Options[4] = Green_OWC_Buffer;
	GUI_Options[5] = Options_BlueComp;
	strcpy(Blue_OWC_Buffer, Tag_Slider);
	strcat(Blue_OWC_Buffer, SLIDERBOX_MAX);
	strcat(Blue_OWC_Buffer, "31>");
	strcat(Blue_OWC_Buffer, SLIDERBOX_MIN);
	strcat(Blue_OWC_Buffer, "0>");
	strcat(Blue_OWC_Buffer, SLIDERBOX_VALUE);
	pNesX_itoa((GUI_OutsideWindowColor & 0x1F), numbuffer);
	strcat(Blue_OWC_Buffer, numbuffer);
	strcat(Blue_OWC_Buffer, ">");
	GUI_Options[6] = Blue_OWC_Buffer;

	GUI_Options[7] = " ";
	GUI_Options[8] = Options_InsideWindowColor;
	GUI_Options[9] = Options_RedComp;
	strcpy(Red_IWC_Buffer, Tag_Slider);
	strcat(Red_IWC_Buffer, SLIDERBOX_MAX);
	strcat(Red_IWC_Buffer, "31>");
	strcat(Red_IWC_Buffer, SLIDERBOX_MIN);
	strcat(Red_IWC_Buffer, "0>");
	strcat(Red_IWC_Buffer, SLIDERBOX_VALUE);
	pNesX_itoa(((GUI_InsideWindowColor >> 10) & 0x1F), numbuffer);
	strcat(Red_IWC_Buffer, numbuffer);
	strcat(Red_IWC_Buffer, ">");
	GUI_Options[10] = Red_IWC_Buffer;
	GUI_Options[11] = Options_GreenComp;
	strcpy(Green_IWC_Buffer, Tag_Slider);
	strcat(Green_IWC_Buffer, SLIDERBOX_MAX);
	strcat(Green_IWC_Buffer, "31>");
	strcat(Green_IWC_Buffer, SLIDERBOX_MIN);
	strcat(Green_IWC_Buffer, "0>");
	strcat(Green_IWC_Buffer, SLIDERBOX_VALUE);
	pNesX_itoa(((GUI_InsideWindowColor >> 5) & 0x1F), numbuffer);
	strcat(Green_IWC_Buffer, numbuffer);
	strcat(Green_IWC_Buffer, ">");
	GUI_Options[12] = Green_IWC_Buffer;
	GUI_Options[13] = Options_BlueComp;
	strcpy(Blue_IWC_Buffer, Tag_Slider);
	strcat(Blue_IWC_Buffer, SLIDERBOX_MAX);
	strcat(Blue_IWC_Buffer, "31>");
	strcat(Blue_IWC_Buffer, SLIDERBOX_MIN);
	strcat(Blue_IWC_Buffer, "0>");
	strcat(Blue_IWC_Buffer, SLIDERBOX_VALUE);
	pNesX_itoa((GUI_InsideWindowColor & 0x1F), numbuffer);
	strcat(Blue_IWC_Buffer, numbuffer);
	strcat(Blue_IWC_Buffer, ">");
	GUI_Options[14] = Blue_IWC_Buffer;

	GUI_Options[15] = " ";
	GUI_Options[16] = Options_TextColor;
	GUI_Options[17] = Options_RedComp;
	strcpy(Red_TC_Buffer, Tag_Slider);
	strcat(Red_TC_Buffer, SLIDERBOX_MAX);
	strcat(Red_TC_Buffer, "31>");
	strcat(Red_TC_Buffer, SLIDERBOX_MIN);
	strcat(Red_TC_Buffer, "0>");
	strcat(Red_TC_Buffer, SLIDERBOX_VALUE);
	pNesX_itoa(((GUI_TextColor >> 10) & 0x1F), numbuffer);
	strcat(Red_TC_Buffer, numbuffer);
	strcat(Red_TC_Buffer, ">");
	GUI_Options[18] = Red_TC_Buffer;
	GUI_Options[19] = Options_GreenComp;
	strcpy(Green_TC_Buffer, Tag_Slider);
	strcat(Green_TC_Buffer, SLIDERBOX_MAX);
	strcat(Green_TC_Buffer, "31>");
	strcat(Green_TC_Buffer, SLIDERBOX_MIN);
	strcat(Green_TC_Buffer, "0>");
	strcat(Green_TC_Buffer, SLIDERBOX_VALUE);
	pNesX_itoa(((GUI_TextColor >> 5) & 0x1F), numbuffer);
	strcat(Green_TC_Buffer, numbuffer);
	strcat(Green_TC_Buffer, ">");
	GUI_Options[20] = Green_TC_Buffer;
	GUI_Options[21] = Options_BlueComp;
	strcpy(Blue_TC_Buffer, Tag_Slider);
	strcat(Blue_TC_Buffer, SLIDERBOX_MAX);
	strcat(Blue_TC_Buffer, "31>");
	strcat(Blue_TC_Buffer, SLIDERBOX_MIN);
	strcat(Blue_TC_Buffer, "0>");
	strcat(Blue_TC_Buffer, SLIDERBOX_VALUE);
	pNesX_itoa((GUI_TextColor & 0x1F), numbuffer);
	strcat(Blue_TC_Buffer, numbuffer);
	strcat(Blue_TC_Buffer, ">");
	GUI_Options[22] = Blue_TC_Buffer;

	GUI_Options[23] = " ";
	GUI_Options[24] = Options_SelectedTextColor;
	GUI_Options[25] = Options_RedComp;
	strcpy(Red_STC_Buffer, Tag_Slider);
	strcat(Red_STC_Buffer, SLIDERBOX_MAX);
	strcat(Red_STC_Buffer, "31>");
	strcat(Red_STC_Buffer, SLIDERBOX_MIN);
	strcat(Red_STC_Buffer, "0>");
	strcat(Red_STC_Buffer, SLIDERBOX_VALUE);
	pNesX_itoa(((GUI_SelectedTextColor >> 10) & 0x1F), numbuffer);
	strcat(Red_STC_Buffer, numbuffer);
	strcat(Red_STC_Buffer, ">");
	GUI_Options[26] = Red_STC_Buffer;
	GUI_Options[27] = Options_GreenComp;
	strcpy(Green_STC_Buffer, Tag_Slider);
	strcat(Green_STC_Buffer, SLIDERBOX_MAX);
	strcat(Green_STC_Buffer, "31>");
	strcat(Green_STC_Buffer, SLIDERBOX_MIN);
	strcat(Green_STC_Buffer, "0>");
	strcat(Green_STC_Buffer, SLIDERBOX_VALUE);
	pNesX_itoa(((GUI_SelectedTextColor >> 5) & 0x1F), numbuffer);
	strcat(Green_STC_Buffer, numbuffer);
	strcat(Green_STC_Buffer, ">");
	GUI_Options[28] = Green_STC_Buffer;
	GUI_Options[29] = Options_BlueComp;
	strcpy(Blue_STC_Buffer, Tag_Slider);
	strcat(Blue_STC_Buffer, SLIDERBOX_MAX);
	strcat(Blue_STC_Buffer, "31>");
	strcat(Blue_STC_Buffer, SLIDERBOX_MIN);
	strcat(Blue_STC_Buffer, "0>");
	strcat(Blue_STC_Buffer, SLIDERBOX_VALUE);
	pNesX_itoa((GUI_SelectedTextColor & 0x1F), numbuffer);
	strcat(Blue_STC_Buffer, numbuffer);
	strcat(Blue_STC_Buffer, ">");
	GUI_Options[30] = Blue_STC_Buffer;

	GUI_Options[31] = " ";
	GUI_Options[32] = Options_BGColor;
	GUI_Options[33] = Options_RedComp;
	strcpy(Red_BG_Buffer, Tag_Slider);
	strcat(Red_BG_Buffer, SLIDERBOX_MAX);
	strcat(Red_BG_Buffer, "255>");
	strcat(Red_BG_Buffer, SLIDERBOX_MIN);
	strcat(Red_BG_Buffer, "0>");
	strcat(Red_BG_Buffer, SLIDERBOX_VALUE);
	pNesX_itoa(((GUI_BGColor >> 16) & 0xFF), numbuffer);
	strcat(Red_BG_Buffer, numbuffer);
	strcat(Red_BG_Buffer, ">");
	GUI_Options[34] = Red_BG_Buffer;
	GUI_Options[35] = Options_GreenComp;
	strcpy(Green_BG_Buffer, Tag_Slider);
	strcat(Green_BG_Buffer, SLIDERBOX_MAX);
	strcat(Green_BG_Buffer, "255>");
	strcat(Green_BG_Buffer, SLIDERBOX_MIN);
	strcat(Green_BG_Buffer, "0>");
	strcat(Green_BG_Buffer, SLIDERBOX_VALUE);
	pNesX_itoa(((GUI_BGColor >> 8) & 0xFF), numbuffer);
	strcat(Green_BG_Buffer, numbuffer);
	strcat(Green_BG_Buffer, ">");
	GUI_Options[36] = Green_BG_Buffer;
	GUI_Options[37] = Options_BlueComp;
	strcpy(Blue_BG_Buffer, Tag_Slider);
	strcat(Blue_BG_Buffer, SLIDERBOX_MAX);
	strcat(Blue_BG_Buffer, "255>");
	strcat(Blue_BG_Buffer, SLIDERBOX_MIN);
	strcat(Blue_BG_Buffer, "0>");
	strcat(Blue_BG_Buffer, SLIDERBOX_VALUE);
	pNesX_itoa((GUI_BGColor & 0xFF), numbuffer);
	strcat(Blue_BG_Buffer, numbuffer);
	strcat(Blue_BG_Buffer, ">");
	GUI_Options[38] = Blue_BG_Buffer;
}

void Handle_GUI_Interface(cont_state_t* my_state)
{
	uint16 temp;

	//Down Key Hit and Key is Ready to be hit
	if ((my_state -> buttons & CONT_DPAD_DOWN) && 
		(mydata.Highlighted_Index < Num_GUI_Options) && 
		(keyhit == 0))
	{
		mydata.Highlighted_Index++;
		if ((mydata.Highlighted_Index - mydata.Top_Index) >= mystyle.Max_Items)
			mydata.Top_Index++;
		keyhit = 1;
		isMainChanged = 1;
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
		isMainChanged = 1;
	}

	//Handle the modify boxes
	if ((my_state -> buttons & CONT_DPAD_LEFT) && 
		(xkeyhit == 0))
	{
		switch(mydata.Highlighted_Index)
		{
			case 2:
				temp = ((GUI_OutsideWindowColor >> 10) & 0x1F);
				if (temp > 0)
				{
					temp--;
					GUI_OutsideWindowColor = (GUI_OutsideWindowColor & 0x83FF) | (temp << 10);
					Generate_GUI_Options_List();
					mystyle.Border_Inside_Color = GUI_OutsideWindowColor;
					helpstyle.Border_Inside_Color = GUI_OutsideWindowColor;
					isMainChanged = 1;
					isSmallChanged = 1;
				}
				break;
			case 4:
				temp = ((GUI_OutsideWindowColor >> 5) & 0x1F);
				if (temp > 0)
				{
					temp--;
					GUI_OutsideWindowColor = (GUI_OutsideWindowColor & 0xFC1F) | (temp << 5);
					Generate_GUI_Options_List();
					mystyle.Border_Inside_Color = GUI_OutsideWindowColor;
					helpstyle.Border_Inside_Color = GUI_OutsideWindowColor;
					isMainChanged = 1;
					isSmallChanged = 1;
				}
				break;
			case 6:
				temp = (GUI_OutsideWindowColor & 0x1F);
				if (temp > 0)
				{
					temp--;
					GUI_OutsideWindowColor = (GUI_OutsideWindowColor & 0xFFE0) | (temp);
					Generate_GUI_Options_List();
					mystyle.Border_Inside_Color = GUI_OutsideWindowColor;
					helpstyle.Border_Inside_Color = GUI_OutsideWindowColor;
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
				}
				break;

			case 34:
				temp = ((GUI_BGColor >> 16) & 0xFF);
				if (temp > 0)
				{
					temp--;
					GUI_BGColor = (GUI_BGColor & 0x0000FFFF) | (temp << 16);
					Generate_GUI_Options_List();
					isMainChanged = 1;
				}
				break;
			case 36:
				temp = ((GUI_BGColor >> 8) & 0xFF);
				if (temp > 0)
				{
					temp--;
					GUI_BGColor = (GUI_BGColor & 0x00FF00FF) | (temp << 8);
					Generate_GUI_Options_List();
					isMainChanged = 1;
				}
				break;
			case 38:
				temp = (GUI_BGColor & 0xFF);
				if (temp > 0)
				{
					temp--;
					GUI_BGColor = (GUI_BGColor & 0x00FFFF00) | (temp);
					Generate_GUI_Options_List();
					isMainChanged = 1;
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
				temp = ((GUI_OutsideWindowColor >> 10) & 0x1F);
				if (temp < 31)
				{
					temp++;
					GUI_OutsideWindowColor = (GUI_OutsideWindowColor & 0x83FF) | (temp << 10);
					Generate_GUI_Options_List();
					mystyle.Border_Inside_Color = GUI_OutsideWindowColor;
					helpstyle.Border_Inside_Color = GUI_OutsideWindowColor;
					isMainChanged = 1;
					isSmallChanged = 1;
				}
				break;
			case 4:
				temp = ((GUI_OutsideWindowColor >> 5) & 0x1F);
				if (temp < 31)
				{
					temp++;
					GUI_OutsideWindowColor = (GUI_OutsideWindowColor & 0xFC1F) | (temp << 5);
					Generate_GUI_Options_List();
					mystyle.Border_Inside_Color = GUI_OutsideWindowColor;
					helpstyle.Border_Inside_Color = GUI_OutsideWindowColor;
					isMainChanged = 1;
					isSmallChanged = 1;
				}
				break;
			case 6:
				temp = (GUI_OutsideWindowColor & 0x1F);
				if (temp < 31)
				{
					temp++;
					GUI_OutsideWindowColor = (GUI_OutsideWindowColor & 0xFFE0) | (temp);
					Generate_GUI_Options_List();
					mystyle.Border_Inside_Color = GUI_OutsideWindowColor;
					helpstyle.Border_Inside_Color = GUI_OutsideWindowColor;
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
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
					isMainChanged = 1;
					isSmallChanged = 1;
				}
				break;

			case 34:
				temp = ((GUI_BGColor >> 16) & 0xFF);
				if (temp < 255)
				{
					temp++;
					GUI_BGColor = (GUI_BGColor & 0x0000FFFF) | (temp << 16);
					Generate_GUI_Options_List();
					isMainChanged = 1;
				}
				break;
			case 36:
				temp = ((GUI_BGColor >> 8) & 0xFF);
				if (temp < 255)
				{
					temp++;
					GUI_BGColor = (GUI_BGColor & 0x00FF00FF) | (temp << 8);
					Generate_GUI_Options_List();
					isMainChanged = 1;
				}
				break;
			case 38:
				temp = (GUI_BGColor & 0xFF);
				if (temp < 255)
				{
					temp++;
					GUI_BGColor = (GUI_BGColor & 0x00FFFF00) | (temp);
					Generate_GUI_Options_List();
					isMainChanged = 1;
				}
				break;
		}
	}					
	
	// Handle Return to Main Menu
	if ((my_state -> buttons & CONT_B) && 
		(keyhit == 0))
	{
		setup_main_menu();
		menuscreen = MENUNUM_MAIN;
		isMainChanged = 1;
		isSmallChanged = 1;
	}
}

