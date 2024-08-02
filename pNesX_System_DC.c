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
#include "macros.h"

#include <bzlib/bzlib.h>

#include "pNesX_System.h"
#include "pNesX_System_DC.h"
#include "pNesX_Utils.h"
#include "vmu_icons.h"
#include "GUI_MainMenu.h"
#include "GUI_FileBrowser.h"
#include "GUI_Credits.h"
#include "GUI_Help.h"
#include "GUI_VideoPage.h"
#include "GUI_SystemPage.h"
#include "GUI_ControlPage.h"
#include "GUI_GUIPage.h"

#include "K6502.h"

extern uint8 romdisk[];
KOS_INIT_FLAGS(INIT_DEFAULT | INIT_VMU);

bool log_enabled;
bool log_enabled_latch;

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
int disable_trigs;

int romselstatus;
int disable_rom_interface;

/*------------------------------------------------------------------*/
/*  GUI Customization Variables                                     */
/*------------------------------------------------------------------*/
uint32 GUI_BGColor;
uint32 GUI_TextColor;
uint32 GUI_SelectedTextColor;
uint32 GUI_InsideWindowColor;
uint32 GUI_OutsideWindowColor;

const int title_offset_x = 25;
const int title_offset_y = 25;

const int help_window_offset_y = 275;

Window_Style mystyle;
Window_Data mydata;

Window_Style helpstyle;
Window_Data helpdata;

//Rom buffer area ... uses dynamic memory allocation
unsigned char *ROM;
unsigned char *VROM;
unsigned char *VRAM;
uint32 SRAM_Enabled;

char App_String[] = "FrNES";
char Version_String[] = "0.7.0";

//Help for the Options screen
char* Options_Keys[] = {
	"A - Toggle",
	"DPad L and R to Change"
};
const int Num_Options_Keys = 2;

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

Font* font;

VQ_Texture* PVR_NESScreen1_Offset;
VQ_Texture* PVR_NESScreen2_Offset;
unsigned char* codebook;

pvr_poly_hdr_t my_pheader;
pvr_vertex_t my_vertex;
pvr_poly_cxt_t my_cxt;

//The crc32 of the currently selected rom
uint32 currentCRC32;

//Control Variables
uint32 oldState;
int nWait;

/*-------------------------------------------------------------------*/
/*  ROM image file information                                       */
/*-------------------------------------------------------------------*/

char szRomPath[ 256 ];
// char szRomName[ 256 ];
char szSaveName[ 256 ];
int nSRAM_SaveFlag;
uint32 RomSize;
bool AutoROM;

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
uint32 numControllers;
maple_device_t* Controllers[4];

//VMUs - will store VMU addresses -- NULL if not found
uint32 numVMUs;
maple_device_t* VMUs[8];

void initialize_controllers() {
	printf("initialize_controllers: start scan\n");
	numControllers = 0;
	for (int i = 0; i < 4; i++) {
		maple_device_t* controller = maple_enum_type(i, MAPLE_FUNC_CONTROLLER);
		if (controller != NULL) {
			Controllers[i] = controller;
			numControllers++;
		}
	}

	numVMUs = 0;
	for (int i = 0; i < 8; i++) {
		maple_device_t* vmu = maple_enum_type(i, MAPLE_FUNC_MEMCARD);
		if (vmu != NULL) {
			VMUs[i] = vmu;
			numVMUs++;
		}
	}
	printf("initialize_controllers: end scan, found [%lu] controllers and [%lu] vmus\n", numControllers, numVMUs);
}

uint16 MakeRGB(int red, int green, int blue) {
	return ((red << 10 | green << 5 | blue << 0) | 0x8000);
}

void draw_screen() {
	//printf("draw_screen: begin\n");
	pvr_poly_hdr_t my_pheader;
	pvr_poly_cxt_t my_cxt;
	//pvr_vertex_t my_vertex;
	pvr_vertex_t my_c_vertex;

	//printf("draw_screen: waiting for PVR to be ready\n");
	pvr_wait_ready();
	pvr_scene_begin();

	// STEP 1: Draw an untextured opaque screen-sized polygon
	//printf("draw_screen: drawing background polygon\n");
	pvr_list_begin(PVR_LIST_OP_POLY);
	pvr_poly_cxt_col(&my_cxt, PVR_LIST_OP_POLY);

	if (menuscreen != MENUNUM_AUTOROM) {
		pvr_poly_compile(&my_pheader, &my_cxt);
		pvr_prim(&my_pheader, sizeof(my_pheader));
		//Draw the Screen ... will be more fancy later

		my_c_vertex.flags = PVR_CMD_VERTEX;
		my_c_vertex.x = 0.0f;
		my_c_vertex.y = 480.0f;
		my_c_vertex.z = 25.0f;
		my_c_vertex.argb = GUI_BGColor;
		my_c_vertex.oargb = 0;
		pvr_prim(&my_c_vertex, sizeof(my_c_vertex));

		my_c_vertex.y = 0.0f;
		pvr_prim(&my_c_vertex, sizeof(my_c_vertex));

		my_c_vertex.x = 640.0f;
		my_c_vertex.y = 480.0f;
		pvr_prim(&my_c_vertex, sizeof(my_c_vertex));

		my_c_vertex.flags = PVR_CMD_VERTEX_EOL;
		my_c_vertex.y = 0.0f;
		pvr_prim(&my_c_vertex, sizeof(my_c_vertex));

		win_draw_textwindow(&mydata, &mystyle, PVR_LIST_OP_POLY);
		win_draw_textwindow(&helpdata, &helpstyle, PVR_LIST_OP_POLY);
		pvr_list_finish();
	}

	// STEP 2: Draw the UI as two translucent textures over top of that previous texture
	//printf("draw_screen: drawing main window\n");
	pvr_list_begin(PVR_LIST_TR_POLY);

	if (menuscreen != MENUNUM_AUTOROM) {
		draw_string(font, PVR_LIST_TR_POLY, App_String, 32.0f, 32.0f, 30.0f, 200.0f, 50.0f, SINGLE, LEFT, 0xFF000000, 1.0f);
		font -> scale = 0.50f;
		draw_string(font, PVR_LIST_TR_POLY, Version_String, 32.0f, 54.0f, 30.0f, 200.0f, 50.0f, SINGLE, LEFT, 0xFF000000, 0.75f);
		font -> scale = 1.00f;

		win_draw_textwindow(&mydata, &mystyle, PVR_LIST_TR_POLY);
		win_draw_textwindow(&helpdata, &helpstyle, PVR_LIST_TR_POLY);
	}

	pvr_list_finish();
	//printf("draw_screen: finishing scene\n");
	pvr_scene_finish();
}

int LoadSRAM() {
	int loadSRAM_success = -1;
	if (*opt_SRAM == 1) {
		printf("VMU: Attempting to Load SRAM from attached VMUs\n");
		for (int i = 0; i < 8; i++) {
			maple_device_t* vmu = maple_enum_type(i, MAPLE_FUNC_MEMCARD);
			if (vmu == NULL)
				break;
			draw_VMU_icon(vmu, vmu_screen_loading);

			char sramFilename[13];
			snprintf(sramFilename, 13, "%08lx", currentCRC32);

			unsigned char* readBuffer;
			int readBufferSize;
			if (vmufs_read(vmu, sramFilename, (void**)&readBuffer, &readBufferSize) == 0) {
				vmu_pkg_t package;
				if (vmu_pkg_parse(readBuffer, &package) == 0) {
					printf("VMU: Found SRAM Save File from VMU [%i]\n", i);

					unsigned int destLength = 0x2000;
					int result = BZ2_bzBuffToBuffDecompress((char*)SRAM, &destLength, (char*)package.data, readBufferSize - sizeof(vmu_pkg_t), 0, 0);
					if (result != BZ_OK) {
						printf("VMU: bz2 decompress error [%i]\n", result);
					}
					loadSRAM_success = 1;				
					draw_VMU_icon(vmu, vmu_screen_normal);
				} else {
					printf("VMU: Package failed CRC check\n");
					draw_VMU_icon(vmu, vmu_screen_error);
				}

				free(readBuffer);
				break;
			} else {
				printf("VMU: Unable to load SRAM Save File from VMU [%i]\n", i);
				draw_VMU_icon(vmu, vmu_screen_error);
			}
		}
	}
	return loadSRAM_success;
}

#define APP_STRING "FrNES"
#define APP_VERSION "0.7.0"

int SaveSRAM() {
	int saveSRAM_success = -1;
	if (*opt_SRAM == 1) {
		for (int i = 0; i < numVMUs; i++) {
			printf("VMU: Attempting to Save SRAM to VMU [%i]\n", i);
			maple_device_t* vmu = maple_enum_type(i, MAPLE_FUNC_MEMCARD);
			if (vmu == NULL)
				break;
			draw_VMU_icon(vmu, vmu_screen_saving);

			printf("VMU: Generating filename\n");
			char sramFilename[13];
			snprintf(sramFilename, 13, "%08lx", currentCRC32);

			printf("VMU: Compressing SRAM buffer\n");
			const unsigned char compressedBuffer[0x2200 + 600];
			unsigned int compressedLength = 0x2200 + 600;
			int result = BZ2_bzBuffToBuffCompress((char*)compressedBuffer, &compressedLength, (char*)SRAM, 0x2000, 9, 0, 0);

			if (result != BZ_OK) {
				printf("VMU: bz2 Compression Failed [%i]\n", result);
				break;
			} else {
				printf("VMU: bz2 Compression Succeeded [%i bytes]\n", compressedLength);
			}

			printf("VMU: Generating VMU Package\n");
			vmu_pkg_t package;
			// Short Description
			char description[20];
			snprintf(description, 20, "%s %s SAVE", APP_STRING, APP_VERSION);
			strcpy(package.desc_short, description);

			// Long Description
			char long_description[255];
			char* beginRomName = szRomPath;
			for (int i = 0; i < strlen(szRomPath); i++) {
				if (szRomPath[i] == '/') {
					beginRomName = &(szRomPath[i + 1]);
				}
			}
			strcpy(long_description, beginRomName);
			for (int i = strlen(long_description); i >= 0; i--) {
				if (long_description[i] == '.') {
					long_description[i] = '\0';
				}
			}
			if (strlen(long_description) >= 36) {
				long_description[35] = '\0';
			}
			strcpy(package.desc_long, long_description);

			// App Id
			snprintf(package.app_id, 20, "%s %s", APP_STRING, APP_VERSION);

			// Final Package Fields
			package.icon_cnt = 0;
			package.icon_anim_speed = 0;
			package.eyecatch_type = VMUPKG_EC_NONE;
			memset(&package.icon_pal, 0, 16 * 2);
			package.data_len = compressedLength;
			package.icon_data = NULL;
			package.eyecatch_data = NULL;
			package.data = compressedBuffer;	

			printf("VMU: Compiling Package\n");
			unsigned char* packageBuffer;
			int packageBufferLength;
			if (vmu_pkg_build(&package, &packageBuffer, &packageBufferLength) == 0) {
				printf("VMU: Saving Package to VMU as [%s]\n", sramFilename);
				if (vmufs_write(vmu, sramFilename, packageBuffer, packageBufferLength, VMUFS_OVERWRITE) == 0) {
					printf("VMU: Saved SRAM Save File to VMU [%i]\n", i);
					saveSRAM_success = 1;
					draw_VMU_icon(vmu, vmu_screen_normal);
					break;
				} else {
					printf("VMU: Unable to save SRAM Save File to VMU [%i]\n", i);
					draw_VMU_icon(vmu, vmu_screen_error);				
				}

				free(packageBuffer);
			} else {
				printf("VMU: Error Compiling package\n");
			}
		}
	}
	return saveSRAM_success;
}

void Save_VMU_Options()
{
	//Menu Screen = Save Options
	/*	
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
	*/
}

void Load_VMU_Options()
{
	//Menu Screen = Load Options
	/*
	if (*opt_VMUPort != -1)
	{
		vmu_icon_draw(vmu_screen_loading, VMUs[*opt_VMUPort]);
		if (load_user_settings(VMUs[*opt_VMUPort]) == -1)
			vmu_icon_draw(vmu_screen_error, VMUs[*opt_VMUPort]);
		else
			vmu_icon_draw(vmu_screen_normal, VMUs[*opt_VMUPort]);
	}
	*/
}

pvr_init_params_t pvr_params =  {
    /* Enable opaque and translucent polygons with size 16 */
    { PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_0 },

    /* Vertex buffer size 512K */
    512 * 1024
};

void pvr_setup() {
    pvr_init(&pvr_params);
    pvr_set_bg_color(0, 0, 0);
}

void initVQTextures() {
	PVR_NESScreen1_Offset = (VQ_Texture*)pvr_mem_malloc(sizeof(VQ_Texture));
	PVR_NESScreen2_Offset = (VQ_Texture*)pvr_mem_malloc(sizeof(VQ_Texture));
 	codebook = memalign(32, 2048);	
}

bool checkForAutoROM() {
	bool autoromPresent = false;
	memset(szRomPath, 0, 256);
	file_t autoromFileHandle = fs_open("/rd/autorom.txt", O_RDONLY);
	if (autoromFileHandle != -1) {
		printf("AutoROM: autorom.txt found\n");
		if (fs_read(autoromFileHandle, szRomPath, 256) > 0) {
			for (int i = 0; i < strlen(szRomPath); i++) {
				if (szRomPath[i] == '\n') {
					szRomPath[i] = '\0';
				}
			}
			printf("AutoROM: file specified [%s]\n", szRomPath);
			fs_close(autoromFileHandle);

			file_t romFileHandle = fs_open(szRomPath, O_RDONLY);
			if (romFileHandle != -1){
				struct stat fileStat;
				if (fs_fstat(romFileHandle, &fileStat) == 0) {
					printf("AutoROM: detected size [%lu]\n", fileStat.st_size);
					RomSize = fileStat.st_size;
					autoromPresent = true;
				}
				fs_close(romFileHandle);
			} else {
				printf("AutoROM: unable to load specified rom\n");
			}
		}
		fs_close(autoromFileHandle);
	} else {
		printf("autorom.txt not present\n");
	}
	return autoromPresent;
}


/*===================================================================*/
/*                                                                   */
/*                dc_main() : Application main                       */
/*                                                                   */
/*===================================================================*/
int main() {
	printf("Starting Main\n");
	cont_state_t* my_state;
	log_enabled = false;
	log_enabled_latch = false;

	// System initiation
	printf("Initializing PVR\n");
	pvr_setup();

	//Load Fonts
	printf("Initializing Fonts\n");
	font = load_font("/rd/neuro.fnt");

	printf("Initializing FrNES GUI\n");	
	Allocate_Video_Options();
	Allocate_System_Options();
	Allocate_Control_Options();

	printf("Initializing VQ Textures\n");
	initVQTextures();

	printf("Initializing Controllers and VMUs\n");	
	initialize_controllers();

	GUI_BGColor = 0xFF0080FF;
	GUI_TextColor = 0xFF000000;
	GUI_SelectedTextColor = 0xFF00FF00;
	GUI_InsideWindowColor = 0xFF00803f;
	GUI_OutsideWindowColor = 0xFFFFFFFF;

	interface_offset = title_offset_x + 160;

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

	printf("Initializing VMUs\n");
	for (i = 0; i < numVMUs; i++) {
//		draw_VMU_icon(VMUs[i], vmu_screen_normal);
	}

	//If the default Memory card is present
	/*
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
	*/

	printf("Checking for autoROM\n");
	AutoROM = checkForAutoROM();
	if (AutoROM) {
		menuscreen = MENUNUM_AUTOROM;
	} else {
		menuscreen = MENUNUM_MAIN;
	}


	printf("Setting up main menu\n");
	setup_main_menu_screen();

	keyhit = 0;

	int exitLoop = 0;
	maple_device_t* my_controller;
	while (exitLoop == 0) {
//		printf("main loop: start\n");

//		printf("main loop: reading controller\n");
		my_controller = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
		if (my_controller != NULL) {
			my_state = (cont_state_t*)maple_dev_status(my_controller);

			if ((my_state == NULL) || (my_state -> buttons & CONT_START)) {
				exitLoop = 1;
			}

			if (invalida && ((my_state -> buttons & CONT_A) == 0)) {
				invalida = 0;
//				printf("Clearing A\n");
			}

			if (keyhit && ((my_state -> buttons & (CONT_DPAD_UP | CONT_DPAD_DOWN)) == 0)) {
				keyhit = 0;
//				printf("Clearing Keyhit\n");
			}

			if (xkeyhit && ((my_state -> buttons & CONT_X) == 0)) {
				xkeyhit = 0;
//				printf("Clearing X\n");
			}

			if (disable_trigs && ((my_state -> rtrig == 0) && (my_state -> ltrig == 0))) {
				disable_trigs = 0;
//				printf("Clearing Triggers\n");
			}

//			printf("main loop: handling controller input\n");
/*
			printf("STATE: U[%s] D[%s] L[%s] R[%s]\n", 
				(my_state -> buttons & CONT_DPAD_UP) ? "X" : ".",
				(my_state -> buttons & CONT_DPAD_DOWN) ? "X" : ".",
				(my_state -> buttons & CONT_DPAD_LEFT) ? "X" : ".",
				(my_state -> buttons & CONT_DPAD_RIGHT) ? "X" : "."
			);
*/			
			//Event handling and processing section
			switch (menuscreen)
			{
				//Main Menu Screen
				case MENUNUM_MAIN:
//					printf("main loop: handling menu screen controller\n");
					Handle_Main_Menu_Interface(my_state);
					break;

				//GUI Options
				case MENUNUM_GUIOPTIONS:
//					printf("main loop: handling gui options controller\n");
					Handle_GUI_Interface(my_state);
					break;

				//System Options
				case MENUNUM_SYSTEMOPTIONS:
//					printf("main loop: handling system options controller\n");			
					Handle_System_Interface(my_state);
					break;

				//Video Options
				case MENUNUM_VIDEOOPTIONS:
//					printf("main loop: handling video options controller\n");			
					Handle_Video_Interface(my_state);
					break;

				//Control Options
				case MENUNUM_CONTROLOPTIONS:
//					printf("main loop: handling control options controller\n");			
					Handle_Control_Interface(my_state);
					break;

				//Rom Selection Screen
				case MENUNUM_ROMSELECT:
//					printf("main loop: handling rom selection controller\n");			
					Handle_File_Browser_Interface(my_state);
					break;

				//About HugoDC Screen
				case MENUNUM_CREDITS:
					Handle_Credits_Interface(my_state);
					break;

				//Help Screen
				case MENUNUM_HELP:
					Handle_Help_Interface(my_state);
					break;
			}
		}

//		printf("main loop: drawing screen\n");
		draw_screen();

		if (AutoROM) {
			printf("main: loading rom [%s]\n", szRomPath);
			if (pNesX_Load(szRomPath, RomSize) == 0) {
				//Load Its SaveRAM
				if (SRAM_Enabled) {
					LoadSRAM();
				}

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
				if (SRAM_Enabled) {
					SaveSRAM();
				}
			} else {
				printf("main: error failed to start emulator!!!!\n");
			}
			AutoROM = false;
			menuscreen = MENUNUM_MAIN;		
		}		
	}

	printf("main loop: exiting\n");

	Free_Video_Options();
	Free_System_Options();
	Free_Control_Options();

	destroy_font(font);
	return 0;
}

int pNesX_ReadRom (const char *filepath, uint32 filesize) {
	unsigned char* ROM_Buffer;
	int i;
	int ROM_offset;
	int VROM_offset;

	ROM_Buffer = malloc(filesize);

	//MS - checksum calculation loads the rom currently, we should decouple that
	currentCRC32 = ReturnChecksum(filepath, filesize, ROM_Buffer);

	//Attempt to read an iNES header
	memcpy(&NesHeader, ROM_Buffer, sizeof(NesHeader));

	//Check if bytes 0-3 are NES header bytes
	int returnValue = -1;
	if ((NesHeader.byID[0] == 0x4E) && (NesHeader.byID[1] == 0x45) && (NesHeader.byID[2] == 0x53) && (NesHeader.byID[3] == 0x1A)) {
		if ((NesHeader.byInfo2 & 0x0C) == 0x08) {
			printf("ReadRom: NES 2.0 Header Detected\n");
		} else {
			printf("ReadRom: iNES Header Detected\n");			
		}

		MapperNo = ((NesHeader.byInfo1 & 0xF0) >> 4) | (NesHeader.byInfo2 & 0xF0);
		printf("ReadRom: Mapper Number [%i]\n", MapperNo);
		printf("ReadRom: PRG ROM [%i] * 16kB banks\n", NesHeader.byRomSize);
		if (NesHeader.byVRomSize == 0) {
			printf("ReadRom: CHR RAM\n");
		} else {
			printf("ReadRom: CHR ROM [%i] * 8kB banks\n", NesHeader.byVRomSize);
		}
		printf("ReadRom: Nametable Arrangement [%s]\n", (NesHeader.byInfo1 & 1) ? "horizontal mirrored" : "vertically mirrored");			
		printf("ReadRom: Battery Backed RAM [%s]\n", (NesHeader.byInfo1 & 2) ? "present" : "not present");
		printf("ReadRom: Trainer Present [%s]\n", (NesHeader.byInfo1 & 4) ? "true" : "false");
		printf("ReadRom: Alternative Nametable Layout [%s]\n", (NesHeader.byInfo1 & 8) ? "present" : "not present");
		if (NesHeader.byInfo2 & 0x01) {
			printf("ReadRom: VS Unisystem ROM detected");
		}
		if (NesHeader.byInfo2 & 0x02) {
			printf("ReadRom: PlayChoice-10 ROM detected");
		}
		if (NesHeader.byReserve[0] != 0) {
			printf("ReadRom: PRG RAM [%i] * 8kB banks\n", NesHeader.byReserve[0]);
		}

		if (NesHeader.byInfo1 & 2) {
			SRAM_Enabled = 1;
		} else {
			SRAM_Enabled = 0;
		}

		i = 16;
		//Read past the trainer
		if (NesHeader.byInfo1 & 4) {
			i += 512;
		}

		ROM_offset = i;
		ROM = malloc (NesHeader.byRomSize * 0x4000);
		for (; i < (ROM_offset + (NesHeader.byRomSize * 0x4000)); i++)
			ROM[i - ROM_offset] = ROM_Buffer[i];

		if (NesHeader.byVRomSize > 0) {
//			VRAM = NULL;
			VROM_offset = i;
			VROM = malloc (NesHeader.byVRomSize * 0x2000);
			for (; i < (VROM_offset + (NesHeader.byVRomSize * 0x2000)); i++)
				VROM[i - VROM_offset] = ROM_Buffer[i];
		} else {
//			VROM = NULL;
			if (MapperNo == 30) {
				printf("ReadRom: Mapper 30 Defaulting to 4 * 8kB CHR RAM\n");
				VROM = malloc(4 * 0x2000);
			} else {
				VROM = malloc(0x2000);
			}
		}

		//Load success
		returnValue = 0;
	} else {
		printf("ReadRom: NOT AN NES FILE - exiting");
	}

	free(ROM_Buffer);

	return returnValue;
}

void pNesX_LoadFrame() {
	pvr_wait_ready();
	pvr_scene_begin();

	pvr_list_begin(PVR_LIST_OP_POLY);
	pvr_poly_cxt_col(&my_cxt, PVR_LIST_OP_POLY);

	uint32 filter = PVR_FILTER_BILINEAR;
	if (!*opt_Filter) {
		filter = PVR_FILTER_NONE;
	}
	pvr_ptr_t texture = PVR_NESScreen2_Offset;
	if (WorkFrameIdx) {
		texture = PVR_NESScreen1_Offset;
	}

	//This creates an NES palette for the VQ textures, 4 pixels of each color in a row
	uint16* codebookEntry = (uint16*)codebook;
	uint32 codebookIdx;
	for (codebookIdx = 0; codebookIdx < 32; codebookIdx++) {
		uint16 codebookValue = PalTable[codebookIdx];
		*codebookEntry++ = codebookValue;
		*codebookEntry++ = codebookValue;
		*codebookEntry++ = codebookValue;
		*codebookEntry++ = codebookValue;
	}
	pvr_txr_load(codebook, texture, 2048);

	pvr_poly_cxt_txr(&my_cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED | PVR_TXRFMT_VQ_ENABLE, FRAMEBUFFER_WIDTH * 4, FRAMEBUFFER_HEIGHT, texture, filter);
	pvr_poly_compile(&my_pheader, &my_cxt);
	pvr_prim(&my_pheader, sizeof(my_pheader));

	float polygon_x1 = 0.0f;
	float polygon_y1 = 0.0f;
	float polygon_x2 = 640.0f;
	float polygon_y2 = 480.0f;
	float texture_u1 = ((float)opt_ClipVars[0] * 4) / 1024.0f;
	float texture_v1 = (float)opt_ClipVars[2] / 256.0f;
	float texture_u2 = (float)(1024 - (opt_ClipVars[1] * 4)) / 1024.0f;
	float texture_v2 = (float)(240 - (opt_ClipVars[3])) / 256.0f;

	if (!*opt_Stretch) {
		// Multiply clipped pixels by two because the texture is 256x256 and we are displaying as 512x512 (roughly)
		polygon_x1 = 64.0f + (float)(opt_ClipVars[0] * 2);
		polygon_x2 = 576.0f - (float)(opt_ClipVars[1] * 2);
		polygon_y1 = 0.0f + (float)(opt_ClipVars[2] * 2);
		polygon_y2 = 480.0f - (float)(opt_ClipVars[3] * 2);
	}

	my_vertex.flags = PVR_CMD_VERTEX;
	my_vertex.x = polygon_x1;
	my_vertex.y = polygon_y2;
	my_vertex.z = 30.0f;
	my_vertex.u = texture_u1;
	my_vertex.v = texture_v2;
	my_vertex.argb = 0xFFFFFFFF;
	my_vertex.oargb = 0;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.y = polygon_y1;
	my_vertex.v = texture_v1;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.x = polygon_x2;
	my_vertex.y = polygon_y2;
	my_vertex.u = texture_u2;
	my_vertex.v = texture_v2;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	my_vertex.flags = PVR_CMD_VERTEX_EOL;
	my_vertex.y = polygon_y1;
	my_vertex.v = texture_v1;
	pvr_prim(&my_vertex, sizeof(my_vertex));

	pvr_list_finish();	
	pvr_scene_finish();
}

void pNesX_PadState(uint32 *pdwPad1, uint32 *pdwPad2, uint32* ExitCount)
{
	maple_device_t* my_controller;
	cont_state_t* my_state = NULL;

	//Grab data from controller 0
	if (numControllers > 0)
	{
		my_controller = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
		if (my_controller != NULL) {
			my_state = (cont_state_t*)maple_dev_status(my_controller);		

#ifdef DEBUG
			// Toggle pNesX_DebugPrint messages while in operation
			if (((my_state -> buttons & CONT_Y) != 0) && !log_enabled_latch) {
				log_enabled = !log_enabled;
				log_enabled_latch = true;
			} else if ((my_state -> buttons & CONT_Y) == 0) {
				log_enabled_latch = false;
			}
#endif

			//Start first
			*pdwPad1 = (((my_state -> buttons & CONT_START) != 0 ) << 3);
			switch (*opt_P1AKey)
			{
				case 0:
					*pdwPad1 |= ( (my_state -> buttons & CONT_A) != 0);
					break;
				case 1:
					*pdwPad1 |= ( (my_state -> buttons & CONT_B) != 0);
					break;
				case 2:
					*pdwPad1 |= ( (my_state -> buttons & CONT_X) != 0);
					break;
				case 3:
					*pdwPad1 |= ( (my_state -> buttons & CONT_Y) != 0);
					break;
				case 4:
					*pdwPad1 |= (my_state -> ltrig != 0);
					break;
			}
			switch (*opt_P1BKey)
			{
				case 0:
					*pdwPad1 |= ( (my_state -> buttons & CONT_A) != 0) << 1;
					break;
				case 1:
					*pdwPad1 |= ( (my_state -> buttons & CONT_B) != 0) << 1;
					break;
				case 2:
					*pdwPad1 |= ( (my_state -> buttons & CONT_X) != 0) << 1;
					break;
				case 3:
					*pdwPad1 |= ( (my_state -> buttons & CONT_Y) != 0) << 1;
					break;
				case 4:
					*pdwPad1 |= (my_state -> ltrig != 0) << 1;
					break;
			}
			switch (*opt_P1SelectKey)
			{
				case 0:
					*pdwPad1 |= ( (my_state -> buttons & CONT_A) != 0) << 2;
					break;
				case 1:
					*pdwPad1 |= ( (my_state -> buttons & CONT_B) != 0) << 2;
					break;
				case 2:
					*pdwPad1 |= ( (my_state -> buttons & CONT_X) != 0) << 2;
					break;
				case 3:
					*pdwPad1 |= ( (my_state -> buttons & CONT_Y) != 0) << 2;
					break;
				case 4:
					*pdwPad1 |= (my_state -> ltrig != 0) << 2;
					break;
			}
			if (*opt_P1AnalogEnabled)
			{
				*pdwPad1 |= 
					( (my_state -> joyy < 114) << 4) | // Up
					( (my_state -> joyy > 140) << 5) | // Down
					( (my_state -> joyx < 114) << 6) | // Left
					( (my_state -> joyx > 140) << 7); // Right
			}
			else
				*pdwPad1 |= 
					( ( (my_state -> buttons & CONT_DPAD_UP    ) != 0 ) << 4) | // Up
					( ( (my_state -> buttons & CONT_DPAD_DOWN  ) != 0 ) << 5) | // Down
					( ( (my_state -> buttons & CONT_DPAD_LEFT  ) != 0 ) << 6) | // Left
					( ( (my_state -> buttons & CONT_DPAD_RIGHT ) != 0 ) << 7); // Right


			*pdwPad1 = *pdwPad1 | ( *pdwPad1 << 8);
		} else {
			*pdwPad1 = 0;
		}
	} else {
		*pdwPad1 = 0;
	}

	// Increment Exit Counter if Required
	if ((my_state != NULL) && (numControllers > 0)) {
		if ((my_state -> rtrig > 200) && (my_state -> ltrig != 0)) {
			(*ExitCount)++;
		} else {
			ExitCount = 0;
		}
	}
	
	//Grab data from controller 1
	if (numControllers > 1)
	{
		my_controller = maple_enum_type(1, MAPLE_FUNC_CONTROLLER);
		if (my_controller != NULL) {
			my_state = (cont_state_t*)maple_dev_status(my_controller);	

			//Start first
			*pdwPad2 = (( (my_state -> buttons & CONT_START ) != 0 ) << 3);
			switch (*opt_P2AKey)
			{
				case 0:
					*pdwPad2 |= ( (my_state -> buttons & CONT_A) != 0);
					break;
				case 1:
					*pdwPad2 |= ( (my_state -> buttons & CONT_B) != 0);
					break;
				case 2:
					*pdwPad2 |= ( (my_state -> buttons & CONT_X) != 0);
					break;
				case 3:
					*pdwPad2 |= ( (my_state -> buttons & CONT_Y) != 0);
					break;
				case 4:
					*pdwPad2 |= (my_state -> ltrig != 0);
			}
			switch (*opt_P2BKey)
			{
				case 0:
					*pdwPad2 |= ( (my_state -> buttons & CONT_A) != 0) << 1;
					break;
				case 1:
					*pdwPad2 |= ( (my_state -> buttons & CONT_B) != 0) << 1;
					break;
				case 2:
					*pdwPad2 |= ( (my_state -> buttons & CONT_X) != 0) << 1;
					break;
				case 3:
					*pdwPad2 |= ( (my_state -> buttons & CONT_Y) != 0) << 1;
					break;
				case 4:
					*pdwPad2 |= (my_state -> ltrig != 0) << 1;
					break;
			}
			switch (*opt_P2SelectKey)
			{
				case 0:
					*pdwPad2 |= ( (my_state -> buttons & CONT_A) != 0) << 2;
					break;
				case 1:
					*pdwPad2 |= ( (my_state -> buttons & CONT_B) != 0) << 2;
					break;
				case 2:
					*pdwPad2 |= ( (my_state -> buttons & CONT_X) != 0) << 2;
					break;
				case 3:
					*pdwPad2 |= ( (my_state -> buttons & CONT_Y) != 0) << 2;
					break;
				case 4:
					*pdwPad2 |= (my_state -> ltrig != 0) << 2;
					break;
			}
			if (*opt_P2AnalogEnabled) {
				*pdwPad2 |= 
					( (my_state -> joyy < 114) << 4) | // Up
					( (my_state -> joyy > 140) << 5) | // Down
					( (my_state -> joyx < 114) << 6) | // Left
					( (my_state -> joyx > 140) << 7); // Right
			} else {
				*pdwPad2 |= 
					( ( (my_state -> buttons & CONT_DPAD_UP    ) != 0 ) << 4) | // Up
					( ( (my_state -> buttons & CONT_DPAD_DOWN  ) != 0 ) << 5) | // Down
					( ( (my_state -> buttons & CONT_DPAD_LEFT  ) != 0 ) << 6) | // Left
					( ( (my_state -> buttons & CONT_DPAD_RIGHT ) != 0 ) << 7); // Right
			}

			*pdwPad2 = *pdwPad2 | ( *pdwPad2 << 8);
		} else {
			*pdwPad2 = 0;			
		}
	} else {
		*pdwPad2 = 0;
	}
}

uint32* pNesX_MemoryCopy_Offset( uint32* dest, uint32* src, int count, uint32 offset)
{
	//printf("memcpy_w_offset: [%u] [%u]\n", count, offset);
	if (offset == 0) {
		return memcpy(dest, src, count);		
	} else {
	// wrapping behaviour for sprite DMA operations
		unsigned char* u8dest = (unsigned char*)dest;
		unsigned char* u8src = (unsigned char*) src;		
		for (int i = 0; i < (count - offset); i++) {
			u8dest[i + offset] = u8src[i];
		}
		for (int i = 0; i < offset; i++) {
			u8dest[i] = u8src[i + (count - offset)];
		}
		return dest;
	}
}

// this routine came from the ghettoplay example that comes 
// with libdream
int draw_VMU_icon(maple_device_t* vmu, char* icon) {
	uint8 bitmap[48*32/8] = {0};
	int x, y, xi, xb;

	for (y=0; y<32; y++) {
		for (x=0; x<48; x++) {
			xi = x / 8;
			xb = 0x80 >> (x % 8);
			if (icon[(31-y)*48+(47-x)] == '+')
				bitmap[y*(48/8)+xi] |= xb;
		}
	}
	
	return vmu_draw_lcd(vmu, bitmap);
}

#ifdef DEBUG
void pNesX_DebugPrint( char *fmt, ... ) {
	if (log_enabled) {
		va_list args;
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
}
#endif