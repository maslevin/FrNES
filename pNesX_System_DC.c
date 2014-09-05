/*===================================================================*/
/*                                                                   */
/*  pNesX_System_DC.c : The function which depends on a system       */
/*                         (for Dreamcast)                           */
/*                                                                   */
/*  2001/04/19  ReGex - Started Work on Port using my Hugo-Dreamcast */
/*                      Loader and PNESX as a template               */
/*  2001/04/25  ReGex - First Public Release                         */
/*                    - Sound not working.. cpu slightly modified    */
/*  2001/05/01  ReGex - PowerVR Menu System working 100%             */
/*                    - Working on accelerating PPU using PVR        */
/*  2001/06/13  ReGex - Release 2 is out ... lots more stuff         */
/*  2001/09/05  ReGex - KOS Port Underway ... Stability OTW??        */
/*  2001/10/03  ReGex - 0.60 Beta Release under KOS                  */
/*  2001/10/15  ReGex - Assembly Language Work Begins on PPU         */
/*  2001/11/15  ReGex - Injury Version Begins                        */
/*  2001/12/12  ReGex - Preparing Injury Beta                        */
/*  2001/12/22  ReGex - Finishing Source Release + Release Intro     */
/*                    - for Christmas                                */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/
#include "pNesX.h"
#include "pNesX_System.h"
#include "pNesX_System_DC.h"
#include "pNesX_Utils.h"
#include "vmu_dc.h"
#include "vmu_icons.h"
#include "GUI_VideoPage.h"
#include "GUI_SystemPage.h"
#include "GUI_ControlPage.h"
#include "GUI_GUIPage.h"
#include "FrNESImg.h"

/*-------------------------------------------------------------------*/
/*  Interface variables                                              */
/*-------------------------------------------------------------------*/
int pagesize = 640 * 480 * 16;
int lpagesize = 153600;
int epagesize = 320 * 240 * 16;
int currentpage = 0;
int xposition = 100;
int yposition = 100;
int xvelocity = 5;
int yvelocity = 5;
int i, j;
uint16* page1;
uint16* page2;
uint32* lpage1;
uint32* lpage2;
uint16* epage1;
uint16* epage2;

int menuscreen;
int invalida;
int keyhit;
int xkeyhit;

/*------------------------------------------------------------------*/
/*  GUI Customization Variables                                     */
/*------------------------------------------------------------------*/
uint32 GUI_BGColor;
uint16 GUI_TextColor;
uint16 GUI_SelectedTextColor;
uint16 GUI_InsideWindowColor;
uint16 GUI_OutsideWindowColor;

const int title_offset_x = 25;
const int title_offset_y = 25;

const int help_window_offset_y = 275;

Window_Style mystyle;
Window_Data mydata;

Window_Style helpstyle;
Window_Data helpdata;

#define NUM_CHARS 66

//Define 3 font areas
BMF_Character smallfont[NUM_CHARS];
BMF_Character medfont[NUM_CHARS];
BMF_Character largefont[NUM_CHARS];

//Rom buffer area ... uses dynamic memory allocation
unsigned char *ROM;
unsigned char *VROM;

uint32 SRAM_Enabled;

char FRNES_DISCSTRING[] = "/ROMS/";
char NESTERDC_DISCSTRING[] = "/GAMES/";

char App_String[] = "FrNES";
char Version_String[] = "0.60 Final";

char Main_Menu[] = "Main Menu";
char* Main_Options[] = {
	"Select A Rom",
	"Video Options",
	"Control Options",
	"System Options",
	"GUI Options",
//	"Font Options",
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

//Help for the Options screen
char* Options_Keys[] = {
	"A - Toggle",
	"DPad L and R to Change"
};
const int Num_Options_Keys = 2;


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

//Help Screen Comments
char* Help_Text[] = {
	"At the Options Screen",
	"  Left and Right on DPad",
	"  change Options and move",
	"  sliders",
	"",
	"  A Toggles Checkboxes",
	"",
	"Inside the Emu",
	"  LTrig and RTrig - Exit",
	"",
	"Otherwise:",
	"  2 Players are supported",
	"  As long as the controllers",
	"  Have been scanned at boot",
	"  or at the menu option."
};

const int Num_Help_Text = 14;

//Generic help for other text/screens
char* Text_Keys[] = {
	"Keys",
	"A - Choose",
	"DPAD - Move"
};
const int Num_Text_Keys = 3;

//Options Screen Menu Items

const int Max_Frameskip = 5;

uint16 default_Sound = 1;
uint16 default_FrameSkip = 0;
uint16 default_Analog = 0;
uint16 default_Select = 4;
uint16 default_AKey = 0;
uint16 default_BKey = 2;
uint16 default_Stretch = 1;
uint16 default_Filter = 1;
uint16 default_Profile = 1;
uint16 default_Format = 0;
uint16 default_VMUPort = 0;
uint16 default_SRAM = 1;
uint16 default_AutoFrameSkip = 0;
uint16 default_Clip = 0;

uint16 color;
uint16 bgcolor;
int textwidth;
int textheight;
int interface_offset;

//PowerVR Offsets and Font stuff
uint32 PVR_Font_Offset;
uint32 PVR_MainWindow_Offset;
uint32 PVR_SmallWindow_Offset;
uint32 PVR_NESScreen1_Offset;
uint32 PVR_NESScreen2_Offset;
uint32 PVR_White_Font_Offset;
uint32 PVR_DemoTex_Offset;
uint32 PVR_TitleScreen_Offset;
unsigned char isMainChanged;
unsigned char isSmallChanged;
unsigned char PVR_Font_Widths[NUM_CHARS];
unsigned char PVR_Font_Heights[NUM_CHARS];
poly_hdr_t my_pheader;
vertex_ot_t my_vertex;

//ROM data storage
const int MAX_ROMS = 2048;
RomInfo myRomInfos[2048];
char* myRomStrings[2048];

//The crc32 of the currently selected rom
uint32 currentCRC32;

//Control Variables
uint32 oldState;
int nWait;

/*-------------------------------------------------------------------*/
/*  ROM image file information                                       */
/*-------------------------------------------------------------------*/

char szRomName[ 256 ];
char szSaveName[ 256 ];
int nSRAM_SaveFlag;

/*-------------------------------------------------------------------*/
/*  Variables for Windows                                            */
/*-------------------------------------------------------------------*/

unsigned char *pScreenMem;

// Palette data
uint16 NesPalette[ 64 ] =
{
  0x39ce, 0x1071, 0x0015, 0x2013, 0x440e, 0x5402, 0x5000, 0x3c20,
  0x20a0, 0x0100, 0x0140, 0x00e2, 0x0ceb, 0x0000, 0x0000, 0x0000,
  0x5ef7, 0x01dd, 0x10fd, 0x401e, 0x5c17, 0x700b, 0x6ca0, 0x6521,
  0x45c0, 0x0240, 0x02a0, 0x0247, 0x0211, 0x0000, 0x0000, 0x0000,
  0x7fff, 0x1eff, 0x2e5f, 0x223f, 0x79ff, 0x7dd6, 0x7dcc, 0x7e67,
  0x7ae7, 0x4342, 0x2769, 0x2ff3, 0x03bb, 0x0000, 0x0000, 0x0000,
  0x7fff, 0x579f, 0x635f, 0x6b3f, 0x7f1f, 0x7f1b, 0x7ef6, 0x7f75,
  0x7f94, 0x73f4, 0x57d7, 0x5bf9, 0x4ffe, 0x0000, 0x0000, 0x0000
};

//Controllers - will store controller addresses - NULL if not found
int8 Controllers[4];

//VMUs - will store VMU addresses -- NULL if not found
int8 VMUs[8];

void initialize_controllers()
{
	Controllers[0] = (int8) NULL;
	Controllers[1] = (int8) NULL;
	Controllers[2] = (int8) NULL;
	Controllers[3] = (int8) NULL;
	VMUs[0] = (int8) NULL;
	VMUs[1] = (int8) NULL;
	VMUs[2] = (int8) NULL;
	VMUs[3] = (int8) NULL;
	VMUs[4] = (int8) NULL;
	VMUs[5] = (int8) NULL;
	VMUs[6] = (int8) NULL;
	VMUs[7] = (int8) NULL;
}

void rescan_controllers()
{
	int i;
	int j;
	int firstport;

	//quietly rescan the bus
	maple_rescan_bus(1);
	
	//scan to make sure we have all the controllers that are plugged in
	//don't worry about vmus for now
	for (i = 0; i < 4; i++)
	{
		firstport = -1;
		for (j = 0; j < 6; j++)
		{
			if (maple_device_func(i, j) & MAPLE_FUNC_CONTROLLER)
			{
				Controllers[i] = maple_create_addr(i, j);
			}
			if (maple_device_func(i, j) & MAPLE_FUNC_MEMCARD)
			{
				if (firstport == -1)
					firstport = j;
				VMUs[(i * 2) + (j - firstport)] = maple_create_addr(i, j);
				vmu_draw_lcd(VMUs[(i * 2) + (j - firstport)], vmu_screen_normal);
			}
		}
	}
}

uint16 MakeRGB(int red, int green, int blue)
{
	return ((red << 10 | green << 5 | blue << 0) | 0x8000);
}

void draw_screen()
{
	
	poly_hdr_t my_pheader;
	vertex_ot_t my_vertex;
	vertex_oc_t my_c_vertex;

	if (isMainChanged)
	{
		vid_waitvbl();
		win_draw_textwindow(&mydata, &mystyle);
		isMainChanged = 0;
	}
	if (isSmallChanged)
	{
		vid_waitvbl();
		win_draw_textwindow(&helpdata, &helpstyle);
		isSmallChanged = 0;
	}

	ta_begin_render();

	//Draw the Screen ... will be more fancy later
	ta_poly_hdr_txr(&my_pheader, TA_OPAQUE, TA_NO_TEXTURE, 0, 0, 0, 0);
	ta_commit_poly_hdr(&my_pheader);

	my_c_vertex.flags = TA_VERTEX_NORMAL;
	my_c_vertex.x = 0.0f;
	my_c_vertex.y = 480.0f;
	my_c_vertex.z = 25.0f;
	my_c_vertex.a = 1.0f;
	my_c_vertex.r = ((float)(GUI_BGColor >> 16) / 255);
	my_c_vertex.g = ((float)(0x00FF & (GUI_BGColor >> 8)) / 255);
	my_c_vertex.b = ((float)(0x00FF & GUI_BGColor) / 255);
	ta_commit_vertex(&my_c_vertex, sizeof(my_c_vertex));

	my_c_vertex.flags = TA_VERTEX_NORMAL;
	my_c_vertex.y = 0.0f;
	ta_commit_vertex(&my_c_vertex, sizeof(my_c_vertex));

	my_c_vertex.flags = TA_VERTEX_NORMAL;
	my_c_vertex.x = 640.0f;
	my_c_vertex.y = 480.0f;
	ta_commit_vertex(&my_c_vertex, sizeof(my_c_vertex));

	my_c_vertex.flags = TA_VERTEX_EOL;
	my_c_vertex.y = 0.0f;
	ta_commit_vertex(&my_c_vertex, sizeof(my_c_vertex));

	ta_commit_eol();

	ta_poly_hdr_txr(&my_pheader, TA_TRANSLUCENT, TA_ARGB1555, 512, 512, PVR_MainWindow_Offset, TA_BILINEAR_FILTER);
	ta_commit_poly_hdr(&my_pheader);

	my_vertex.flags = TA_VERTEX_NORMAL;
	my_vertex.x = 200.0f;
	my_vertex.y = 440.0f;
	my_vertex.z = 30.0f;
	my_vertex.u = 0.0f;
	my_vertex.v = ((float) mydata.height) / 512.0f;
	my_vertex.a = 0.75f;
	my_vertex.r = 1.0f;
	my_vertex.g = 1.0f;
	my_vertex.b = 1.0f;
	my_vertex.oa = 1.0f;
	my_vertex.or = 1.0f;
	my_vertex.og = 1.0f;
	my_vertex.ob = 1.0f;
	my_vertex.dummy1 = 0;
	my_vertex.dummy2 = 0;
	ta_commit_vertex(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = TA_VERTEX_NORMAL;
	my_vertex.y = 20.0f;
	my_vertex.u = 0.0f;
	my_vertex.v = 0.0f;
	ta_commit_vertex(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = TA_VERTEX_NORMAL;
	my_vertex.x = 620.0f;
	my_vertex.y = 440.0f;
	my_vertex.u = ((float) mydata.width - 7) / 512.0f;
	my_vertex.v = ((float) mydata.height) / 512.0f;
	ta_commit_vertex(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = TA_VERTEX_EOL;
	my_vertex.y = 20.0f;
	my_vertex.u = ((float) mydata.width - 7) / 512.0f;
	my_vertex.v = 0.0f;
	ta_commit_vertex(&my_vertex, sizeof(my_vertex));

	ta_poly_hdr_txr(&my_pheader, TA_TRANSLUCENT, TA_ARGB1555, 512, 512, PVR_SmallWindow_Offset, TA_BILINEAR_FILTER);
	ta_commit_poly_hdr(&my_pheader);

	my_vertex.flags = TA_VERTEX_NORMAL;
	my_vertex.x = 20.0f;
	my_vertex.y = 440.0f;
	my_vertex.z = 30.0f;
	my_vertex.u = 0.0f;
	my_vertex.v = ((float) helpdata.height) / 512.0f;
	my_vertex.a = 0.75f;
	my_vertex.r = 1.0f;
	my_vertex.g = 1.0f;
	my_vertex.b = 1.0f;
	my_vertex.oa = 1.0f;
	my_vertex.or = 1.0f;
	my_vertex.og = 1.0f;
	my_vertex.ob = 1.0f;
	my_vertex.dummy1 = 0;
	my_vertex.dummy2 = 0;
	ta_commit_vertex(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = TA_VERTEX_NORMAL;
	my_vertex.y = 280.0f;
	my_vertex.u = 0.0f;
	my_vertex.v = 0.0f;
	ta_commit_vertex(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = TA_VERTEX_NORMAL;
	my_vertex.x = 190.0f;
	my_vertex.y = 440.0f;
	my_vertex.u = ((float) helpdata.width - 7) / 512.0f;
	my_vertex.v = ((float) helpdata.height) / 512.0f;
	ta_commit_vertex(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = TA_VERTEX_EOL;
	my_vertex.y = 280.0f;
	my_vertex.u = ((float) helpdata.width - 7) / 512.0f;
	my_vertex.v = 0.0f;
	ta_commit_vertex(&my_vertex, sizeof(my_vertex));

	bf_ta_submit_string(PVR_Font_Offset, title_offset_x, title_offset_y, 32.0f, 150, 35, PVR_Font_Widths, PVR_Font_Heights, App_String);
	bf_ta_submit_string(PVR_Font_Offset, title_offset_x, title_offset_y + 35, 32.0f, 160, 20, PVR_Font_Widths, PVR_Font_Heights, Version_String);
	ta_commit_eol();

	ta_finish_frame();

}

void setup_main_menu()
{

	//Set Up Window Data Features
	mydata.x = 0;
	mydata.y = 0;
	mydata.width = 400;
	mydata.height = 460 - (2 * title_offset_y);
	mydata.Target_Width = 512;
	mydata.Header_Font = largefont;
	mydata.Item_Font = medfont;
	mydata.Header_Text = Main_Menu;
	mydata.Data_Strings = Main_Options;
	mydata.Num_Strings = Num_Main;
	mydata.Highlighted_Index = 0;
	mydata.Top_Index = 0;
	mydata.Target_Buffer = (ta_txr_map(PVR_MainWindow_Offset));
	
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
	helpdata.Data_Strings = Main_Keys;
	helpdata.Num_Strings = Num_Main_Keys;
	helpdata.Highlighted_Index = Num_Main_Keys;
	helpdata.Top_Index = 0;
	helpdata.Target_Buffer = (ta_txr_map(PVR_SmallWindow_Offset));
}

void setup_rom_screen()
{
	//Set Up Window Data Features
	mydata.x = 0;
	mydata.y = 0;
	mydata.width = 400;
	mydata.height = 460 - (2 * title_offset_y);
	mydata.Target_Width = 512;
	mydata.Header_Font = largefont;
	mydata.Item_Font = medfont;
	mydata.Header_Text = Main_Options[0];
	mydata.Data_Strings = myRomStrings;
	mydata.Num_Strings = MAX_ROMS;
	mydata.Highlighted_Index = 0;
	mydata.Top_Index = 0;
	mydata.Target_Buffer = (ta_txr_map(PVR_MainWindow_Offset));

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
	helpdata.Data_Strings = Rom_Keys;
	helpdata.Num_Strings = Num_Rom_Keys;
	helpdata.Highlighted_Index = Num_Rom_Keys;
	helpdata.Top_Index = 0;
	helpdata.Target_Buffer = (ta_txr_map(PVR_SmallWindow_Offset));
}

void setup_about_screen()
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
	mydata.Header_Text = Main_Options[6];
	mydata.Data_Strings = About_Text;
	mydata.Num_Strings = Num_About_Text;
	mydata.Highlighted_Index = 0;
	mydata.Top_Index = 0;
	mydata.Target_Buffer = (ta_txr_map(PVR_MainWindow_Offset));

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
	helpdata.Data_Strings = Text_Keys;
	helpdata.Num_Strings = Num_Text_Keys;
	helpdata.Highlighted_Index = Num_Text_Keys;
	helpdata.Top_Index = 0;
	helpdata.Target_Buffer = (ta_txr_map(PVR_SmallWindow_Offset));
}

void setup_help_screen()
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
	mydata.Header_Text = Main_Options[5];
	mydata.Data_Strings = Help_Text;
	mydata.Num_Strings = Num_Help_Text;
	mydata.Highlighted_Index = 0;
	mydata.Top_Index = 0;
	mydata.Target_Buffer = (ta_txr_map(PVR_MainWindow_Offset));

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
	helpdata.Header_Text = "";
	helpdata.Data_Strings = Text_Keys;
	helpdata.Num_Strings = Num_Text_Keys;
	helpdata.Highlighted_Index = 0;
	helpdata.Top_Index = 0;
	helpdata.Target_Buffer = (ta_txr_map(PVR_SmallWindow_Offset));
}


int LoadSRAM()
{
	char textbuffer[13];

	if ((*opt_SRAM == 1) && (*opt_VMUPort != -1))
	{
		pNesX_i32toa(currentCRC32, textbuffer);
		strcat(textbuffer, "    ");
		vmu_icon_draw(vmu_screen_loading, VMUs[*opt_VMUPort]);
		switch (load_SRAM(VMUs[*opt_VMUPort], textbuffer, SRAM))
		{
			case 1:
				vmu_icon_draw(vmu_screen_normal, VMUs[*opt_VMUPort]);
				break;
			case -2:
				vmu_icon_draw(vmu_screen_error, VMUs[*opt_VMUPort]);
				break;
			case -3:
				vmu_icon_draw(vmu_screen_error, VMUs[*opt_VMUPort]);
				break;
			default:
				vmu_icon_draw(vmu_screen_error, VMUs[*opt_VMUPort]);
				break;
		}
		return 1;
	}
	else
		return -1;
}

int SaveSRAM()
{
	char textbuffer[13];
	char textbuffer2[255];

	if ((*opt_SRAM == 1) && (*opt_VMUPort != -1))
	{
		pNesX_i32toa(currentCRC32, textbuffer);
		strcat(textbuffer, "    ");


		strcpy(textbuffer2, myRomInfos[mydata.Highlighted_Index].PhysFileName);
		textbuffer2[31] = ((char)NULL);

		vmu_icon_draw(vmu_screen_saving, VMUs[*opt_VMUPort]);
		switch (save_SRAM(VMUs[*opt_VMUPort], textbuffer, textbuffer2, SRAM))
		{
			case 1:
				vmu_icon_draw(vmu_screen_normal, VMUs[*opt_VMUPort]);
				break;
			case 2:
				vmu_icon_draw(vmu_screen_normal, VMUs[*opt_VMUPort]);
				break;
			case -1:
				vmu_icon_draw(vmu_screen_error, VMUs[*opt_VMUPort]);
				break;
		}
		return 1;
	}
	else
		return -1;
}

void Save_VMU_Options()
{
	//Menu Screen = Save Options
	if (*opt_VMUPort != -1)
	{
		vmu_icon_draw(vmu_screen_saving, VMUs[*opt_VMUPort]);
		switch (save_user_settings(VMUs[*opt_VMUPort]))
		{
			case -1:
				vmu_icon_draw(vmu_screen_error, VMUs[*opt_VMUPort]);
				break;
			default:
				vmu_icon_draw(vmu_screen_normal, VMUs[*opt_VMUPort]);
				break;
		}
	}
}

void Load_VMU_Options()
{
	//Menu Screen = Load Options
	if (*opt_VMUPort != -1)
	{
		vmu_icon_draw(vmu_screen_loading, VMUs[*opt_VMUPort]);
		if (load_user_settings(VMUs[*opt_VMUPort]) == -1)
			vmu_icon_draw(vmu_screen_error, VMUs[*opt_VMUPort]);
		else
			vmu_icon_draw(vmu_screen_normal, VMUs[*opt_VMUPort]);
	}
}


void DoDemo()
{
	float xpos;
	float ypos;
	float xvelocity;
	float yvelocity;
	float sidelength;

	int charindex;
	int sineoffset;
	int charoffset;

	float amplitude;
	float period;
	float ampvel;
	float pervel;

	int done;
	int timer = 0;
	int timer_min = 1;
	int alphacount = 0;
	int alphavel;
	int i;
	int j;
	float alpha;
	uint16* texptr;
	int zoomin;
	int zoomout;
	uint16 demo_bgcolor;

	poly_hdr_t my_pheader;
	vertex_ot_t my_vertex;
	vertex_oc_t my_c_vertex;

	int8 Controller;
	cont_cond_t my_cond;

	uint32 Tex_Offset;
	int Char_Maxbytes;

	Tex_Offset = PVR_Font_Offset;

	texptr = ta_txr_map(PVR_TitleScreen_Offset);

	//Load the titlescreen texture
	for (j = 0; j < imheight; j++)
		for (i = 0; i < imwidth; i++)
			texptr[(j * 512) + i] = FrNESImg[(j * imwidth) + i];

	demo_bgcolor = FrNESImg[0];

	Char_Maxbytes = 64*64*2;

	//Grab the controller
	Controller = maple_first_controller();

	xpos = 320.0f;
	ypos = 240.0f;
	sidelength = 50.0f;
	xvelocity = 5.0f;
	yvelocity = 5.0f;
	amplitude = 80;
	period = 1.0;
	ampvel = 0.05;
	pervel = 0.005;
	alphacount = 0;
	alphavel = 5;
	alpha = 1.0;

	cont_get_cond(Controller, &my_cond);

	charindex = 0;
	sineoffset = 0;
	charoffset = 0;

	done = 0;
	
	timer_init();
	// Start a counter to cycle at once a second
	timer_prime(TMU1, 1, 0);
	timer_start(TMU1);

	zoomin = 1;
	zoomout = 0;
	alpha = 0;

	while (!(done))
	{
		ta_begin_render();

		//Draw the Screen ... will be more fancy later
		ta_poly_hdr_txr(&my_pheader, TA_OPAQUE, TA_NO_TEXTURE, 0, 0, 0, 0);
		ta_commit_poly_hdr(&my_pheader);
		
		my_c_vertex.flags = TA_VERTEX_NORMAL;
		my_c_vertex.x = 0.0f;
		my_c_vertex.y = 480.0f;
		my_c_vertex.z = 1.5f;
		my_c_vertex.a = 1.0f;
		my_c_vertex.r = (((demo_bgcolor >> 10) & 0x1f) / 31.0f);
		my_c_vertex.g = (((demo_bgcolor >> 5) & 0x1f) / 31.0f);
		my_c_vertex.b = ((demo_bgcolor & 0x1f) / 31.0f);
		ta_commit_vertex(&my_c_vertex, sizeof(my_c_vertex));

		my_c_vertex.flags = TA_VERTEX_NORMAL;
		my_c_vertex.y = 0.0f;
		ta_commit_vertex(&my_c_vertex, sizeof(my_c_vertex));

		my_c_vertex.flags = TA_VERTEX_NORMAL;
		my_c_vertex.x = 640.0f;
		my_c_vertex.y = 480.0f;
		ta_commit_vertex(&my_c_vertex, sizeof(my_c_vertex));

		my_c_vertex.flags = TA_VERTEX_EOL;
		my_c_vertex.y = 0.0f;
		ta_commit_vertex(&my_c_vertex, sizeof(my_c_vertex));

		ta_commit_eol();
		
		ta_poly_hdr_txr(&my_pheader, TA_TRANSLUCENT, TA_ARGB1555, 512, 512, PVR_TitleScreen_Offset, TA_BILINEAR_FILTER);
		ta_commit_poly_hdr(&my_pheader);

		my_vertex.flags = TA_VERTEX_NORMAL;
		my_vertex.x = (640 - imwidth) / 2.0f;
		my_vertex.y = 460;
		my_vertex.z = 10;
		my_vertex.u = 0.0f;
		my_vertex.v = 0;
		my_vertex.a = alpha;
		my_vertex.r = 1.0f;
		my_vertex.g = 1.0f;
		my_vertex.b = 1.0f;
		my_vertex.oa = 0.0f;
		my_vertex.or = 0.0f;
		my_vertex.og = 0.0f;
		my_vertex.ob = 0.0f;
		my_vertex.dummy1 = 0;
		my_vertex.dummy2 = 0;
		ta_commit_vertex(&my_vertex, sizeof(my_vertex));

		my_vertex.flags = TA_VERTEX_NORMAL;
		my_vertex.y = 0;
		my_vertex.v = (imheight / 512.0f);
		ta_commit_vertex(&my_vertex, sizeof(my_vertex));

		my_vertex.flags = TA_VERTEX_NORMAL;
		my_vertex.x = 640 - ((640 - imwidth) / 2.0f);
		my_vertex.y = 460;
		my_vertex.u = 1.0;
		my_vertex.v = 0.0f;
		ta_commit_vertex(&my_vertex, sizeof(my_vertex));

		my_vertex.flags = TA_VERTEX_EOL;
		my_vertex.y = 0;                                                
		my_vertex.u = 1.0;
		my_vertex.v = (imheight / 512.0f);
		ta_commit_vertex(&my_vertex, sizeof(my_vertex));

		ta_commit_eol();

		ta_finish_frame();

		if (zoomin)
		{
			alpha += 0.01;
		}
		
		if (zoomout)
		{
			alpha -= 0.01;
		}

		if ((alpha >= 1) && (zoomin == 1))
			zoomin = 0;


		if ((alpha <= 0) && (zoomout == 1))
			done = 1;

		cont_get_cond(Controller, &my_cond);

		if (timer_clear(TMU1))
		{
			timer++;
			timer_prime(TMU1, 1, 0);
			timer_start(TMU1);
		}

		if ((!(my_cond.buttons & CONT_START)) && (timer > timer_min))
		{
			zoomout = 1;
		}
	}
	timer_stop(TMU1);
}

/*===================================================================*/
/*                                                                   */
/*                dc_main() : Application main                       */
/*                                                                   */
/*===================================================================*/
int main()
{
	int result;
	int romselstatus;

	int disable_trigs;
	int disable_rom_interface;
	int i;
	cont_cond_t my_cond;

	// System initiation
	kos_init_all(IRQ_ENABLE | TA_ENABLE , ROMDISK_NONE);

	Allocate_Video_Options();
	Allocate_System_Options();
	Allocate_Control_Options();

	//Some Memory Areas
	PVR_Font_Offset = ta_txr_allocate(540672);
	PVR_MainWindow_Offset = ta_txr_allocate(512 * 512 * 2);
	PVR_SmallWindow_Offset = ta_txr_allocate(512 * 512 * 2);

	//32-byte align so the Store Queues can write to it.
	PVR_NESScreen1_Offset = ta_txr_allocate(256 * 256 * 2);
	PVR_NESScreen2_Offset = ta_txr_allocate(256 * 256 * 2);
	PVR_White_Font_Offset = ta_txr_allocate(540672);

	//Use the MainWindow Texture for Title Screen since both won't ever be used at the same time
	PVR_TitleScreen_Offset = PVR_MainWindow_Offset;

	isMainChanged = 1;
	isSmallChanged = 1;

	initialize_controllers();
	rescan_controllers();

	//Load Fonts
	fs_iso9660_init();
	bf_load_file("/FONTS/LGFONT.BMF", largefont);
	bf_load_file("/FONTS/SMLFONT.BMF", smallfont);
	result = bf_load_file("/FONTS/MEDFONT.BMF", medfont);

	//Load Fonts from CD into PVR memory
	bf_load_file_pvr_colors("/FONTS/LGFONT.BMF", PVR_Font_Offset, 64 * 64 * 2, PVR_Font_Widths, PVR_Font_Heights, 0x0000, 0x8000);
	bf_load_file_pvr_colors("/FONTS/LGFONT.BMF", PVR_White_Font_Offset, 64 * 64 * 2, PVR_Font_Widths, PVR_Font_Heights, 0x0000, 0xFFFF);

	//Do demo
	DoDemo();

	//Christmas Colors
	GUI_BGColor = 0x0000FF80;
	GUI_TextColor = 0x8000;
	GUI_SelectedTextColor = MakeRGB(13, 28, 8);
	GUI_InsideWindowColor = MakeRGB(31, 8, 8);
	GUI_OutsideWindowColor = MakeRGB(31, 31, 31);

	interface_offset = title_offset_x + 160;

	menuscreen = MENUNUM_MAIN;
	romselstatus = 0;
	invalida = 0;
	xkeyhit = 0;
	disable_trigs = 0;
	disable_rom_interface = 0;

	*opt_P1AnalogEnabled = default_Analog;
	*opt_P2AnalogEnabled = default_Analog;
	*opt_P1SelectKey = default_Select;
	*opt_P1AKey = default_AKey;
	*opt_P1BKey = default_BKey;
	*opt_P2SelectKey = default_Select;
	*opt_P2AKey = default_AKey;
	*opt_P2BKey = default_BKey;

	*opt_Stretch = default_Stretch;
	*opt_Filter = default_Filter;
	opt_ClipVars[0] = default_Clip;
	opt_ClipVars[1] = default_Clip;
	opt_ClipVars[2] = default_Clip;
	opt_ClipVars[3] = default_Clip;
	
	*opt_SoundEnabled = default_Sound;
	*opt_FrameSkip = default_FrameSkip;
	*opt_AutoFrameSkip = default_AutoFrameSkip;
	*opt_DiscFormat = default_Format;
	*opt_SRAM = default_SRAM;

	//If the default Memory card is present
	if (VMUs[default_VMUPort] != 0)
	{
		*opt_VMUPort = default_VMUPort;
		//Try to load settings if possible
		vmu_icon_draw(vmu_screen_loading, VMUs[*opt_VMUPort]);
		load_user_settings(VMUs[*opt_VMUPort]);
		vmu_icon_draw(vmu_screen_normal, VMUs[*opt_VMUPort]);
	}
	else
	//Otherwise Search for a Memcard
	{
		//Choose the first one available
		for (i = 0; i < 8; i++)
		{
			if (VMUs[i] != 0)
			{
				*opt_VMUPort = i;
				//Try to load settings if possible
				vmu_icon_draw(vmu_screen_loading, VMUs[*opt_VMUPort]);
				load_user_settings(VMUs[*opt_VMUPort]);
				vmu_icon_draw(vmu_screen_normal, VMUs[*opt_VMUPort]);
				break;
			}
			else
				//If one wasn't found, turn off memcard support
				*opt_VMUPort = -1;
		}
	}

	setup_main_menu();

	switch (result)
	{
		case 1:
		{
			keyhit = 0;
			cont_get_cond(Controllers[0], &my_cond);

//			while (my_cond.buttons & CONT_Y)
			while (1) 
			{
				//Event handling and processing section
				switch (menuscreen)
				{
					//Main Menu Screen
					case MENUNUM_MAIN:
						//Down Key Hit and Key is Ready to be hit
						if (((!(my_cond.buttons & CONT_DPAD_DOWN)) && (mydata.Highlighted_Index < (Num_Main - 1))) && (keyhit == 0))
						{
							mydata.Highlighted_Index++;
							keyhit = 1;
							isMainChanged = 1;
						}

						//Up Key Hit and Key is Ready to be hit
						if (((!(my_cond.buttons & CONT_DPAD_UP)) && (mydata.Highlighted_Index > 0)) && (keyhit == 0))
						{
							mydata.Highlighted_Index--;

							keyhit = 1;
							isMainChanged = 1;
						}

						//Choose and setup another page
						if (!(my_cond.buttons & CONT_A) && (invalida == 0))
						{
							switch (mydata.Highlighted_Index)
							{
								case PAGE_ROMSELECT:
									//Menu Screen = Romselect
									menuscreen = MENUNUM_ROMSELECT;
									romselstatus = 0;
									disable_rom_interface = 0;
									setup_rom_screen();
									break;

								case PAGE_CREDITS:
									//Menu Screen = About
									menuscreen = MENUNUM_CREDITS;
									setup_about_screen();
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

//								case PAGE_FONTOPTIONS:
								default:
									break;
							}
							isMainChanged = 1;
							isSmallChanged = 1;
							invalida = 1;
						}
						break;

					//GUI Options
					case MENUNUM_GUIOPTIONS:
						Handle_GUI_Interface(my_cond);
						break;

					//System Options
					case MENUNUM_SYSTEMOPTIONS:
						Handle_System_Interface(my_cond);
						break;

					//Video Options
					case MENUNUM_VIDEOOPTIONS:
						Handle_Video_Interface(my_cond);
						break;

					//Control Options
					case MENUNUM_CONTROLOPTIONS:
						Handle_Control_Interface(my_cond);
						break;

					//Rom Selection Screen
					case MENUNUM_ROMSELECT:
						switch (romselstatus)
						{
							case 0:
								InitializeFileInfos(myRomInfos, myRomStrings, MAX_ROMS);
								if (*opt_DiscFormat)
								{
									if (StartFileSearch(NESTERDC_DISCSTRING) == 1)
										romselstatus = 1;
									else
									{
										mydata.Header_Text = "Rom Load Failed";
										disable_rom_interface = 1;
									}
								}
								else
								{
									if (StartFileSearch(FRNES_DISCSTRING) == 1)
										romselstatus = 1;
									else
									{
										mydata.Header_Text = "Rom Load Failed";
										disable_rom_interface = 1;
									}
								}
								isMainChanged = 1;
								break;
							case 1:
								if (LoadNextFileSimple(myRomInfos) != 1)
									romselstatus = 2;
								else
									mydata.Num_Strings = ReturnCurrentNumRoms();
								isMainChanged = 1;
								break;
							case 2:
								EndFileSearch();
								romselstatus = 3;
								break;
							default:
								break;
						}

						//Down Key Hit and Key is Ready to be hit
						if ((!(my_cond.buttons & CONT_DPAD_DOWN)) && (mydata.Highlighted_Index < (mydata.Num_Strings - 1)))
						{
							mydata.Highlighted_Index++;
							if ((mydata.Highlighted_Index - mydata.Top_Index) >= mystyle.Max_Items)
								mydata.Top_Index++;
							isMainChanged = 1;
						}

						//Up Key Hit and Key is Ready to be hit
						if ((!(my_cond.buttons & CONT_DPAD_UP)) && (mydata.Highlighted_Index > 0))
						{
							mydata.Highlighted_Index--;
							if (mydata.Top_Index > mydata.Highlighted_Index)
								mydata.Top_Index--;
							keyhit = 1;
							isMainChanged = 1;
						}
						
						//Page Down
						if (((my_cond.rtrig > 240) && ((mydata.Highlighted_Index + mystyle.Max_Items) < (mydata.Num_Strings - 1))) && (disable_trigs == 0))
						{
							mydata.Highlighted_Index += mystyle.Max_Items;
							mydata.Top_Index += mystyle.Max_Items;
							isMainChanged = 1;
						}

						//Page Up
						if (((my_cond.ltrig > 240) && (mydata.Highlighted_Index > (mystyle.Max_Items))) && (disable_trigs == 0))
						{
							mydata.Highlighted_Index -= mystyle.Max_Items;
							if (mydata.Top_Index - mystyle.Max_Items >= 0)
								mydata.Top_Index -= mystyle.Max_Items;
							else
								mydata.Top_Index = 0;
							isMainChanged = 1;
						}
						else
						if (((my_cond.ltrig > 240) && (mydata.Highlighted_Index <= (mystyle.Max_Items))) && (disable_trigs == 0))
						{
							mydata.Highlighted_Index = 0;
							mydata.Top_Index = 0;
							isMainChanged = 1;
						};

						// Handle Return to Main Menu
						if (!(my_cond.buttons & CONT_B) && (keyhit == 0))
						{
							EndFileSearch();
							setup_main_menu();
							menuscreen = MENUNUM_MAIN;
							isMainChanged = 1;
						}

						//A Button Hit... send the rom path to the PnesX Main, init the emulator and prepare for return to main upon exit
						if (!(my_cond.buttons & CONT_A) && (invalida == 0) && (disable_rom_interface == 0))
						{
							if (pNesX_Load(myRomInfos[mydata.Highlighted_Index].PhysFileName) == 0)
							{
								//Load It's SaveRAM
								if (SRAM_Enabled)
									LoadSRAM();

								pNesX_Main();

								free (ROM);
								//There are some games that don't have VROM
								if (VROM != NULL)
									free (VROM);

								//Save It's SaveRAM
								if (SRAM_Enabled)
									SaveSRAM();
							}
							disable_trigs = 1;
							my_cond.rtrig = 255;
						}
						break;



					//About HugoDC Screen
					case MENUNUM_CREDITS:
						//Down Key Hit and Key is Ready to be hit
						if ((!(my_cond.buttons & CONT_DPAD_DOWN)) && (mydata.Highlighted_Index < Num_About_Text))
						{
							mydata.Highlighted_Index++;
							if ((mydata.Highlighted_Index - mydata.Top_Index) >= mystyle.Max_Items)
								mydata.Top_Index++;
							keyhit = 1;
							isMainChanged = 1;
						}

						//Up Key Hit and Key is Ready to be hit
						if ((!(my_cond.buttons & CONT_DPAD_UP)) && (mydata.Highlighted_Index > 0))
						{
							mydata.Highlighted_Index--;
							if (mydata.Top_Index > mydata.Highlighted_Index)
								mydata.Top_Index--;
							keyhit = 1;
							isMainChanged = 1;
						}
						
						// Handle Return to Main Menu
						if (!(my_cond.buttons & CONT_B) && (keyhit == 0))
						{
							setup_main_menu();
							menuscreen = MENUNUM_MAIN;
							isMainChanged = 1;
							isSmallChanged = 1;
						}
						break;

					//Help Screen
					case MENUNUM_HELP:
						//Down Key Hit and Key is Ready to be hit
						if ((!(my_cond.buttons & CONT_DPAD_DOWN)) && (mydata.Highlighted_Index < Num_Help_Text))
						{
							mydata.Highlighted_Index++;
							if ((mydata.Highlighted_Index - mydata.Top_Index) >= mystyle.Max_Items)
								mydata.Top_Index++;
							keyhit = 1;
							isMainChanged = 1;
						}

						//Up Key Hit and Key is Ready to be hit
						if ((!(my_cond.buttons & CONT_DPAD_UP)) && (mydata.Highlighted_Index > 0))
						{
							mydata.Highlighted_Index--;
							if (mydata.Top_Index > mydata.Highlighted_Index)
								mydata.Top_Index--;
							keyhit = 1;
							isMainChanged = 1;
						}

						// Handle Return to Main Menu
						if (!(my_cond.buttons & CONT_B) && (keyhit == 0))
						{
							setup_main_menu();
							menuscreen = MENUNUM_MAIN;
							isMainChanged = 1;
							isSmallChanged = 1;
						}
						break;

				}


				//Used to Validate A Key after it's been pressed
				if (((keyhit == 1) && (my_cond.buttons & CONT_DPAD_DOWN)) && 
				   ((keyhit == 1) && (my_cond.buttons & CONT_DPAD_UP)))
				{
					keyhit = 0;
				}

				if (((xkeyhit == 1) && (my_cond.buttons & CONT_DPAD_LEFT)) && 
				   ((xkeyhit == 1) && (my_cond.buttons & CONT_DPAD_RIGHT)))
				{
					xkeyhit = 0;
				}

				if ((invalida == 1) && (my_cond.buttons & CONT_A))
					invalida = 0;

				if ((disable_trigs == 1) && ((my_cond.rtrig == 0) && (my_cond.ltrig == 0)))
					disable_trigs = 0;

				draw_screen();
				
				cont_get_cond(Controllers[0], &my_cond);
			}
		}
		break;


		case 2:
		{
			bfont_draw_str(page1 + (200 * 640), 640, 0, "Not a .BMF File");
			bfont_draw_str(page1 + (300 * 640), 640, 0, "Initialization Halted");		
		}
		break;

		case 4:
		{
			bfont_draw_str(page1 + (200 * 640), 640, 0, "Bad Dimension");
			bfont_draw_str(page1 + (300 * 640), 640, 0, "Initialization Halted");		
		}
		break;
	}

	Free_Video_Options();
	Free_System_Options();
	Free_Control_Options();

	ta_txr_release_all();
	ta_shutdown();
	bf_free_font(medfont);
	bf_free_font(largefont);
	return 0;
}

int pNesX_ReadRom ( const char *pszFileName)
{
	char textbuffer[255];
	char* ROM_Buffer;
	uint32 my_fd;
	int i;
	int ROM_offset;
	int VROM_offset;

	ROM_Buffer = malloc(myRomInfos[mydata.Highlighted_Index].FileSize);

	currentCRC32 = ReturnChecksum(mydata.Highlighted_Index, myRomInfos, ROM_Buffer);

	//Support for multiple disc formats
	if (*opt_DiscFormat)
		strcpy(textbuffer, "/GAMES/");
	else
		strcpy(textbuffer, "/ROMS/");

	strcat (textbuffer, pszFileName);

	my_fd = iso_open(textbuffer, O_RDONLY);
	iso_read(my_fd, ROM_Buffer, myRomInfos[mydata.Highlighted_Index].FileSize);
	iso_close(my_fd);

	//Assume there's an NesHeader to read..  .nes files only supported
	for (i = 0; i < sizeof (NesHeader); i++)
		((unsigned char*) &NesHeader)[i] = ROM_Buffer[i];

	//Read past the trainer
	if (NesHeader.byInfo1 & 4)
	{
		i += 512;
	}

	if (NesHeader.byInfo1 & 2)
		SRAM_Enabled = 1;
	else
		SRAM_Enabled = 0;
	
	ROM_offset = i;
	ROM = malloc (NesHeader.byRomSize * 0x4000);
	for (; i < (ROM_offset + (NesHeader.byRomSize * 0x4000)); i++)
		ROM[i - ROM_offset] = ROM_Buffer[i];

	if (NesHeader.byVRomSize > 0)
	{
		VROM_offset = i;
		VROM = malloc (NesHeader.byVRomSize * 0x2000);
		for (; i < (VROM_offset + (NesHeader.byVRomSize * 0x2000)); i++)
			VROM[i - VROM_offset] = ROM_Buffer[i];
	}
	else
		VROM = NULL;

	free(ROM_Buffer);

	return 0;
}

void pNesX_LoadFrame()
{
//	char tempbuffer1[20];
//	char tempbuffer2[20];

	ta_begin_render();
	if (*opt_Filter)
	{
		if (WorkFrameIdx)
			ta_poly_hdr_txr(&my_pheader, TA_OPAQUE, TA_ARGB1555, 256, 256, PVR_NESScreen2_Offset, TA_BILINEAR_FILTER);
		else
			ta_poly_hdr_txr(&my_pheader, TA_OPAQUE, TA_ARGB1555, 256, 256, PVR_NESScreen1_Offset, TA_BILINEAR_FILTER);			

	}
	else
	{
		if (WorkFrameIdx)
			ta_poly_hdr_txr(&my_pheader, TA_OPAQUE, TA_ARGB1555, 256, 256, PVR_NESScreen2_Offset, TA_NO_FILTER);
		else
			ta_poly_hdr_txr(&my_pheader, TA_OPAQUE, TA_ARGB1555, 256, 256, PVR_NESScreen1_Offset, TA_NO_FILTER);
	}
	ta_commit_poly_hdr(&my_pheader);


	if (*opt_Stretch)
	{
		my_vertex.flags = TA_VERTEX_NORMAL;
		my_vertex.x = 0.0f;
		my_vertex.y = 480.0f;
		my_vertex.z = 30.0f;
		my_vertex.u = ((float) (opt_ClipVars[0])) / 256.0f;
		my_vertex.v = ((float) 240 - (opt_ClipVars[3])) / 256.0f;
		my_vertex.a = 1.0f;
		my_vertex.r = 1.0f;
		my_vertex.g = 1.0f;
		my_vertex.b = 1.0f;
		my_vertex.oa = 1.0f;
		my_vertex.or = 1.0f;
		my_vertex.og = 1.0f;
		my_vertex.ob = 1.0f;
		my_vertex.dummy1 = 0;
		my_vertex.dummy2 = 0;
		ta_commit_vertex(&my_vertex, sizeof(my_vertex));

		my_vertex.flags = TA_VERTEX_NORMAL;
		my_vertex.y = 0.0f;
		my_vertex.v = ((float) (opt_ClipVars[2])) / 256.0f;
		ta_commit_vertex(&my_vertex, sizeof(my_vertex));

		my_vertex.flags = TA_VERTEX_NORMAL;
		my_vertex.x = 640.0f;
		my_vertex.y = 480.0f;
		my_vertex.u = ((float) 256 - (opt_ClipVars[1])) / 256.0f;
		my_vertex.v = ((float) 240 - (opt_ClipVars[3])) / 256.0f;
		ta_commit_vertex(&my_vertex, sizeof(my_vertex));

		my_vertex.flags = TA_VERTEX_EOL;
		my_vertex.y = 0.0f;
		my_vertex.v = ((float) (opt_ClipVars[2])) / 256.0f;
		ta_commit_vertex(&my_vertex, sizeof(my_vertex));	
		ta_commit_eol();
	}
	else
	{
		my_vertex.flags = TA_VERTEX_NORMAL;
		my_vertex.x = 64.0f;
		my_vertex.y = 480.0f;
		my_vertex.z = 30.0f;
		my_vertex.u = ((float) (opt_ClipVars[0])) / 256.0f;
		my_vertex.v = ((float) 240 - (opt_ClipVars[3])) / 256.0f;
		my_vertex.a = 1.0f;
		my_vertex.r = 1.0f;
		my_vertex.g = 1.0f;
		my_vertex.b = 1.0f;
		my_vertex.oa = 1.0f;
		my_vertex.or = 1.0f;
		my_vertex.og = 1.0f;
		my_vertex.ob = 1.0f;
		my_vertex.dummy1 = 0;
		my_vertex.dummy2 = 0;
		ta_commit_vertex(&my_vertex, sizeof(my_vertex));

		my_vertex.flags = TA_VERTEX_NORMAL;
		my_vertex.y = 0.0f;
		my_vertex.v = ((float) (opt_ClipVars[2])) / 256.0f;
		ta_commit_vertex(&my_vertex, sizeof(my_vertex));

		my_vertex.flags = TA_VERTEX_NORMAL;
		my_vertex.x = 576.0f;
		my_vertex.y = 480.0f;
		my_vertex.u = ((float) 256 - (opt_ClipVars[1])) / 256.0f;
		my_vertex.v = ((float) 240 - (opt_ClipVars[3])) / 256.0f;
		ta_commit_vertex(&my_vertex, sizeof(my_vertex));

		my_vertex.flags = TA_VERTEX_EOL;
		my_vertex.y = 0.0f;
		my_vertex.v = ((float) (opt_ClipVars[2])) / 256.0f;
		ta_commit_vertex(&my_vertex, sizeof(my_vertex));	
		ta_commit_eol();
	}

	ta_poly_hdr_txr(&my_pheader, TA_TRANSLUCENT, TA_NO_TEXTURE, 0, 0, 0, 0);
	ta_commit_poly_hdr(&my_pheader);

	ta_commit_eol();

	ta_finish_frame();
}

void pNesX_PadState(uint32 *pdwPad1, uint32 *pdwPad2, uint32* ExitCount)
{
	cont_cond_t my_cond;
	uint32 dwTemp;

	dwTemp = 0;
	//Grab data from controller 0
	if (Controllers[0] != (int8) NULL)
	{
		cont_get_cond(Controllers[0], &my_cond);
		//Start first
		*pdwPad1 = (( !(my_cond.buttons & CONT_START      ) != 0 ) << 3);
		switch (*opt_P1AKey)
		{
			case 0:
				*pdwPad1 |= ( !(my_cond.buttons & CONT_A) != 0);
				break;
			case 1:
				*pdwPad1 |= ( !(my_cond.buttons & CONT_B) != 0);
				break;
			case 2:
				*pdwPad1 |= ( !(my_cond.buttons & CONT_X) != 0);
				break;
			case 3:
				*pdwPad1 |= ( !(my_cond.buttons & CONT_Y) != 0);
				break;
			case 4:
				*pdwPad1 |= (my_cond.ltrig != 0);
				break;
		}
		switch (*opt_P1BKey)
		{
			case 0:
				*pdwPad1 |= ( !(my_cond.buttons & CONT_A) != 0) << 1;
				break;
			case 1:
				*pdwPad1 |= ( !(my_cond.buttons & CONT_B) != 0) << 1;
				break;
			case 2:
				*pdwPad1 |= ( !(my_cond.buttons & CONT_X) != 0) << 1;
				break;
			case 3:
				*pdwPad1 |= ( !(my_cond.buttons & CONT_Y) != 0) << 1;
				break;
			case 4:
				*pdwPad1 |= (my_cond.ltrig != 0) << 1;
				break;
		}
		switch (*opt_P1SelectKey)
		{
			case 0:
				*pdwPad1 |= ( !(my_cond.buttons & CONT_A) != 0) << 2;
				break;
			case 1:
				*pdwPad1 |= ( !(my_cond.buttons & CONT_B) != 0) << 2;
				break;
			case 2:
				*pdwPad1 |= ( !(my_cond.buttons & CONT_X) != 0) << 2;
				break;
			case 3:
				*pdwPad1 |= ( !(my_cond.buttons & CONT_Y) != 0) << 2;
				break;
			case 4:
				*pdwPad1 |= (my_cond.ltrig != 0) << 2;
				break;
		}
		if (*opt_P1AnalogEnabled)
		{
			*pdwPad1 |= 
				   ( (my_cond.joyy < 114) << 4) | // Up
				   ( (my_cond.joyy > 140) << 5) | // Down
				   ( (my_cond.joyx < 114) << 6) | // Left
				   ( (my_cond.joyx > 140) << 7); // Right
		}
		else
			*pdwPad1 |= 
				   ( ( !(my_cond.buttons & CONT_DPAD_UP    ) != 0 ) << 4) | // Up
				   ( ( !(my_cond.buttons & CONT_DPAD_DOWN  ) != 0 ) << 5) | // Down
				   ( ( !(my_cond.buttons & CONT_DPAD_LEFT  ) != 0 ) << 6) | // Left
				   ( ( !(my_cond.buttons & CONT_DPAD_RIGHT ) != 0 ) << 7); // Right


		*pdwPad1 = *pdwPad1 | ( *pdwPad1 << 8);
	}
	else
		*pdwPad1 = 0;


	// Increment Exit Counter if Required
	if (Controllers[0] != (int8) NULL)
	{
		if ((my_cond.rtrig > 200) && (my_cond.ltrig != 0))
			(*ExitCount)++;
		else
			ExitCount = 0;
	}

	
	//Grab data from controller 1
	if (Controllers[1] != (int8) NULL)
	{
		cont_get_cond(Controllers[1], &my_cond);
		//Start first
		*pdwPad2 = (( !(my_cond.buttons & CONT_START      ) != 0 ) << 3);
		switch (*opt_P2AKey)
		{
			case 0:
				*pdwPad2 |= ( !(my_cond.buttons & CONT_A) != 0);
				break;
			case 1:
				*pdwPad2 |= ( !(my_cond.buttons & CONT_B) != 0);
				break;
			case 2:
				*pdwPad2 |= ( !(my_cond.buttons & CONT_X) != 0);
				break;
			case 3:
				*pdwPad2 |= ( !(my_cond.buttons & CONT_Y) != 0);
				break;
			case 4:
				*pdwPad2 |= (my_cond.ltrig != 0);
				break;
		}
		switch (*opt_P2BKey)
		{
			case 0:
				*pdwPad2 |= ( !(my_cond.buttons & CONT_A) != 0) << 1;
				break;
			case 1:
				*pdwPad2 |= ( !(my_cond.buttons & CONT_B) != 0) << 1;
				break;
			case 2:
				*pdwPad2 |= ( !(my_cond.buttons & CONT_X) != 0) << 1;
				break;
			case 3:
				*pdwPad2 |= ( !(my_cond.buttons & CONT_Y) != 0) << 1;
				break;
			case 4:
				*pdwPad2 |= (my_cond.ltrig != 0) << 1;
				break;
		}
		switch (*opt_P2SelectKey)
		{
			case 0:
				*pdwPad2 |= ( !(my_cond.buttons & CONT_A) != 0) << 2;
				break;
			case 1:
				*pdwPad2 |= ( !(my_cond.buttons & CONT_B) != 0) << 2;
				break;
			case 2:
				*pdwPad2 |= ( !(my_cond.buttons & CONT_X) != 0) << 2;
				break;
			case 3:
				*pdwPad2 |= ( !(my_cond.buttons & CONT_Y) != 0) << 2;
				break;
			case 4:
				*pdwPad2 |= (my_cond.ltrig != 0) << 2;
				break;
		}
		if (*opt_P2AnalogEnabled)
		{
			*pdwPad2 |= 
				   ( (my_cond.joyy < 114) << 4) | // Up
				   ( (my_cond.joyy > 140) << 5) | // Down
				   ( (my_cond.joyx < 114) << 6) | // Left
				   ( (my_cond.joyx > 140) << 7); // Right
		}
		else
			*pdwPad2 |= 
				   ( ( !(my_cond.buttons & CONT_DPAD_UP    ) != 0 ) << 4) | // Up
				   ( ( !(my_cond.buttons & CONT_DPAD_DOWN  ) != 0 ) << 5) | // Down
				   ( ( !(my_cond.buttons & CONT_DPAD_LEFT  ) != 0 ) << 6) | // Left
				   ( ( !(my_cond.buttons & CONT_DPAD_RIGHT ) != 0 ) << 7); // Right


		*pdwPad2 = *pdwPad2 | ( *pdwPad2 << 8);
	}
	else
		*pdwPad2 = 0;
}

uint32* pNesX_MemoryCopy( uint32* dest, uint32* src, int count)
{
	while (count)
	{
		*dest++ = *src++;
		count-=4;
	}
	return dest;
}

//Inefficient and dirty
void *pNesX_MemorySet( void *dest, int c, int count)
{
	int i;
	int numdwords;
	uint32* bigdest;
	uint32 bigc;
	unsigned char* smalldest;
	bigc = c << 24 | c << 16 | c << 8 | c;
	bigdest = (uint32*) dest;
	smalldest = (unsigned char*) dest;

	numdwords = count / 4;
	for (i = 0; i < numdwords; i++)
		bigdest[i] = bigc;
	for (i = numdwords * 4; i < count; i++)
		smalldest[i] = c;

	return dest;
}

//Inefficient and dirty
void *pNesX_Uint32MemSet( void *dest, uint32 val, int count)
{
	int i;
	uint32* bigdest;

	bigdest = (uint32*) dest;

	for (i = 0; i < count; i++)
		bigdest[i] = val;

	return dest;
}

void Do_Prestore(uint32 address)
{
	asm("pref @r4");
}

void pNesX_Texture_Write( void *dest, void* src)
{
	int i;
	int j;
	volatile unsigned long *regs = (unsigned long*)0xff000038;
	unsigned long formatted_address;
	uint32* SQPtr;

	regs[0] = regs[1] = ((unsigned long)dest >> 24);
	SQPtr = ((uint32*) 0xe0000000);

	formatted_address = 0xe0000000 | (((unsigned long)dest) & 0x03FFFFFF); 

	//For the 16 transfers associated with one scanline = 16 transfers * 32 bytes = 512 bytes
	for (i = 0; i < 16; i++)
	{
		//32-byte copy to the correct area of the Storage Queues
		if ((i % 2) == 0) // SQ0 write
		{
			for (j = 0; j < 8; j++)
			{
				SQPtr[j] = ((uint32*) src)[(i*8) + j];
			}

			Do_Prestore(formatted_address);
		}
		else // SQ1 write
		{
			for (j = 0; j < 8; j++)
			{
				SQPtr[j + 8] = ((uint32*) src)[(i*8) + j];
			}

			Do_Prestore(formatted_address);
		}

		formatted_address += 32;
	}
}

void pNesX_Texture_Fill(void *dest, uint32 val)
{
	int i;
	int j;
	volatile unsigned long *regs = (unsigned long*)0xff000038;
	unsigned long formatted_address;
	uint32* SQPtr;

	regs[0] = regs[1] = ((unsigned long)dest >> 24);
	SQPtr = ((uint32*) 0xe0000000);

	formatted_address = 0xe0000000 | (((unsigned long)dest) & 0x03FFFFFF); 


	//Fill the SQ Buffer area with the fill value
	for (j = 0; j < 16; j++)
	{
		SQPtr[j] = val;
	}

	//For the 16 transfers associated with one scanline = 16 transfers * 32 bytes = 512 bytes
	for (i = 0; i < 16; i++)
	{
		Do_Prestore(formatted_address);
		formatted_address += 32;
	}
}

//No debugging support
void pNesX_DebugPrint( char *pszMsg )
{
}