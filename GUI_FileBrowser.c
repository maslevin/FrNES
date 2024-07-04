/*===================================================================*/
/*                                                                   */
/*  GUI_VideoPage.c : GUI Page for Video Settings                    */
/*                    Interface, Variables, and Event Handling       */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#include "GUI_FileBrowser.h"
#include "macros.h"

#include "pNesX_System_Dc.h"
#include "pNesX_Utils.h"
#include "pNesX.h"

//Help for the ROM loading screen
char* Rom_Keys[] = {
	"Rom Select Screen",
	"A - Sel",
	"B - Back",
//	"X - Identify",
	"DPAD - Scroll", 
	"LTrig - PgUp",
	"RTrig - PgDn"
};
const int Num_Rom_Keys = 6;

//ROM data storage
const int MAX_ROMS = 2048;
RomInfo myRomInfos[2048];
char* myRomStrings[2048];

#define FILEBROWSER_ROOT_MENU 0
#define FILEBROWSER_START_LISTING_DIRECTORY 1
#define FILEBROWSER_CONTINUE_LISTING_DIRECTORY 2
#define FILEBROWSER_DIRECTORY_LISTING_COMPLETE 3
#define FILEBROWSER_IDLE 4

#define FILEBROWSER_DEVICE_UNKNOWN -1
#define FILEBROWSER_DEVICE_ROMDISK 0
#define FILEBROWSER_DEVICE_SDCARD 1
#define FILEBROWSER_DEVICE_CDROM 2
#define FILEBROWSER_DEVICE_PC 3
uint32 filebrowser_device = FILEBROWSER_DEVICE_UNKNOWN;

char current_directory_path[512];
char* ROOT_PATHS[] = {
	"/rd",
	"/sd",		
	"/cd",
	"/pc"
};
const uint32 NUM_ROOT_PATHS = 4;

uint32 invalidb = 0;

void setup_file_browser_screen()
{
	current_directory_path[0] = '/';
	current_directory_path[1] = '\0';

	//Set Up Window Data Features
	mydata.x = 208.0f;
	mydata.y = 32.0f;
	mydata.width = 400.0f;
	mydata.height = 416.0f;
	mydata.font = font;
	mydata.Header_Text = "Open File";//Main_Options[0];
	mydata.Data_Strings = ROOT_PATHS;
	mydata.Num_Strings = NUM_ROOT_PATHS;
	mydata.Highlighted_Index = 0;
	mydata.Top_Index = 0;

	//Set Up Window Style Features
	mystyle.Header_Text_Scale = 1.0f;
	mystyle.Text_Scale = 0.50f;
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
	helpdata.Data_Strings = Rom_Keys;
	helpdata.Num_Strings = Num_Rom_Keys;
	helpdata.Highlighted_Index = Num_Rom_Keys;
	helpdata.Top_Index = 0;

	//Set Up Window Style Features
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

void Handle_File_Browser_Interface(cont_state_t* my_state)
{
	//Down Key Hit and Key is Ready to be hit
	if ((my_state -> buttons & CONT_DPAD_DOWN) && 
		(keyhit == 0) && 
		(mydata.Highlighted_Index < (mydata.Num_Strings - 1))) {
		mydata.Highlighted_Index++;
		if ((mydata.Highlighted_Index - mydata.Top_Index) >= mystyle.Max_Items)
			mydata.Top_Index++;
		keyhit = 1;			
	}

	//Up Key Hit and Key is Ready to be hit
	if ((my_state -> buttons & CONT_DPAD_UP) && 
		(keyhit == 0) &&	
		(mydata.Highlighted_Index > 0)) {
		mydata.Highlighted_Index--;
		if (mydata.Top_Index > mydata.Highlighted_Index)
			mydata.Top_Index--;
		keyhit = 1;
	}
	
	//Page Down
	if (((my_state -> rtrig > 240) && ((mydata.Highlighted_Index + mystyle.Max_Items) < (mydata.Num_Strings - 1))) && (disable_trigs == 0)) {
		mydata.Highlighted_Index += mystyle.Max_Items;
		mydata.Top_Index += mystyle.Max_Items;
	}

	//Page Up
	if (((my_state -> ltrig > 240) && (mydata.Highlighted_Index > (mystyle.Max_Items))) && (disable_trigs == 0)) {
		mydata.Highlighted_Index -= mystyle.Max_Items;
		if (mydata.Top_Index - mystyle.Max_Items >= 0)
			mydata.Top_Index -= mystyle.Max_Items;
		else
			mydata.Top_Index = 0;
	} else if (((my_state -> ltrig > 240) && (mydata.Highlighted_Index <= (mystyle.Max_Items))) && (disable_trigs == 0)) {
		mydata.Highlighted_Index = 0;
		mydata.Top_Index = 0;
	};

	// Handle Return to Main Menu
	if ((my_state -> buttons & CONT_B) && 
		(invalidb == 0) &&
		(keyhit == 0)) {
		invalidb = 1;
		EndFileSearch();
		if (romselstatus == FILEBROWSER_ROOT_MENU) {
			setup_main_menu_screen();
			menuscreen = MENUNUM_MAIN;
		} else {
			// Set root device selection
			if (strlen(current_directory_path) <= 4) {
				romselstatus = FILEBROWSER_ROOT_MENU;				
				mydata.Header_Text = "Open File";//Main_Options[0];
				mydata.Data_Strings = ROOT_PATHS;
				mydata.Num_Strings = NUM_ROOT_PATHS;
				mydata.Highlighted_Index = 0;
			} else {
				for (int i = (strlen(current_directory_path) - 2); (i > 0) && (current_directory_path[i] != '/'); i--) {
					current_directory_path[i] = '\0';
				}
				printf("Going back directory to [%s]\n", current_directory_path);
				romselstatus = FILEBROWSER_START_LISTING_DIRECTORY;
			}			
		}
	} else if ((my_state -> buttons & CONT_B) == 0) {
		invalidb = 0;
	}


	//A Button Hit... send the rom path to the PnesX Main, init the emulator and prepare for return to main upon exit
	if ((my_state -> buttons & CONT_A) && 
		(invalida == 0)) {
		invalida = 1;
		char* selected_string = mydata.Data_Strings[mydata.Highlighted_Index];
		uint32 selected_string_length = strlen(selected_string);
		// pressed on .. - go up a level
		if (strcmp(selected_string, ".") == 0) {
			//DO NOTHING
		} else if (strcmp(selected_string, "..") == 0) {
			// Set root device selection
			if (strlen(current_directory_path) <= 4) {
				romselstatus = FILEBROWSER_ROOT_MENU;				
				mydata.Header_Text = "Open File";//Main_Options[0];
				mydata.Data_Strings = ROOT_PATHS;
				mydata.Num_Strings = NUM_ROOT_PATHS;
			} else {
				for (int i = (strlen(current_directory_path) - 2); (i > 0) && (current_directory_path[i] != '/'); i--) {
					current_directory_path[i] = '\0';
				}
				printf("Going back directory to [%s]\n", current_directory_path);
				romselstatus = FILEBROWSER_START_LISTING_DIRECTORY;
			}
		// pressed on a .nes file
		} else if ((disable_rom_interface == 0) &&
			(selected_string_length > 4) && 
			(selected_string[selected_string_length - 4] == '.') &&
			(selected_string[selected_string_length - 3] == 'n') && 
			(selected_string[selected_string_length - 2] == 'e') && 
			(selected_string[selected_string_length - 1] == 's')) {
			printf("main: loading rom [%s]\n", myRomInfos[mydata.Highlighted_Index].PhysFileName);
			if (pNesX_Load(myRomInfos[mydata.Highlighted_Index].PhysFileName, myRomInfos[mydata.Highlighted_Index].FileSize) == 0)
			{
				//Load Its SaveRAM
				if (SRAM_Enabled)
					LoadSRAM();

				//Stay in Emulator During Operation
				pNesX_Main();

				//Clean Up Afterwards
				free (ROM);
				//There are some games that don't have VROM
				if (VROM != NULL)
					free (VROM);
				if (VRAM != NULL)
					free (VRAM);

				//Save Its SaveRAM
				if (SRAM_Enabled)
					SaveSRAM();
			} else {
				printf("main: error failed to start emulator!!!!\n");
			}
			disable_trigs = 1;
			my_state -> rtrig = 255;
		// pressed on a directory
		} else {
			if (romselstatus == FILEBROWSER_ROOT_MENU) {
				strcpy(current_directory_path, selected_string);
				strcat(current_directory_path, "/");
				romselstatus = FILEBROWSER_START_LISTING_DIRECTORY;	
			} else {
				strcat(current_directory_path, selected_string);
				strcat(current_directory_path, "/");
				romselstatus = FILEBROWSER_START_LISTING_DIRECTORY;				
			}	
		}
	}

	switch (romselstatus) {
		case FILEBROWSER_START_LISTING_DIRECTORY:
			printf("starting search of path [%s]\n", current_directory_path);
			InitializeFileInfos(myRomInfos, myRomStrings, MAX_ROMS);
			mydata.Data_Strings = myRomStrings;
			mydata.Num_Strings = ReturnCurrentNumRoms();
			mydata.Highlighted_Index = 0;			
			if (StartFileSearch(current_directory_path, myRomInfos) == 1) {
				romselstatus = FILEBROWSER_CONTINUE_LISTING_DIRECTORY;
			} else {
				mydata.Header_Text = "Rom Load Failed";
				disable_rom_interface = 1;
				romselstatus = FILEBROWSER_IDLE;
			}			
			break;
		case FILEBROWSER_CONTINUE_LISTING_DIRECTORY:
			if (LoadNextFileSimple(myRomInfos, current_directory_path) != 1)
				romselstatus = FILEBROWSER_DIRECTORY_LISTING_COMPLETE;
			else
				mydata.Num_Strings = ReturnCurrentNumRoms();
			break;
		case FILEBROWSER_DIRECTORY_LISTING_COMPLETE:
			EndFileSearch();
			romselstatus = FILEBROWSER_IDLE;
			break;
		default:
			break;
	}	
}