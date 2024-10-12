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

#include <bzlib/bzlib.h>

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/
#include "pNesX.h"
#include "macros.h"

#include "options.h"

#include "input_recorder.h"

#include "pNesX_System.h"
#include "pNesX_System_DC.h"
#include "profile.h"
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
Window_Style mystyle;
Window_Data mydata;
Window_Style helpstyle;
Window_Data helpdata;

//Help for the Options screen
char* Options_Keys[] = {
	"A - Toggle",
	"DPad L and R to Change"
};
const int Num_Options_Keys = 2;

const int Max_Frameskip = 5;

Font* font;

/*-------------------------------------------------------------------*/
/*  ROM image file information                                       */
/*-------------------------------------------------------------------*/
char szRomPath[ 256 ];
char szSaveName[ 256 ];
uint32 RomSize;
bool AutoROM;

/*-------------------------------------------------------------------*/
/*  Palette                                                          */
/*-------------------------------------------------------------------*/
uint16* NesPalette;

const uint16 DEFAULT_NES_PALETTE[] = {
	0x630C, 0x016F, 0x10B3, 0x3013, 0x500F, 0x6008, 0x6020, 0x50E0,
	0x3180, 0x1220, 0x0260, 0x0260, 0x0208, 0x0000, 0x0000, 0x0000,
	0xAD55, 0x1319, 0x3A1F, 0x695F, 0x98F9, 0xB0F0, 0xB165, 0x9A40,
	0x6B40, 0x3C00, 0x1460, 0x0465, 0x03F0, 0x0000, 0x0000, 0x0000,
	0xFFFF, 0x659F, 0x8C9F, 0xC3DF, 0xEB5F, 0xFB7A, 0xFBEF, 0xECC5,
	0xC5C0, 0x8EA0, 0x6705, 0x470F, 0x467A, 0x4A69, 0x0000, 0x0000,
	0xFFFF, 0xBF1F, 0xD69F, 0xE65F, 0xF61F, 0xFE3D, 0xFE59, 0xF6B5,
	0xE712, 0xD772, 0xBF95, 0xB799, 0xB75D, 0xBDD7, 0x0000, 0x0000
};

/*-------------------------------------------------------------------*/
/*  Controllers and VMUs                                             */
/*-------------------------------------------------------------------*/
//Controllers - will store controller addresses - NULL if not found
uint32 numControllers;
maple_device_t* Controllers[4];

//VMUs - will store VMU addresses -- NULL if not found
uint32 numVMUs;
maple_device_t* VMUs[8];

/*-------------------------------------------------------------------*/
/*  Recording mode                                                   */
/*-------------------------------------------------------------------*/
uint8 recordingMode = RECORDING_MODE_DISABLED;
bool inputActive[8];
InputFrame_t inputs[8];

#define MODE_BUTTONS 0
#define MODE_LTRIGGER 1
#define MODE_ANALOG_X_LEFT 2
#define MODE_ANALOG_X_RIGHT 3
#define MODE_ANALOG_Y_UP 4
#define MODE_ANALOG_Y_DOWN 5

/*-------------------------------------------------------------------*/
/*  Screen geometry                                                  */
/*-------------------------------------------------------------------*/
float polygon_x1;
float polygon_y1;
float polygon_x2;
float polygon_y2;
float texture_u1;
float texture_v1;
float texture_u2;
float texture_v2;

void calculateOutputScreenGeometry() {
	polygon_x1 = 0.0f + (((float)options.opt_ClipVars[0] * 640.0f) / 256.0f);
	polygon_y1 = 0.0f + (((float)options.opt_ClipVars[2] * 480.0f) / 240.0f);
	polygon_x2 = 640.0f - (((float)options.opt_ClipVars[1] * 640.0f) / 256.0f);
	polygon_y2 = 480.0f - (((float)options.opt_ClipVars[3] * 480.0f) / 240.0f);
	texture_u1 = ((float)options.opt_ClipVars[0] * 4) / 1024.0f;
	texture_v1 = (float)options.opt_ClipVars[2] / 256.0f;
	texture_u2 = (float)(1024 - (options.opt_ClipVars[1] * 4)) / 1024.0f;
	texture_v2 = (float)(240 - (options.opt_ClipVars[3])) / 256.0f;

	if (!options.opt_Stretch) {
		// Multiply clipped pixels by two because the texture is 256x256 and we are displaying as 512x512 (roughly)
		polygon_x1 = 64.0f + (float)(options.opt_ClipVars[0] * 2);
		polygon_x2 = 576.0f - (float)(options.opt_ClipVars[1] * 2);
		polygon_y1 = 0.0f + (float)(options.opt_ClipVars[2] * 2);
		polygon_y2 = 480.0f - (float)(options.opt_ClipVars[3] * 2);
	}	
}

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

void draw_screen() {
	//printf("draw_screen: begin\n");
	pvr_poly_hdr_t my_pheader;
	pvr_poly_cxt_t my_cxt;
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
		my_c_vertex.argb = options.GUI_BGColor;
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
		draw_string(font, PVR_LIST_TR_POLY, APP_STRING, 32.0f, 32.0f, 30.0f, 200.0f, 50.0f, SINGLE, LEFT, 0xFF000000, 1.0f);
		font -> scale = 0.50f;
		draw_string(font, PVR_LIST_TR_POLY, APP_VERSION, 32.0f, 54.0f, 30.0f, 200.0f, 50.0f, SINGLE, LEFT, 0xFF000000, 0.75f);
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
	if (options.opt_SRAM == 1) {
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
					printf("VMU: Found SRAM Save File on VMU [%i]\n", i);

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

int SaveSRAM() {
	unsigned int compressedLength = 0x2200 + 0x1000;
	char* compressedBuffer = malloc(compressedLength);	

	int saveSRAM_success = -1;
	if (options.opt_SRAM == 1) {
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
			int result = BZ2_bzBuffToBuffCompress(compressedBuffer, &compressedLength, (char*)SRAM, 0x2000, 5, 4, 30);

			if (result != BZ_OK) {
				printf("VMU: bz2 Compression Failed With Error Code [%i]\n", result);
				draw_VMU_icon(vmu, vmu_screen_error);
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
			package.data = (uint8*)compressedBuffer;	

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

	free(compressedBuffer);		
	return saveSRAM_success;
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

void loadPalette(char* path) {
	uint32 my_fd = fs_open(path, O_RDONLY);
	bool errored_out = false;
	if (my_fd == -1) {
		printf("ERROR: Unable to load palette from file, using default\n");
		errored_out = true;
	} else {
		uint8 palFileData[192];
		if (fs_read(my_fd, palFileData, 192) != 192) {
			errored_out = true;
		} else {
			printf("LoadPalette: Loaded Palette - Converting to RGB565\n");
			NesPalette = malloc(64 * 2);
			for (int i = 0; i < 64; i++) {
				NesPalette[i] = 
				  (((palFileData[(i * 3)] >> 3) & 0x1F) << 11) |
				  (((palFileData[(i * 3) + 1] >> 2) & 0x3F) << 5) |
				  (((palFileData[(i * 3) + 2] >> 3) & 0x1F));
			}
		}

/*
		// Keep this commented out - this is just for creating another default palette though if you want to in the future
		printf("const uint16 NesPalette[] = {\n");
		for (uint i = 0; i < 64; i++) {
			printf("0x%04X,", NesPalette[i]);
			if ((i != 0) && ((i + 1) % 8 == 0)) {
				printf("\n");
			} else {
				printf(" ");
			}
		}
		printf("}\n\n");
*/

		fs_close(my_fd);
	}

	if (errored_out) {
		NesPalette = (uint16*)DEFAULT_NES_PALETTE;
	}
}

void initVQTextures() {
	// This creates an NES palette for each VQ texture, 4 pixels of each Global NES palette color in a row
	uint16* codebookEntry = (uint16*)codebook;
	uint32 codebookIdx;
	// Actually we put 4 copies of the Global NES palette into the codebook - this is useful so we can use the upper 
	// bits of these pixels as mask bits during rendering, but have them make not perceptual difference during display 
	for (codebookIdx = 0; codebookIdx < 256; codebookIdx++) {
		uint16 codebookValue = NesPalette[codebookIdx % 64];
		*codebookEntry++ = codebookValue;
		*codebookEntry++ = codebookValue;
		*codebookEntry++ = codebookValue;
		*codebookEntry++ = codebookValue;
	}

	for (uint32 i = 0; i < NUM_PVR_FRAMES; i++) {
		WorkFrames[i] = (VQ_Texture*)pvr_mem_malloc(sizeof(VQ_Texture));
		printf("Allocated frame [%lu] at address [0x%8lX]\n", i, (uint32)WorkFrames[i]);
		pvr_txr_load(codebook, WorkFrames[i] -> codebook, 2048);
		printf("Updated texture with NES Palette\n");
	}
}

void initializeUserOptions() {
	// If we weren't able to load any FrNES Options from any of the connected VMUs set the application defaults
	if (!load_options_from_VMU()) {
		options.GUI_BGColor = 0xFF0080FF;
		options.GUI_TextColor = 0xFF000000;
		options.GUI_SelectedTextColor = 0xFF00FF00;
		options.GUI_InsideWindowColor = 0xFF00803f;
		options.GUI_OutsideWindowColor = 0xFFFFFFFF;

		for (uint8 i = 0; i < 4; i++) {
			options.controllerSettings[i].analogEnabled = DEFAULT_ANALOG;
			options.controllerSettings[i].aKey = DEFAULT_AKEY;
			options.controllerSettings[i].bKey = DEFAULT_BKEY;
			options.controllerSettings[i].selectKey = DEFAULT_SELECT;
		}

		options.opt_Stretch = DEFAULT_STRETCH;
		options.opt_Filter = DEFAULT_FILTER;
		options.opt_ClipVars[0] = DEFAULT_CLIP_LEFT;
		options.opt_ClipVars[1] = DEFAULT_CLIP_RIGHT;
		options.opt_ClipVars[2] = DEFAULT_CLIP_TOP;
		options.opt_ClipVars[3] = DEFAULT_CLIP_BOTTOM;
		
		options.opt_SoundEnabled = DEFAULT_SOUND;
		options.opt_FrameSkip = DEFAULT_FRAMESKIP;
		options.opt_AutoFrameSkip = DEFAULT_AUTOFRAMESKIP;
		options.opt_ShowFrameRate = DEFAULT_SHOWFRAMERATE;
		options.opt_SRAM = DEFAULT_SRAM;
	}
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

void launchEmulator() {
	printf("launchEmulator: loading rom [%s]\n", szRomPath);
	numEmulationFrames = 0;

	if (pNesX_Load(szRomPath, RomSize) == 0) {
		//Load Its SaveRAM
		if (SRAM_Enabled) {
			LoadSRAM();
		}

		memset(inputActive, 0, sizeof(bool) * 8);
		memset(inputs, 0, sizeof(InputFrame_t) * 8);
		switch(recordingMode) {
			case RECORDING_MODE_DISABLED:
				printf("launchEmulator: Input Recording Disabled\n");
				break;
			case RECORDING_MODE_ENABLED:
				printf("launchEmulator: Input Recording Enabled\n");
				break;
			case RECORDING_MODE_PLAYBACK:
				printf("launchEmulator: Input Playback Mode Enabled\n");
				loadRecording();
				break;
		}

		//Stay in Emulator During Operation
		pNesX_Main();

		//Save Its SaveRAM
		if (SRAM_Enabled) {
			SaveSRAM();
		}

		if (recordingMode == RECORDING_MODE_ENABLED) {
			printf("Uploading Recording\n");
			uploadRecording();
		}
	} else {
		printf("main: error failed to start emulator!!!!\n");
	}	
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

	// Load palette
	printf("Initializing NES Palette\n");
	loadPalette("/rd/nes.pal");

	printf("Initializing VQ Textures\n");
	initVQTextures();

	printf("Initializing Controllers and VMUs\n");	
	initialize_controllers();

	printf("Initializing VMUs\n");
	for (uint8 i = 0; i < numVMUs; i++) {
		draw_VMU_icon(VMUs[i], vmu_screen_normal);
	}

	printf("Initializing User Options\n");
	initializeUserOptions();

	romselstatus = 0;
	invalida = 0;
	xkeyhit = 0;
	disable_trigs = 0;
	disable_rom_interface = 0;

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
		my_controller = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
		if (my_controller != NULL) {
			my_state = (cont_state_t*)maple_dev_status(my_controller);

			if ((my_state == NULL) || (my_state -> buttons & CONT_START)) {
				exitLoop = 1;
			}

			if (invalida && ((my_state -> buttons & CONT_A) == 0)) {
				invalida = 0;
			}

			if (keyhit && ((my_state -> buttons & (CONT_DPAD_UP | CONT_DPAD_DOWN)) == 0)) {
				keyhit = 0;
			}

			if (xkeyhit && ((my_state -> buttons & (CONT_X | CONT_DPAD_LEFT | CONT_DPAD_RIGHT)) == 0)) {
				xkeyhit = 0;
			}

			if (disable_trigs && ((my_state -> rtrig == 0) && (my_state -> ltrig == 0))) {
				disable_trigs = 0;
			}

			//Event handling and processing section
			switch (menuscreen) {
				//Main Menu Screen
				case MENUNUM_MAIN:
					Handle_Main_Menu_Interface(my_state);
					break;

				//GUI Options
				case MENUNUM_GUIOPTIONS:
					Handle_GUI_Interface(my_state);
					break;

				//System Options
				case MENUNUM_SYSTEMOPTIONS:
					Handle_System_Interface(my_state);
					break;

				//Video Options
				case MENUNUM_VIDEOOPTIONS:
					Handle_Video_Interface(my_state);
					break;

				//Control Options
				case MENUNUM_CONTROLOPTIONS:
					Handle_Control_Interface(my_state);
					break;

				//Rom Selection Screen
				case MENUNUM_ROMSELECT:
					Handle_File_Browser_Interface(my_state);
					break;

				//Credits Screen
				case MENUNUM_CREDITS:
					Handle_Credits_Interface(my_state);
					break;

				//Help Screen
				case MENUNUM_HELP:
					Handle_Help_Interface(my_state);
					break;
			}
		}

		draw_screen();

		if (AutoROM) {
			launchEmulator();
			AutoROM = false;
			menuscreen = MENUNUM_MAIN;		
		}		
	}

	printf("main loop: exiting\n");

	if (NesPalette != DEFAULT_NES_PALETTE) {
		free(NesPalette);
	}

	destroy_font(font);
	return 0;
}

__attribute__ ((hot)) void pNesX_LoadFrame() {
	startProfiling(3);

	pvr_poly_hdr_t my_pheader;
	pvr_poly_cxt_t my_cxt;
	pvr_vertex_t my_vertex;

	pvr_wait_ready();
	pvr_scene_begin();

	pvr_list_begin(PVR_LIST_OP_POLY);
	pvr_poly_cxt_col(&my_cxt, PVR_LIST_OP_POLY);

	uint32 filter = PVR_FILTER_BILINEAR;
	if (!options.opt_Filter) {
		filter = PVR_FILTER_NONE;
	}

	pvr_poly_cxt_txr(&my_cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED | PVR_TXRFMT_VQ_ENABLE, FRAMEBUFFER_WIDTH * 4, FRAMEBUFFER_HEIGHT, WorkFrame, filter);
	pvr_poly_compile(&my_pheader, &my_cxt);
	pvr_prim(&my_pheader, sizeof(my_pheader));

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

	if (options.opt_ShowFrameRate) {
		pvr_list_begin(PVR_LIST_TR_POLY);

		char fps[10];
		if (frames_per_second > 60.0f) {
			frames_per_second = 60.0f;
		}
		snprintf(fps, 10, "%.0f", frames_per_second);

		draw_string(font,
			PVR_LIST_TR_POLY,
			fps,
			640.0 - 60.0f,
			10.0f,
			35.0f,
			45.0f,
			35.0f,
			SINGLE,
			RIGHT,
			0xFFFFFFFF,
			0.80f
		);

		pvr_list_finish();
	}		

	pvr_scene_finish();
	endProfiling(3);
}

__attribute__ ((hot)) void handleButton(uint32* controllerBitflags, 
	uint8 controllerBitflagIndex, 
	uint8 controller, 
	cont_state_t* controller_state,
	uint8 mode,
	uint16 state_flag
) {
	bool buttonOn = false;
	switch (mode) {
		case MODE_BUTTONS:
			buttonOn = (controller_state -> buttons & state_flag);
			break;
		case MODE_LTRIGGER:
			buttonOn = (controller_state -> ltrig > 0);
			break;
		case MODE_ANALOG_X_LEFT:
			buttonOn = (controller_state -> joyx < -114);
			break;
		case MODE_ANALOG_X_RIGHT:
			buttonOn = (controller_state -> joyx > 114);
			break;
		case MODE_ANALOG_Y_UP:
			buttonOn = (controller_state -> joyy < -114);
			break;
		case MODE_ANALOG_Y_DOWN:
			buttonOn = (controller_state -> joyy > 114);
			break;	
	}

	if (buttonOn) {
		*controllerBitflags |= (0x1 << controllerBitflagIndex);
		if ((recordingMode == RECORDING_MODE_ENABLED) && (!inputActive[controllerBitflagIndex])) {
			inputActive[controllerBitflagIndex] = true;
			inputs[controllerBitflagIndex].frameStart = numEmulationFrames;
		}
	} else if (inputActive[controllerBitflagIndex]) {
		if (recordingMode == RECORDING_MODE_ENABLED) {
			inputs[controllerBitflagIndex].frameDuration = numEmulationFrames - inputs[controllerBitflagIndex].frameStart;
			inputs[controllerBitflagIndex].controller = controller;
			inputs[controllerBitflagIndex].button = controllerBitflagIndex;
			recordInput(&inputs[controllerBitflagIndex]);
			inputActive[controllerBitflagIndex] = false;
		}
	}
}

bool playbackRecording(uint32* controllerBitflags) {
	*controllerBitflags = 0;

	// if there are any inputs scheduled to happen on this frame, set them into activity
	while (frameInputs[currentSample].frameStart == numEmulationFrames) {
		InputFrame_t* sample = &frameInputs[currentSample];
		inputActive[sample -> button] = true;
		memcpy(&inputs[sample -> button], sample, sizeof(InputFrame_t));
		currentSample++;
	}

	// process all inputs, and if they are active, continue to or them into the controller bitflags
	bool activityDone = true;
	for (uint8 inputIndex = 0; inputIndex < 8; inputIndex++) {
		if (inputActive[inputIndex]) {		
			activityDone = false;
			// if they reach their duration, turn the input off			
			if (numEmulationFrames - inputs[inputIndex].frameStart == (inputs[inputIndex].frameDuration)) {
				inputActive[inputIndex] = false;
			} else {
				*controllerBitflags |= (0x1 << inputIndex);	
			}
		}
	}

	return !((currentSample == numSamples) && (activityDone));
}

__attribute__ ((hot)) void handleController(cont_state_t* state, uint32* bitflags, uint8 controllerIndex) {
	//Start first
	handleButton(bitflags, CONTROLLER_BUTTON_START, controllerIndex, state, MODE_BUTTONS, CONT_START);
	switch (options.controllerSettings[controllerIndex].aKey) {
		case 0:
			handleButton(bitflags, CONTROLLER_BUTTON_A, controllerIndex, state, MODE_BUTTONS, CONT_A);
			break;
		case 1:
			handleButton(bitflags, CONTROLLER_BUTTON_A, controllerIndex, state, MODE_BUTTONS, CONT_B);
			break;
		case 2:
			handleButton(bitflags, CONTROLLER_BUTTON_A, controllerIndex, state, MODE_BUTTONS, CONT_X);
			break;
		case 3:
			handleButton(bitflags, CONTROLLER_BUTTON_A, controllerIndex, state, MODE_BUTTONS, CONT_Y);
			break;
		case 4:
			handleButton(bitflags, CONTROLLER_BUTTON_A, controllerIndex, state, MODE_LTRIGGER, 0);
			break;
	}
	switch (options.controllerSettings[controllerIndex].bKey) {
		case 0:
			handleButton(bitflags, CONTROLLER_BUTTON_B, controllerIndex, state, MODE_BUTTONS, CONT_A);
			break;
		case 1:
			handleButton(bitflags, CONTROLLER_BUTTON_B, controllerIndex, state, MODE_BUTTONS, CONT_B);
			break;
		case 2:
			handleButton(bitflags, CONTROLLER_BUTTON_B, controllerIndex, state, MODE_BUTTONS, CONT_X);
			break;
		case 3:
			handleButton(bitflags, CONTROLLER_BUTTON_B, controllerIndex, state, MODE_BUTTONS, CONT_Y);
			break;
		case 4:
			handleButton(bitflags, CONTROLLER_BUTTON_B, controllerIndex, state, MODE_LTRIGGER, 0);
			break;
	}
	switch (options.controllerSettings[controllerIndex].selectKey) {
		case 0:
			handleButton(bitflags, CONTROLLER_BUTTON_SELECT, controllerIndex, state, MODE_BUTTONS, CONT_A);
			break;
		case 1:
			handleButton(bitflags, CONTROLLER_BUTTON_SELECT, controllerIndex, state, MODE_BUTTONS, CONT_B);
			break;
		case 2:
			handleButton(bitflags, CONTROLLER_BUTTON_SELECT, controllerIndex, state, MODE_BUTTONS, CONT_X);
			break;
		case 3:
			handleButton(bitflags, CONTROLLER_BUTTON_SELECT, controllerIndex, state, MODE_BUTTONS, CONT_Y);
			break;
		case 4:
			handleButton(bitflags, CONTROLLER_BUTTON_SELECT, controllerIndex, state, MODE_LTRIGGER, 0);
			break;
	}
	if (options.controllerSettings[controllerIndex].analogEnabled) {
		handleButton(bitflags, CONTROLLER_BUTTON_UP, controllerIndex, state, MODE_ANALOG_Y_UP, 0);
		handleButton(bitflags, CONTROLLER_BUTTON_DOWN, controllerIndex, state, MODE_ANALOG_Y_DOWN, 0);
		handleButton(bitflags, CONTROLLER_BUTTON_LEFT, controllerIndex, state, MODE_ANALOG_X_LEFT, 0);
		handleButton(bitflags, CONTROLLER_BUTTON_RIGHT, controllerIndex, state, MODE_ANALOG_X_RIGHT, 0);
	} else {
		handleButton(bitflags, CONTROLLER_BUTTON_UP, controllerIndex, state, MODE_BUTTONS, CONT_DPAD_UP);
		handleButton(bitflags, CONTROLLER_BUTTON_DOWN, controllerIndex, state, MODE_BUTTONS, CONT_DPAD_DOWN);
		handleButton(bitflags, CONTROLLER_BUTTON_LEFT, controllerIndex, state, MODE_BUTTONS, CONT_DPAD_LEFT);
		handleButton(bitflags, CONTROLLER_BUTTON_RIGHT, controllerIndex, state, MODE_BUTTONS, CONT_DPAD_RIGHT);
	}
}

__attribute__ ((hot)) void pNesX_PadState(uint32 *pdwPad1, uint32 *pdwPad2, uint32* ExitCount) {	
	maple_device_t* my_controller;
	cont_state_t* my_state = NULL;

	if (recordingMode == RECORDING_MODE_PLAYBACK) {
		if (!playbackRecording(pdwPad1)) {
			(*ExitCount)++;
		}

		//Grab data from controller 0
		my_controller = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
		if (my_controller != NULL) {
			my_state = (cont_state_t*)maple_dev_status(my_controller);
			if (my_state -> buttons & CONT_START) {
				*ExitCount = MAX_EXIT_COUNT + 1;
			}
		}
	} else {
		//Grab data from controller 0
		*pdwPad1 = 0;		
		if (numControllers > 0) {
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
				handleController(my_state, pdwPad1, 0);
			}

			// Increment Exit Counter if Required
			if ((my_state != NULL) && (numControllers > 0)) {
				if ((my_state -> rtrig > 200) && (my_state -> ltrig != 0)) {
					(*ExitCount)++;
				} else {
					*ExitCount = 0;
				}
			}				
		}

		if (numControllers > 1) {
			*pdwPad2 = 0;
			my_controller = maple_enum_type(1, MAPLE_FUNC_CONTROLLER);
			if (my_controller != NULL) {
				my_state = (cont_state_t*)maple_dev_status(my_controller);
				handleController(my_state, pdwPad2, 1);
			}
		}

		if (numControllers > 2) {
			my_controller = maple_enum_type(2, MAPLE_FUNC_CONTROLLER);
			if (my_controller != NULL) {
				uint32 player_3_bitflags;
				my_state = (cont_state_t*)maple_dev_status(my_controller);
				handleController(my_state, &player_3_bitflags, 2);
				*pdwPad1 |= (player_3_bitflags << 8);
			}
		}

		if (numControllers > 3) {
			my_controller = maple_enum_type(3, MAPLE_FUNC_CONTROLLER);
			if (my_controller != NULL) {
				uint32 player_4_bitflags;
				my_state = (cont_state_t*)maple_dev_status(my_controller);
				handleController(my_state, &player_4_bitflags, 3);
				*pdwPad2 |= (player_4_bitflags << 8);
			}
		}
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