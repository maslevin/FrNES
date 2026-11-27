/*===================================================================*/
/*                                                                   */
/*  frontend.c : FrNES menus, options and ROM launching              */
/*               (for Dreamcast)                                     */
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
/*  2026/10/22  The 0.7.1 menus over the FrNES 1.0.0 core (frnes_run)  */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/
#include "macros.h"

#include "frontend.h"
#include "browser.h"
#include "dialog.h"
#include "cheats.h"
#include "viewer.h"
#include "image.h"
#include "markdown.h"
#include "palettes.h"
#include "input.h"
#include "menu.h"
#include "options.h"
#include "pages.h"
#include "movies.h"
#include "../emulator/sd_card.h"
#include "../emulator/vmu_icons.h"

#include "../emulator/frnes.h"
#include "../emulator/mapper.h"
#include "../emulator/movie.h"
#include "../emulator/state.h"
#include "../emulator/vmu_save.h"

KOS_INIT_FLAGS(INIT_DEFAULT | INIT_VMU);

// the monospace font (lists, values, help): make frontend MONO_FONT=/rd/other.fnt to try another
#ifndef FRNES_MONO_FONT
#define FRNES_MONO_FONT "/rd/jetbrainsmono.fnt"
#endif

/*-------------------------------------------------------------------*/
/*  The windows: the main one (menu pages, the ROM browser) and the  */
/*  help one at the left                                             */
/*-------------------------------------------------------------------*/
window_style_t mystyle;
window_data_t mydata;
window_style_t helpstyle;
window_data_t helpdata;

Font* font;
Font* font_mono;

// the ROM browser has the windows (otherwise a menu page)
static int browsing = 0;

// opaque and translucent polygons; the text-heavy pages (help, about) put many glyphs in a tile, so the bins are
// large and have overflow room (when a tile's bin filled, the PVR dropped glyphs)
static pvr_init_params_t menu_pvr_params = {
	.opb_sizes = { PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_0 },
	.vertex_buf_size = 1024 * 1024,
	.opb_overflow_count = 2
};

static void pvr_setup() {
	pvr_init(&menu_pvr_params);
	pvr_set_bg_color(0, 0, 0);
}

static void draw_screen() {
	pvr_poly_hdr_t my_pheader;
	pvr_poly_cxt_t my_cxt;
	pvr_vertex_t my_c_vertex;

	pvr_wait_ready();
	pvr_scene_begin();

	// the background, then the windows' frames
	pvr_list_begin(PVR_LIST_OP_POLY);
	pvr_poly_cxt_col(&my_cxt, PVR_LIST_OP_POLY);
	pvr_poly_compile(&my_pheader, &my_cxt);
	pvr_prim(&my_pheader, sizeof(my_pheader));

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
	menu_draw_page(PVR_LIST_OP_POLY);
	cheats_draw(PVR_LIST_OP_POLY);
	viewer_draw(PVR_LIST_OP_POLY);
	dialog_draw(PVR_LIST_OP_POLY);
	pvr_list_finish();

	// the text over them
	pvr_list_begin(PVR_LIST_TR_POLY);
	// the title on one line at the top, above the windows
	draw_string(font, PVR_LIST_TR_POLY, APP_STRING " " APP_VERSION, 32.0f, 10.0f, 30.0f, 576.0f, 40.0f, SINGLE, LEFT, 0xFF000000, 1.0f);
	win_draw_textwindow(&mydata, &mystyle, PVR_LIST_TR_POLY);
	win_draw_textwindow(&helpdata, &helpstyle, PVR_LIST_TR_POLY);
	menu_draw_page(PVR_LIST_TR_POLY);
	if (browsing) {
		browser_draw(PVR_LIST_TR_POLY);
	}
	cheats_draw(PVR_LIST_TR_POLY);
	viewer_draw(PVR_LIST_TR_POLY);
	dialog_draw(PVR_LIST_TR_POLY);
	pvr_list_finish();

	pvr_scene_finish();
}

void frontend_main_menu() {
	browsing = 0;
	// (the SD card's page when the adapter's there: tried each time, as the browser does)
	pages_sd_present(sd_card_mount());
	menu_open(&main_menu_page);
}

void frontend_browsing() {
	browsing = 1;
}

/*
 * Plays a ROM with the options' emulator settings. The emulator owns the PVR (and the store queues) while it plays,
 * so the menus' PVR and their font textures are let go first and set up again after; the exit buttons, still held
 * when it returns, are ignored until they're released.
 */
// (the emulator in this binary: its palette straight from the options)
const uint16_t* frnes_palette(void) {
	return options.palette;
}

// (the Game Genie codes switched on for the game being launched, as the separate binaries' handoff block has them)
static char launch_cheats[FRNES_CHEATS][9];

const char (*frnes_cheats(void))[9] {
	return (const char (*)[9])launch_cheats;
}

int launchEmulator(const char* path) {
	return launchEmulatorMovie(path, NULL, NULL);
}

int launchEmulatorMovie(const char* path, const char* movie, const char* record_directory) {
	printf("launchEmulator: loading rom [%s]%s%s\n", path, movie ? ", movie " : "", movie ? movie : "");
	memset(launch_cheats, 0, sizeof(launch_cheats));
	// (none under a movie: it plays the game as it was recorded)
	for (int i = 0; (i < FRNES_CHEATS) && (movie == NULL) && (strcmp(options.cheat_rom, path) == 0); i++) {
		if (options.cheat_on[i]) {
			memcpy(launch_cheats[i], options.cheat_codes[i], sizeof(launch_cheats[i]));
		}
	}
	options_sync_pads();
	frnes_settings = options.emulator;
	images_free();
	markdown_free();
	browser_release_art();
	destroy_font(font_mono);
	destroy_font(font);
	pvr_shutdown();

	// (the board's settings, kept by the core across games: the Zapper's speed, and NES port 2's device)
	zapper_set_sensitivity(frnes_settings.zapper_speed);
	options_input_to_core();
	// (where battery saves go, and the VMUs picked for them and for states)
	battery_set_place(options.battery_on_sd);
	vmu_save_choose(options.battery_vmu, options.state_vmu);
	state_set_flip_hotkey(options.flip_disk_hotkey);
	// (frnes_run is this with neither: one call, so the link has one copy of the frame loop)
	int result = frnes_run_movie(path, movie, record_directory);
	if (result != 0) {
		printf("launchEmulator: unable to start [%s]\n", path);
	}

	// (the video mode and filtering, changed in the game menu: kept, and saved with the options)
	if ((frnes_settings.picture != options.emulator.picture) || (frnes_settings.bilinear != options.emulator.bilinear)) {
		options.emulator.picture = frnes_settings.picture;
		options.emulator.bilinear = frnes_settings.bilinear;
		save_options_to_VMU();
	}
	// (the game's movie, save states)
	sd_card_sync();
	input_wait_release();
	pvr_setup();
	font = load_font("/rd/neuro.fnt");
	font_mono = load_font(FRNES_MONO_FONT);
	images_load();
	mydata.font = font_mono;
	mydata.header_font = font;
	helpdata.font = font_mono;
	helpdata.header_font = font;
	return result;
}

/*
 * AutoROM: a romdisk with autorom.txt naming a ROM plays it at start (a ROM and FrNES on one disc), then shows the
 * menus when it ends
 */
static int autorom_path(char* path, size_t size) {
	file_t handle = fs_open("/rd/autorom.txt", O_RDONLY);
	if (handle == -1) {
		return 0;
	}
	memset(path, 0, size);
	int got = fs_read(handle, path, size - 1);
	fs_close(handle);
	if (got <= 0) {
		return 0;
	}
	path[strcspn(path, "\r\n")] = '\0';
	printf("AutoROM: [%s]\n", path);
	return path[0] != '\0';
}

/*===================================================================*/
/*                                                                   */
/*                main() : Application main                          */
/*                                                                   */
/*===================================================================*/
int main() {
	printf("FrNES %s starting\n", APP_VERSION);
	// (the recording's saves on the core's save thread put the SD card's cache on the card after each one)
	autosave_file_sync = sd_card_sync;
	pvr_setup();
	font = load_font("/rd/neuro.fnt");
	font_mono = load_font(FRNES_MONO_FONT);
	images_load();

	for (int i = 0; i < 8; i++) {
		maple_device_t* vmu = maple_enum_type(i, MAPLE_FUNC_MEMCARD);
		if (vmu != NULL) {
			draw_VMU_icon(vmu, vmu_screen_normal);
		}
	}

	options_set_defaults();
	load_options_from_VMU();
	palettes_scan();

	char autorom[OPTIONS_PATH_LENGTH];
	if (autorom_path(autorom, sizeof(autorom))) {
		launchEmulator(autorom);
	}

	frontend_main_menu();
	for (;;) {
		uint32 input = input_poll();
		if (input & INPUT_EXIT) {
			break;
		}
		if (dialog_active()) {
			dialog_handle(input);
		} else if (viewer_active()) {
			viewer_input(input);
		} else if (cheats_active()) {
			cheats_input(input);
		} else if (browsing) {
			browser_handle(input);
		} else {
			menu_handle(input);
		}
		draw_screen();
	}

	printf("FrNES exiting\n");
	// (the SD card's FAT and directories written out, and the card left unmounted, before the BIOS or dcload has it)
	sd_card_unmount();
	images_free();
	markdown_free();
	browser_release_art();
	destroy_font(font_mono);
	destroy_font(font);
	pvr_shutdown();
	return 0;
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

