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

#include "pNesX_System_DC.h"
#include "pNesX.h"

#include <fat/fs_fat.h>

//Help for the ROM loading screen
char* Rom_Keys[] = {
	"A - Select",
	"B - Back",
	"Up - Scroll Up",
	"Down - Scroll Down",
	"LT - Page Up",
	"RT - Page Down"
};
const int Num_Rom_Keys = 6;

//ROM data storage
#define MAX_ROMS 1024
RomInfo_t myRomInfos[MAX_ROMS];
char* myRomStrings[MAX_ROMS];

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

#define MAX_KEYHIT_INCREMENT 6

static kos_blockdev_t sd_dev;
bool sd_mounted = false;
bool file_search_completed;

bool mount_sd_fat() {
    uint8 partition_type;

    if(sd_init()) {
        printf("mount_sd_fat: Could not initialize the SD card. Please make sure that you "
               "have an SD card adapter plugged in and an SD card inserted.\n");
        return false;
    }

    if(fs_fat_init()) {
        printf("mount_sd_fat: Could not initialize fs_fat!\n");
        return false;
    }
        
    if(sd_blockdev_for_partition(0, &sd_dev, &partition_type)) {
        printf("mount_sd_fat: Could not find the first partition on the SD card!\n");
        return false;
    }

    if(fs_fat_mount("/sd", &sd_dev, FS_FAT_MOUNT_READWRITE)) {
        printf("mount_sd_fat: Could not mount SD card as fatfs. Please make sure the card "
            "has been properly formatted.\n");
        return false;
    }

    return true;
}

bool unmount_sd_fat() {
	if (fs_fat_unmount("/sd")) {
		printf("unmount_sd_fat: Could not unmount SD card\n");
		return false;
	}
    fs_fat_shutdown();
	if (sd_shutdown()) {
		printf("unmount_sd_fat: Could not shut down SD interface\n");
		return false;		
	}
	return true;
}

char current_directory_path[512];
char* ROOT_PATHS[] = {
	"<Previous Directory>,DIR<RLAlign>",
	"/rd,DIR<RLAlign>",
	"/cd,DIR<RLAlign>",
	"/pc,DIR<RLAlign>",
	"/sd,DIR<RLAlign>"
};
char* ROOT_PATH_VALUES[] = {
	"..",
	"/rd",
	"/cd",
	"/pc",
	"/sd"
};
const uint32 NUM_ROOT_PATHS = 5;

uint32 invalidb = 0;

void setup_file_browser_screen() {
	// If we don't have an SD card adapter installed in the serial port, remove it from the browseable paths in the file browser
	uint32 num_root_paths = NUM_ROOT_PATHS;

	if (!sd_mounted) {
		sd_mounted = mount_sd_fat();
		if (!sd_mounted) {
			num_root_paths--;
		}
	}

	current_directory_path[0] = '/';
	current_directory_path[1] = '\0';

	//Set Up Window Data Features
	mydata.x = 208.0f;
	mydata.y = 32.0f;
	mydata.width = 400.0f;
	mydata.height = 416.0f;
	mydata.font = font;
	mydata.Header_Text = "Open File";
	mydata.Data_Strings = ROOT_PATHS;
	mydata.Num_Strings = num_root_paths;
	mydata.Highlighted_Index = 0;
	mydata.Top_Index = 0;

	//Set Up Window Style Features
	mystyle.Header_Text_Scale = 1.0f;
	mystyle.Text_Scale = 0.50f;
	mystyle.Border_Thickness = 5.0f;
	mystyle.Border_Color = options.GUI_OutsideWindowColor; 
	mystyle.Inside_Color = options.GUI_InsideWindowColor;
	mystyle.Left_Margin = 15;
	mystyle.Line_Spacing = 0.0f;	
	mystyle.Header_Text_Color = options.GUI_TextColor;
	mystyle.Text_Color = options.GUI_TextColor;
	mystyle.Max_Items = (mydata.height - (mydata.font -> fontHeight * mystyle.Header_Text_Scale) - (2 * CORNER_RADIUS)) / ((float)mydata.font -> fontHeight * mystyle.Text_Scale);
	mystyle.Selected_Text_Color = options.GUI_SelectedTextColor;
	mystyle.Selected_Background_Color = options.GUI_SelectedTextColor;

	//Set Up Window Data Features
	helpdata.x = 32.0f;
	helpdata.y = 300.0f;
	helpdata.width = 160.0f;
	helpdata.height = 148.0f;
	helpdata.font = font;
	helpdata.Header_Text = "Browse";
	helpdata.Data_Strings = Rom_Keys;
	helpdata.Num_Strings = Num_Rom_Keys;
	helpdata.Highlighted_Index = Num_Rom_Keys;
	helpdata.Top_Index = 0;

	//Set Up Window Style Features
	helpstyle.Header_Text_Scale = 0.75f;
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

void Return_To_Main_Menu() {
	setup_main_menu_screen();
	menuscreen = MENUNUM_MAIN;			
	if (sd_mounted) {
		unmount_sd_fat();
		sd_mounted = false;
	}		
}

void Handle_File_Browser_Interface(cont_state_t* my_state) {
	//Down Key Hit and Key is Ready to be hit
	if (my_state -> buttons & CONT_DPAD_DOWN) {
		switch (keyhit) {
			case 0: {
				if ((mydata.Highlighted_Index < (mydata.Num_Strings - 1))) {
					mydata.Highlighted_Index++;
					if ((mydata.Highlighted_Index - mydata.Top_Index) >= mystyle.Max_Items) {
						mydata.Top_Index++;
					}
				}
				keyhit = 1;
			} break;
			case 1 ... (MAX_KEYHIT_INCREMENT - 1): {
				keyhit++;
			} break;
			case MAX_KEYHIT_INCREMENT: {			
				keyhit = 0;
			} break;
		}
	}

	//Up Key Hit and Key is Ready to be hit
	if (my_state -> buttons & CONT_DPAD_UP) {
		switch (keyhit) {
			case 0: {
				if (mydata.Highlighted_Index > 0) {
					mydata.Highlighted_Index--;
					if (mydata.Top_Index > mydata.Highlighted_Index) {
						mydata.Top_Index--;
					}
				}
				keyhit = 1;
			} break;
			case 1 ... (MAX_KEYHIT_INCREMENT - 1): {
				keyhit++;
			} break;
			case MAX_KEYHIT_INCREMENT: {			
				keyhit = 0;
			} break;
		}
	}
	
	//Page Down
	if ((my_state -> rtrig > 240) && (disable_trigs == 0)) {
		if ((mydata.Highlighted_Index + mystyle.Max_Items) < (mydata.Num_Strings - 1)) {
			mydata.Highlighted_Index += mystyle.Max_Items;
			mydata.Top_Index += mystyle.Max_Items;
		} else if (mydata.Num_Strings > mystyle.Max_Items) {
			mydata.Highlighted_Index = mydata.Num_Strings - 1;
			mydata.Top_Index = mydata.Num_Strings - mystyle.Max_Items;
		} else {
			mydata.Highlighted_Index = mydata.Num_Strings - 1;
		}
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
		if (!file_search_completed) {
			EndFileSearch();
		}
		if (romselstatus == FILEBROWSER_ROOT_MENU) {
			Return_To_Main_Menu();
		} else {
			// Set root device selection
			if (strlen(current_directory_path) <= 4) {
				romselstatus = FILEBROWSER_ROOT_MENU;
				setup_file_browser_screen();
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
		char* selected_string;
		if (romselstatus == FILEBROWSER_ROOT_MENU) {
			selected_string = ROOT_PATHS[mydata.Highlighted_Index];
		} else {
			selected_string = myRomInfos[mydata.Highlighted_Index].PhysFileName;
		}
		uint32 selected_string_length = strlen(selected_string);
		// pressed on .. - go up a level
		if (strcmp(selected_string, "..") == 0) {
			// Set root device selection
			if (strlen(current_directory_path) <= 4) {
				romselstatus = FILEBROWSER_ROOT_MENU;				
				mydata.Header_Text = "Open File";
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
		} else if (
			(disable_rom_interface == 0) &&
			(selected_string_length > 4) && 
			(selected_string[selected_string_length - 4] == '.') &&
			((selected_string[selected_string_length - 3] == 'n') || (selected_string[selected_string_length - 3] == 'N')) && 
			((selected_string[selected_string_length - 2] == 'e') || (selected_string[selected_string_length - 2] == 'E')) && 
			((selected_string[selected_string_length - 1] == 's') || (selected_string[selected_string_length - 1] == 'S'))
		) {
			RomSize = myRomInfos[mydata.Highlighted_Index].FileSize;
			strcpy(szRomPath, myRomInfos[mydata.Highlighted_Index].PhysFileName);
			launchEmulator();
			disable_trigs = 1;
			my_state -> rtrig = 255;
		// pressed on a directory
		} else {
			if (romselstatus == FILEBROWSER_ROOT_MENU) {
				if (mydata.Highlighted_Index == 0) {
					Return_To_Main_Menu();
				} else {
					strcpy(current_directory_path, ROOT_PATH_VALUES[mydata.Highlighted_Index]);
					strcat(current_directory_path, "/");
					romselstatus = FILEBROWSER_START_LISTING_DIRECTORY;	
				}
			} else {
				strcat(current_directory_path, selected_string);
				strcat(current_directory_path, "/");
				romselstatus = FILEBROWSER_START_LISTING_DIRECTORY;				
			}	
		}
	}

	switch (romselstatus) {
		case FILEBROWSER_START_LISTING_DIRECTORY:
			file_search_completed = false;
			printf("starting search of path [%s]\n", current_directory_path);
			InitializeFileInfos(myRomInfos, myRomStrings, MAX_ROMS);
			mydata.Data_Strings = myRomStrings;
			mydata.Num_Strings = ReturnCurrentNumRoms();
			mydata.Highlighted_Index = 0;
			mydata.Top_Index = 0;
			if (StartFileSearch(current_directory_path, myRomInfos) == 1) {
				romselstatus = FILEBROWSER_CONTINUE_LISTING_DIRECTORY;
			} else {
				mydata.Header_Text = "Rom Load Failed";
				disable_rom_interface = 1;
				romselstatus = FILEBROWSER_IDLE;
			}			
			break;
		case FILEBROWSER_CONTINUE_LISTING_DIRECTORY:
			if (LoadNextFileSimple(myRomInfos, current_directory_path) != 1) {
				romselstatus = FILEBROWSER_DIRECTORY_LISTING_COMPLETE;
			} else {
				mydata.Num_Strings = ReturnCurrentNumRoms();
			}
			break;
		case FILEBROWSER_DIRECTORY_LISTING_COMPLETE:
			file_search_completed = true;		
			EndFileSearch();
			romselstatus = FILEBROWSER_IDLE;
			break;
		default:
			break;
	}	
}