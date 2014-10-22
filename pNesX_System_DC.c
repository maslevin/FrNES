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
#ifndef _arch_dreamcast
#define _arch_dreamcast
#endif

#include <kos.h>
#include "TextWindow.h"
#include "BMFont.h"

/*
#include "ROMLoad.h"
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
*/

/*-------------------------------------------------------------------*/
/*  Interface variables                                              */
/*-------------------------------------------------------------------*/
/*
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
*/

/*------------------------------------------------------------------*/
/*  GUI Customization Variables                                     */
/*------------------------------------------------------------------*/

/*
uint32 GUI_BGColor;
uint16 GUI_TextColor;
uint16 GUI_SelectedTextColor;
uint16 GUI_InsideWindowColor;
uint16 GUI_OutsideWindowColor;

Window_Style mystyle;
Window_Data mydata;
Window_Style helpstyle;
Window_Data helpdata;

#define NUM_CHARS 66

pvr_poly_hdr_t fontHeader;

const int title_offset_x = 25;
const int title_offset_y = 25;

char App_String[] = "FrNES";
char Version_String[] = "0.60 Final";

const int help_window_offset_y = 275;

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
void* PVR_Font_Offset;
void* PVR_MainWindow_Offset;
void* PVR_SmallWindow_Offset;
void* PVR_NESScreen1_Offset;
void* PVR_NESScreen2_Offset;
void* PVR_White_Font_Offset;
void* PVR_TitleScreen_Offset;
unsigned char isMainChanged;
unsigned char isSmallChanged;

unsigned char PVR_Font_Widths[NUM_CHARS];
unsigned char PVR_Font_Heights[NUM_CHARS];
pvr_poly_hdr_t my_pheader;
pvr_vertex_t my_vertex;

pvr_poly_hdr_t* blackFont;
pvr_poly_hdr_t* whiteFont;
pvr_poly_hdr_t nesScreen1;
pvr_poly_hdr_t nesScreen2;
pvr_poly_hdr_t nesScreen1_filtered;
pvr_poly_hdr_t nesScreen2_filtered;
*/
/*

//ROM data storage
const int MAX_ROMS = 2048;
RomInfo myRomInfos[2048];
char* myRomStrings[2048];

//The crc32 of the currently selected rom
uint32 currentCRC32;

//Control Variables
uint32 oldState;
int nWait; */

/*-------------------------------------------------------------------*/
/*  ROM image file information                                       */
/*-------------------------------------------------------------------*/

/*
char szRomName[ 256 ];
char szSaveName[ 256 ];
int nSRAM_SaveFlag;
*/

/*-------------------------------------------------------------------*/
/*  Variables for Windows                                            */
/*-------------------------------------------------------------------*/

/*
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
	//TODO: controllers need to be reimplemented
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
}*/

pvr_init_params_t pvr_params = {
    /* Enable opaque and translucent polygons with size 16 */
    { PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_0 },

    /* Vertex buffer size 512K */
    512 * 1024
};

pvr_poly_cxt_t cxt;
pvr_poly_hdr_t hdr;
int createdHeader;

pvr_ptr_t whiteTexture;

void pvr_setup() {
	createdHeader = 0;
    pvr_init(&pvr_params);
    pvr_set_bg_color(0, 0, 0);

	whiteTexture = pvr_mem_malloc(2 * 256 * 256);
	uint16* textureBuffer = malloc(2 * 256 * 256);
	//memset((char*)textureBuffer, 0xFF, 2 * 256 * 256);
	int i;
	for (i = 0; i < 2 * 256 * 256; i++) {
		if (i % 4 < 2) {
			textureBuffer[i] = 0x00;
		} else {
			textureBuffer[i] = 0xFF;
		}
	}
	pvr_txr_load(textureBuffer, whiteTexture, 2 * 256 * 256);
}

extern uint8 romdisk[];

KOS_INIT_FLAGS(INIT_DEFAULT);	
KOS_INIT_ROMDISK(romdisk);

uint16 MakeRGB(int red, int green, int blue) {
	return ((red << 10 | green << 5 | blue << 0) | 0x8000);
}

//TEST - try to get the PVR system displaying a rainbow rectangle
void draw_screen(BMFont* font) {
	pvr_vertex_t my_vertex;

	pvr_wait_ready();
	pvr_scene_begin();

	pvr_list_begin(PVR_LIST_OP_POLY);

	pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
   	cxt.gen.shading = 1;
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));	

	my_vertex.flags = PVR_CMD_VERTEX;
	my_vertex.x = 0.0f;
	my_vertex.y = 480.0f;
	my_vertex.z = 1.0f;
	my_vertex.argb = PVR_PACK_COLOR(1.0f, 1.0f, 0.0f, 0.0f);
	my_vertex.oargb = 0;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.y = 0.0f;
	my_vertex.argb = PVR_PACK_COLOR(1.0f, 0.0f, 1.0f, 0.0f);	
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.x = 640.0f;
	my_vertex.y = 480.0f;
	my_vertex.argb = PVR_PACK_COLOR(1.0f, 0.0f, 0.0f, 1.0f);	
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = PVR_CMD_VERTEX_EOL;
	my_vertex.y = 0;
	my_vertex.argb = PVR_PACK_COLOR(1.0f, 0.5f, 0.5f, 0.5f);	
	pvr_prim(&my_vertex, sizeof(my_vertex));

	drawString(font, PVR_LIST_OP_POLY, "Test", 10.0f, 30.0f);

	pvr_list_finish();

	pvr_list_begin(PVR_LIST_TR_POLY);

	drawString(font, PVR_LIST_TR_POLY, "Test", 10.0f, 10.0f);

	pvr_list_finish();

/*
	pvr_list_begin(PVR_LIST_OP_POLY);

	if (createdHeader == 0) {
		pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
    	//cxt.gen.shading = 1;
    	//cxt.txr.enabled = 1;
		BMFPage* page = (BMFPage*)utarray_eltptr(font -> pages, 0);
		printf("Page 0 texture width is %i x %i\n", (short)page -> width, (short)page -> height);
		printf("Page 0 texture is 0x%08x\n", page -> texture);
		pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, 256, 256, page -> texture, PVR_FILTER_NONE); 
    	pvr_poly_compile(&hdr, &cxt);
    	createdHeader = 1;
    }

	pvr_prim(&hdr, sizeof(hdr));

	my_vertex.flags = PVR_CMD_VERTEX;
	my_vertex.x = 0.0f;
	my_vertex.y = 480.0f;
	my_vertex.z = 1.0f;
	my_vertex.u = 0.0f;
	my_vertex.v = 1.0f;
	my_vertex.argb = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
	//my_vertex.argb = 0;
	my_vertex.oargb = 0;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.y = 0.0f;
	my_vertex.v = 0.0f;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.x = 640.0f;
	my_vertex.y = 480.0f;
	my_vertex.u = 1.0f;
	my_vertex.v = 1.0f;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = PVR_CMD_VERTEX_EOL;
	my_vertex.y = 0;
	my_vertex.v = 0.0f;
	pvr_prim(&my_vertex, sizeof(my_vertex));
*/
/*
	BMFPage* page = (BMFPage*)utarray_eltptr(font -> pages, 0);
	pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, page -> width, page -> height, page -> texture, PVR_FILTER_NONE);
	pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));

	my_vertex.flags = PVR_CMD_VERTEX;
	my_vertex.x = 0.0f;
	my_vertex.y = 480.0f;
	my_vertex.z = 1.0f;
	my_vertex.u = 0.0f;
	my_vertex.v = 1.0f;
	my_vertex.argb =  PVR_PACK_COLOR(0.5f, 0.0f, 0.75f, 1.0f);
	my_vertex.oargb = 0;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.y = 0.0f;
	my_vertex.v = 0.0f;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.x = 640.0f;
	my_vertex.y = 480.0f;
	my_vertex.u = 1.0f;
	my_vertex.v = 1.0f;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = PVR_CMD_VERTEX_EOL;
	my_vertex.y = 0;
	my_vertex.v = 0.0f;
	pvr_prim(&my_vertex, sizeof(my_vertex));
*/

//	pvr_list_finish();

	pvr_scene_finish();	
}

/*
void draw_screen()
{
	pvr_poly_hdr_t my_pheader;
	pvr_poly_cxt_t cxt;
	pvr_vertex_t my_vertex;
	pvr_vertex_t my_c_vertex;

	if (isMainChanged)
	{
		win_draw_textwindow(&mydata, &mystyle);
		isMainChanged = 0;
	}
	if (isSmallChanged)
	{
		win_draw_textwindow(&helpdata, &helpstyle);
		isSmallChanged = 0;
	}

	pvr_wait_ready();
	pvr_scene_begin();

	//Draw the Screen ... will be more fancy later
	pvr_list_begin(PVR_LIST_OP_POLY);

	my_c_vertex.flags = PVR_CMD_VERTEX;
	my_c_vertex.x = 0.0f;
	my_c_vertex.y = 480.0f;
	my_c_vertex.z = 25.0f;
	my_c_vertex.argb = PVR_PACK_COLOR(1.0f,
							((float)(GUI_BGColor >> 16) / 255),
							((float)(0x00FF & (GUI_BGColor >> 8)) / 255),
							((float)(0x00FF & GUI_BGColor) / 255));
	pvr_prim(&my_c_vertex, sizeof(my_c_vertex));

	my_c_vertex.y = 0.0f;
	pvr_prim(&my_c_vertex, sizeof(my_c_vertex));

	my_c_vertex.x = 640.0f;
	my_c_vertex.y = 480.0f;
	pvr_prim(&my_c_vertex, sizeof(my_c_vertex));

	my_c_vertex.flags = PVR_CMD_VERTEX_EOL;
	my_c_vertex.y = 0.0f;
	pvr_prim(&my_c_vertex, sizeof(my_c_vertex));

	pvr_list_begin(PVR_LIST_TR_POLY);

	pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED, 512, 512, PVR_MainWindow_Offset, PVR_FILTER_BILINEAR);
	pvr_poly_compile(&my_pheader, &cxt);

	my_vertex.flags = PVR_CMD_VERTEX;
	my_vertex.x = 200.0f;
	my_vertex.y = 440.0f;
	my_vertex.z = 30.0f;
	my_vertex.u = 0.0f;
	my_vertex.v = ((float) mydata.height) / 512.0f;
	my_vertex.argb = PVR_PACK_COLOR(0.75f, 1.0f, 1.0f, 1.0f);
	my_vertex.oargb = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.y = 20.0f;
	my_vertex.u = 0.0f;
	my_vertex.v = 0.0f;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.x = 620.0f;
	my_vertex.y = 440.0f;
	my_vertex.u = ((float) mydata.width - 7) / 512.0f;
	my_vertex.v = ((float) mydata.height) / 512.0f;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = PVR_CMD_VERTEX_EOL;
	my_vertex.y = 20.0f;
	my_vertex.u = ((float) mydata.width - 7) / 512.0f;
	my_vertex.v = 0.0f;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED, 512, 512, PVR_SmallWindow_Offset, PVR_FILTER_BILINEAR);
	pvr_poly_compile(&my_pheader, &cxt);	

	my_vertex.flags = PVR_CMD_VERTEX;
	my_vertex.x = 20.0f;
	my_vertex.y = 440.0f;
	my_vertex.z = 30.0f;
	my_vertex.u = 0.0f;
	my_vertex.v = ((float) helpdata.height) / 512.0f;
	my_vertex.argb = PVR_PACK_COLOR(0.75f, 1.0f, 1.0f, 1.0f);
	my_vertex.oargb = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.y = 280.0f;
	my_vertex.u = 0.0f;
	my_vertex.v = 0.0f;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.x = 190.0f;
	my_vertex.y = 440.0f;
	my_vertex.u = ((float) helpdata.width - 7) / 512.0f;
	my_vertex.v = ((float) helpdata.height) / 512.0f;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = PVR_CMD_VERTEX_EOL;
	my_vertex.y = 280.0f;
	my_vertex.u = ((float) helpdata.width - 7) / 512.0f;
	my_vertex.v = 0.0f;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	bf_ta_submit_string(&fontHeader, title_offset_x, title_offset_y, 32.0f, 150, 35, PVR_Font_Widths, PVR_Font_Heights, App_String);
	bf_ta_submit_string(&fontHeader, title_offset_x, title_offset_y + 35, 32.0f, 160, 20, PVR_Font_Widths, PVR_Font_Heights, Version_String);

	pvr_scene_finish();
}

void setup_main_menu()
{
	//Set Up Window Data Features
	mydata.x = 0;
	mydata.y = 0;
	mydata.width = 400;
	mydata.height = 460 - (2 * title_offset_y);
	mydata.Target_Width = 512;
	mydata.Header_Font = NULL;
	mydata.Item_Font = NULL;
	mydata.Header_Text = Main_Menu;
	mydata.Data_Strings = Main_Options;
	mydata.Num_Strings = Num_Main;
	mydata.Highlighted_Index = 0;
	mydata.Top_Index = 0;
	mydata.Target_Buffer = PVR_MainWindow_Offset;
	
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
	helpdata.Header_Font = NULL;
	helpdata.Item_Font = NULL;
	helpdata.Header_Text = " ";
	helpdata.Data_Strings = Main_Keys;
	helpdata.Num_Strings = Num_Main_Keys;
	helpdata.Highlighted_Index = Num_Main_Keys;
	helpdata.Top_Index = 0;
	helpdata.Target_Buffer = PVR_SmallWindow_Offset;
}
*/
/*
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
	mydata.Target_Buffer = PVR_MainWindow_Offset;

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
	helpdata.Target_Buffer = PVR_SmallWindow_Offset;
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
	helpdata.Data_Strings = Text_Keys;
	helpdata.Num_Strings = Num_Text_Keys;
	helpdata.Highlighted_Index = Num_Text_Keys;
	helpdata.Top_Index = 0;
	helpdata.Target_Buffer = PVR_SmallWindow_Offset;
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
	helpdata.Header_Text = "";
	helpdata.Data_Strings = Text_Keys;
	helpdata.Num_Strings = Num_Text_Keys;
	helpdata.Highlighted_Index = 0;
	helpdata.Top_Index = 0;
	helpdata.Target_Buffer = PVR_SmallWindow_Offset;
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
}*/

/*
void initializeMemory()
{
	printf("Allocating Options Memory\n");
	Allocate_Video_Options();
	Allocate_System_Options();
	Allocate_Control_Options();

	//Some Memory Areas
	printf("Allocating PVR Memory\n");
	PVR_Font_Offset = pvr_mem_malloc(540672);
	PVR_MainWindow_Offset = pvr_mem_malloc(512 * 512 * 2);
	PVR_SmallWindow_Offset = pvr_mem_malloc(512 * 512 * 2);

	//32-byte align so the Store Queues can write to it.
	PVR_NESScreen1_Offset = pvr_mem_malloc(256 * 256 * 2);
	PVR_NESScreen2_Offset = pvr_mem_malloc(256 * 256 * 2);
	PVR_White_Font_Offset = pvr_mem_malloc(540672);

	//Use the MainWindow Texture for Title Screen since both won't ever be used at the same time
	PVR_TitleScreen_Offset = PVR_MainWindow_Offset;	
}
*/

/*
void createPVRHeaders()
{
	pvr_poly_cxt_t cxt;

	printf("Compiling PVR Headers\n");
	//TODO : move all PVR texture header creation here
    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
                     256, 256, PVR_NESScreen1_Offset, PVR_FILTER_NONE);
    pvr_poly_compile(&nesScreen1, &cxt);
    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
                     256, 256, PVR_NESScreen2_Offset, PVR_FILTER_NONE);
    pvr_poly_compile(&nesScreen2, &cxt);
    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
                     256, 256, PVR_NESScreen1_Offset, PVR_FILTER_BILINEAR);
    pvr_poly_compile(&nesScreen1_filtered, &cxt);
    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
                     256, 256, PVR_NESScreen2_Offset, PVR_FILTER_BILINEAR);
    pvr_poly_compile(&nesScreen2_filtered, &cxt);
}
*/

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
	maple_device_t* cont;
	cont_state_t* state;

	printf("FrNES 1.0/KallistiOS 2.0.0 starting\n");

	printf("Initializing PVR\n");
//	pvr_init(&params);
	pvr_setup();

	printf("Initialized PVR\n");

//	initializeMemory();

/*
	printf("Setting up main menu\n");
	setup_main_menu();
*/

	printf("Loading font files\n");
//	bf_load_file_pvr_colors("/FONTS/LGFONT.BMF", PVR_Font_Offset, 64 * 64 * 2, PVR_Font_Widths, PVR_Font_Heights, 0x0000, 0x8000);
//	bf_load_file_pvr_colors("/rd/FONTS/LGFONT.BMF", PVR_White_Font_Offset, 64 * 64 * 2, PVR_Font_Widths, PVR_Font_Heights, 0x0000, 0xFFFF, &fontHeader);

	BMFont* font = load_BMF("/rd/neuro.fnt");
	if (font == NULL) {
		printf("Unable to load font\n");
	} else {
		printf("Loaded font\n");
	}

	printf("Font baseline : %i\n", (int)font -> base);

	//Christmas Colors
/*	GUI_BGColor = 0x0000FF80;
	GUI_TextColor = 0x8000;
	GUI_SelectedTextColor = MakeRGB(13, 28, 8);
	GUI_InsideWindowColor = MakeRGB(31, 8, 8);
	GUI_OutsideWindowColor = MakeRGB(31, 31, 31);

	isMainChanged = 1;
	isSmallChanged = 1;*/

    while(1) {
        if((cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER)) != NULL) {
            state = (cont_state_t *)maple_dev_status(cont);

            if(state == NULL)
                break;

            if(state->buttons & CONT_START)
                break;
        }

        draw_screen(font);
    }

    printf("Unloading font\n");
    destroy_BMF(font);

//	initialize_controllers();
//	rescan_controllers();

	//Load Fonts
/*	fs_iso9660_init();
	bf_load_file("/FONTS/LGFONT.BMF", largefont);
	bf_load_file("/FONTS/SMLFONT.BMF", smallfont);
	result = bf_load_file("/FONTS/MEDFONT.BMF", medfont);

	//Load Fonts from CD into PVR memory
	bf_load_file_pvr_colors("/FONTS/LGFONT.BMF", PVR_Font_Offset, 64 * 64 * 2, PVR_Font_Widths, PVR_Font_Heights, 0x0000, 0x8000);
	bf_load_file_pvr_colors("/FONTS/LGFONT.BMF", PVR_White_Font_Offset, 64 * 64 * 2, PVR_Font_Widths, PVR_Font_Heights, 0x0000, 0xFFFF);

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

	printf("Starting Shutdown\n");

	printf("Freeing Options Memory\n");
	Free_Video_Options();
	Free_System_Options();
	Free_Control_Options();

	printf("Freeing PVR Memory\n");
	pvr_mem_free(PVR_MainWindow_Offset);
	pvr_mem_free(PVR_SmallWindow_Offset);
	pvr_mem_free(PVR_Font_Offset);
	pvr_mem_free(PVR_White_Font_Offset);
	pvr_mem_free(PVR_NESScreen1_Offset);
	pvr_mem_free(PVR_NESScreen2_Offset);

	printf("Shutting down PVR\n");
	pvr_shutdown();
/*	bf_free_font(medfont);
	bf_free_font(largefont);*/
	return 0;
}

/*
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
	pvr_wait_ready();
	pvr_scene_begin();
	pvr_list_begin(PVR_LIST_OP_POLY);

	if (*opt_Filter)
	{
		if (WorkFrameIdx)
			pvr_prim(&nesScreen1_filtered, sizeof(nesScreen1_filtered));
		else
			pvr_prim(&nesScreen2_filtered, sizeof(nesScreen2_filtered));			
	}
	else
	{
		if (WorkFrameIdx)
			pvr_prim(&nesScreen1, sizeof(nesScreen1));
		else
			pvr_prim(&nesScreen2, sizeof(nesScreen2));
	}

	if (*opt_Stretch)
	{
		my_vertex.flags = PVR_CMD_VERTEX;
		my_vertex.x = 0.0f;
		my_vertex.y = 480.0f;
		my_vertex.z = 30.0f;
		my_vertex.u = ((float) (opt_ClipVars[0])) / 256.0f;
		my_vertex.v = ((float) 240 - (opt_ClipVars[3])) / 256.0f;
		my_vertex.argb = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
		my_vertex.oargb = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.y = 0.0f;
		my_vertex.v = ((float) (opt_ClipVars[2])) / 256.0f;
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.x = 640.0f;
		my_vertex.y = 480.0f;
		my_vertex.u = ((float) 256 - (opt_ClipVars[1])) / 256.0f;
		my_vertex.v = ((float) 240 - (opt_ClipVars[3])) / 256.0f;
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.flags = PVR_CMD_VERTEX_EOL;
		my_vertex.y = 0.0f;
		my_vertex.v = ((float) (opt_ClipVars[2])) / 256.0f;
		pvr_prim(&my_vertex, sizeof(my_vertex));	
	}
	else
	{
		my_vertex.flags = PVR_CMD_VERTEX;
		my_vertex.x = 64.0f;
		my_vertex.y = 480.0f;
		my_vertex.z = 30.0f;
		my_vertex.u = ((float) (opt_ClipVars[0])) / 256.0f;
		my_vertex.v = ((float) 240 - (opt_ClipVars[3])) / 256.0f;
		my_vertex.argb = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
		my_vertex.oargb = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.y = 0.0f;
		my_vertex.v = ((float) (opt_ClipVars[2])) / 256.0f;
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.x = 576.0f;
		my_vertex.y = 480.0f;
		my_vertex.u = ((float) 256 - (opt_ClipVars[1])) / 256.0f;
		my_vertex.v = ((float) 240 - (opt_ClipVars[3])) / 256.0f;
		pvr_prim(&my_vertex, sizeof(my_vertex));

		my_vertex.flags = PVR_CMD_VERTEX_EOL;
		my_vertex.y = 0.0f;
		my_vertex.v = ((float) (opt_ClipVars[2])) / 256.0f;
		pvr_prim(&my_vertex, sizeof(my_vertex));
	}

	pvr_scene_finish();
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
} */