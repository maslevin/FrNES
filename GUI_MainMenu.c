/*===================================================================*/
/*                                                                   */
/*  GUI_ControlPage.c : GUI Page for Control Settings                */
/*                      Interface, Variables, and Event Handling     */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#include "GUI_MainMenu.h"
#include "macros.h"

#include "pNesX_System_DC.h"

char Main_Menu[] = "Main Menu";
char* Main_Options[] = {
	"Select A Rom",
	"Video Options",
	"Control Options",
	"System Options",
	"GUI Options",
	"Help",
	"FrNES Credits"
};
const int Num_Main = 7;

//Help for the Main screen
char* Main_Keys[] = {
	"Main",
	"A - Choose",
	"DPAD - Move"
};
const int Num_Main_Keys = 3;

void setup_main_menu_screen()
{
	//Set Up Window Data Features
	mydata.x = 208.0f;
	mydata.y = 32.0f;
	mydata.width = 400.0f;
	mydata.height = 416.0f;
	mydata.font = font;
	mydata.Header_Text = Main_Menu;
	mydata.Data_Strings = Main_Options;
	mydata.Num_Strings = Num_Main;
	mydata.Highlighted_Index = 0;
	mydata.Top_Index = 0;
	
	//Set Up Window Style Features
	mystyle.Header_Text_Scale = 1.0f;
	mystyle.Text_Scale = 0.60f;
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
	helpdata.Data_Strings = Main_Keys;
	helpdata.Num_Strings = Num_Main_Keys;
	helpdata.Highlighted_Index = Num_Main_Keys;
	helpdata.Top_Index = 0;

	//Set Up Window Style Features
	helpstyle.Header_Text_Scale = 0.5f;
	helpstyle.Text_Scale = 0.5f;	
	helpstyle.Border_Thickness = 5.0f;
	helpstyle.Border_Color = GUI_OutsideWindowColor;
	helpstyle.Inside_Color = GUI_InsideWindowColor;
	helpstyle.Left_Margin = 15;
	helpstyle.Header_Text_Color = GUI_TextColor;
	helpstyle.Text_Color = GUI_TextColor;
	helpstyle.Max_Items = 10;
	helpstyle.Selected_Text_Color = GUI_SelectedTextColor;
	helpstyle.Selected_Background_Color = GUI_SelectedTextColor;//MakeRGB(31, 18, 8);
}

void Handle_Main_Menu_Interface(cont_state_t* my_state)
{
	if ((my_state -> buttons & CONT_DPAD_DOWN) && 
		(mydata.Highlighted_Index < (Num_Main - 1)) && 
		(keyhit == 0))
	{
		mydata.Highlighted_Index++;
		keyhit = 1;
	}

	//Up Key Hit and Key is Ready to be hit
	if ((my_state -> buttons & CONT_DPAD_UP) && 
		(mydata.Highlighted_Index > 0) && 
		(keyhit == 0))
	{
		mydata.Highlighted_Index--;

		keyhit = 1;
	}

	//Choose and setup another page
	if ((my_state -> buttons & CONT_A) && 
		(invalida == 0))
	{
		switch (mydata.Highlighted_Index)
		{
			case PAGE_ROMSELECT:
				//Menu Screen = Romselect
				menuscreen = MENUNUM_ROMSELECT;
				romselstatus = 0;
				disable_rom_interface = 0;
				setup_file_browser_screen();
				break;

			case PAGE_CREDITS:
				//Menu Screen = About
				menuscreen = MENUNUM_CREDITS;
				setup_credits_screen();
				break;

			case PAGE_CONTROLOPTIONS:
				//Menu Screen = Options
				menuscreen = MENUNUM_CONTROLOPTIONS;
				setup_control_options_screen();
				Generate_Control_Options_List();
				break;

			case PAGE_SYSTEMOPTIONS:
				//Menu Screen = Options
				menuscreen = MENUNUM_SYSTEMOPTIONS;
				setup_system_options_screen();
				Generate_System_Options_List();
				break;

			case PAGE_HELP:
				//Menu Screen = Help
				menuscreen = MENUNUM_HELP;
				setup_help_screen();
				break;

			case PAGE_VIDEOOPTIONS:
				menuscreen = MENUNUM_VIDEOOPTIONS;
				setup_video_options_screen();
				Generate_Video_Options_List();
				break;

			case PAGE_GUIOPTIONS:
				menuscreen = MENUNUM_GUIOPTIONS;
				setup_gui_options_screen();
				Generate_GUI_Options_List();
				break;

			default:
				break;
		}
		invalida = 1;
	}	
}