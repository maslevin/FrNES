/*===================================================================*/
/*                                                                   */
/*  GUI_ControlPage.c : GUI Page for Control Settings                */
/*                      Interface, Variables, and Event Handling     */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#include "GUI_Credits.h"
#include "macros.h"

#include "pNesX_System_DC.h"

//About Screen Comments
char* About_Text[] = {
	"FrNES is a NES emulator for",
	"Sega Dreamcast. It was based",
	"on the first release of",
	"Racoons NES emulator for",
	"Playstation, pNesX.",
	"Now it has so much new",
	"code in it, it cannot",
	"truly be considered a",
	"port of the original",
	"",
	"This final version of",
	"0.6 includes new code",
	"for the PPU renderer",
	"written in assembly",
	"language.  As well, new",
	"code was introduced for",
	"mappers 7 and 9, with",
	"many more on their way",
	"",
	"",
	"",
	"Credits:",
	"  ReGex - Emulator Code",
	"        - Tools + GUI",
	"  Miss Soy Beansprout",
	"        - Art",
	"  Racoon - PS Version",
	"  Matt Conte - Sound Code",
	"  Marat - 6502 Emulator",
	"  Dan Potter",
	"  Marcus Comstedt",
	"  Jordan DeLong",
	"  And Many More",
	"             Kallistios",
	"",
	"Do not distribute this",
	"with ROM images!"
};

const int Num_About_Text = 36;

void setup_credits_screen()
{
	//Set Up Window Data Features
	mydata.x = 208.0f;
	mydata.y = 32.0f;
	mydata.width = 400.0f;
	mydata.height = 416.0f;
	mydata.font = font;
	mydata.Header_Text = "FrNES Credits";//Main_Options[6];
	mydata.Data_Strings = About_Text;
	mydata.Num_Strings = Num_About_Text;
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
	helpstyle.Border_Thickness = 5.0f;
	helpstyle.Border_Color = GUI_OutsideWindowColor;
	helpstyle.Inside_Color = GUI_InsideWindowColor;
	helpstyle.Left_Margin = 15;
	helpstyle.Header_Text_Color = GUI_TextColor;
	helpstyle.Text_Color = GUI_TextColor;
	helpstyle.Text_Scale = 0.40f;
	helpstyle.Max_Items = 10;
	helpstyle.Selected_Text_Color = GUI_SelectedTextColor;
	helpstyle.Selected_Background_Color = GUI_SelectedTextColor;//MakeRGB(31, 18, 8);
}

void Handle_Credits_Interface(cont_state_t* my_state)
{
	//printf("main loop: handling credits controller\n");			
	//Down Key Hit and Key is Ready to be hit
	if ((my_state -> buttons & CONT_DPAD_DOWN) && 
		(mydata.Highlighted_Index < Num_About_Text))
	{
		mydata.Highlighted_Index++;
		if ((mydata.Highlighted_Index - mydata.Top_Index) >= mystyle.Max_Items)
			mydata.Top_Index++;
		keyhit = 1;
	}

	//Up Key Hit and Key is Ready to be hit
	if ((my_state -> buttons & CONT_DPAD_UP) && 
		(mydata.Highlighted_Index > 0))
	{
		mydata.Highlighted_Index--;
		if (mydata.Top_Index > mydata.Highlighted_Index)
			mydata.Top_Index--;
		keyhit = 1;
	}
	
	// Handle Return to Main Menu
	if ((my_state -> buttons & CONT_B) && 
		(keyhit == 0))
	{
		setup_main_menu_screen();
		menuscreen = MENUNUM_MAIN;
	}
}