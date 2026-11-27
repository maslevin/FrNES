/*===================================================================*/
/*                                                                   */
/*  pages.c : The menu pages                                         */
/*                                                                   */
/*  Each page is a table of rows (menu.h): what they show, the value */
/*  they change, and what happens when they're chosen.               */
/*                                                                   */
/*===================================================================*/

#include "pages.h"
#include "browser.h"
#include "frontend.h"
#include "options.h"
#include "palettes.h"
#include "image.h"
#include "input.h"
#include "color_picker.h"
#include "markdown.h"
#include "gun_calibrate.h"
#include "dialog.h"
#include <dc/vmufs.h>

static const window_key_t options_keys[] = {
	{ KEY_DPAD, "Move / Change" },
	{ KEY_A, "Choose" },
	{ KEY_B, "Back" }
};
#define NUM_OPTIONS_KEYS 3

static const window_key_t main_keys[] = {
	{ KEY_DPAD, "Move" },
	{ KEY_A, "Choose" }
};


// a line under an action's rows saying how it went (saving options, ...)
static char status_line[40] = "";

static void open_page(const Menu_Item* item);

/*-------------------------------------------------------------------*/
/*  Video                                                            */
/*-------------------------------------------------------------------*/
static const char* const picture_modes[FRNES_PICTURE_MODES] = { "Full screen", "Exact 2x", "8:7 aspect" };

static void reset_crop(const Menu_Item* item) {
	(void)item;
	options.emulator.crop_left = options.emulator.crop_right = 0;
	options.emulator.crop_top = options.emulator.crop_bottom = 8;
}

static int palette_get(const Menu_Item* item) {
	(void)item;
	return palettes_current();
}

static void palette_set(const Menu_Item* item, int value) {
	(void)item;
	palettes_choose(value);
}

// the chosen palette's 64 colors under the rows, in its 4 rows of 16 (the NES's own order)
#define SWATCH_LEFT 80.0f
#define SWATCH_TOP 292.0f
#define SWATCH_CELL_WIDTH 30.0f
#define SWATCH_CELL_HEIGHT 10.0f

static void draw_palette_swatch(uint32 list) {
	if (list != PVR_LIST_OP_POLY) {
		return;
	}
	pvr_poly_hdr_t hdr;
	pvr_poly_cxt_t cxt;
	pvr_poly_cxt_col(&cxt, list);
	pvr_poly_compile(&hdr, &cxt);
	pvr_prim(&hdr, sizeof(hdr));
	const uint32* colors = palettes_colors();
	pvr_vertex_t v;
	v.z = 33.0f;
	v.oargb = 0;
	v.u = v.v = 0.0f;
	for (int i = 0; i < 64; i++) {
		float x1 = SWATCH_LEFT + (i & 15) * SWATCH_CELL_WIDTH, y1 = SWATCH_TOP + (i >> 4) * SWATCH_CELL_HEIGHT;
		float x2 = x1 + SWATCH_CELL_WIDTH, y2 = y1 + SWATCH_CELL_HEIGHT;
		v.argb = colors[i];
		v.flags = PVR_CMD_VERTEX;
		v.x = x1; v.y = y2; pvr_prim(&v, sizeof(v));
		v.x = x1; v.y = y1; pvr_prim(&v, sizeof(v));
		v.x = x2; v.y = y2; pvr_prim(&v, sizeof(v));
		v.flags = PVR_CMD_VERTEX_EOL;
		v.x = x2; v.y = y1; pvr_prim(&v, sizeof(v));
	}
}

static const char* const sound_modes[] = { "On", "Off" };
static const char* const sound_filters[] = { "Off", "NES", "Famicom" };
static const char* const sound_qualities[] = { "Standard", "High" };

// (not const: the palette row's choices are the .pal files found at start, palettes_scan)
static Menu_Item video_items[] = {
	{ .kind = MENU_CHOICE, .label = "Palette", .get = palette_get, .set = palette_set, .count = 1, .help = "The NES's colors: each .pal file is one maker's idea of how an NES looked on a TV. The colors are shown below." },
	{ .kind = MENU_CHOICE, .label = "Picture", .value = &options.emulator.picture, .choices = picture_modes, .count = FRNES_PICTURE_MODES, .help = "How the NES picture fills the screen. Full screen stretches it to the whole display. Exact 2x keeps square pixels. 8:7 shows the NES's own pixel shape." },
	{ .kind = MENU_TOGGLE, .label = "Smooth (bilinear filter)", .value = &options.emulator.bilinear, .help = "Smooths the picture with the PVR's bilinear filter. Off keeps sharp pixel edges." },
	{ .kind = MENU_TEXT, .label = "" },
	{ .kind = MENU_TEXT, .label = "Hide pixels at the edges" },
	{ .kind = MENU_NUMBER, .label = "  Left", .value = &options.emulator.crop_left, .min = 0, .max = FRNES_CROP_MAX_X, .format = "%d px", .help = "Pixels hidden at the left edge, shown as black. The picture keeps its size and place." },
	{ .kind = MENU_NUMBER, .label = "  Right", .value = &options.emulator.crop_right, .min = 0, .max = FRNES_CROP_MAX_X, .format = "%d px", .help = "Pixels hidden at the right edge, shown as black. The picture keeps its size and place." },
	{ .kind = MENU_NUMBER, .label = "  Top", .value = &options.emulator.crop_top, .min = 0, .max = FRNES_CROP_MAX_Y, .format = "%d px", .help = "Lines hidden at the top. TVs hid about 8; games often leave junk in the top lines." },
	{ .kind = MENU_NUMBER, .label = "  Bottom", .value = &options.emulator.crop_bottom, .min = 0, .max = FRNES_CROP_MAX_Y, .format = "%d px", .help = "Lines hidden at the bottom. TVs hid about 8." },
	{ .kind = MENU_ACTION, .label = "  Reset (top and bottom 8)", .action = reset_crop, .help = "Hides 8 lines at the top and bottom and none at the sides, as a TV of the day showed it." }
};

static const Menu_Page video_page = {
	"Video Options", video_items, sizeof(video_items) / sizeof(video_items[0]), options_keys, NUM_OPTIONS_KEYS, &main_menu_page, 0.0f,
	draw_palette_swatch
};

/*-------------------------------------------------------------------*/
/*  Audio Options                                                    */
/*-------------------------------------------------------------------*/
static const Menu_Item audio_items[] = {
	{ .kind = MENU_CHOICE, .label = "Sound", .value = &options.sound_off, .choices = sound_modes, .count = 2, .help = "Off doesn't emulate the sound at all, which leaves more time for the rest of the emulation. The game runs at its speed by the timer instead of the sound." },
	{ .kind = MENU_CHOICE, .label = "Sound filter", .value = &options.sound_filter, .choices = sound_filters, .count = 3, .help = "Shapes the sound as a console's own output did. NES: the front-loader's 90 Hz and 440 Hz high-pass and 14 kHz low-pass. Famicom: its 37 Hz high-pass. Off leaves the sound as the chips make it." },
	{ .kind = MENU_CHOICE, .label = "Sound quality", .value = &options.sound_quality, .choices = sound_qualities, .count = 2, .help = "High has less aliasing on high notes. It costs about 0.5 ms a frame, about 2 ms in games with VRC6 or Sunsoft 5B sound." },
};

static const Menu_Page audio_page = {
	"Audio Options", audio_items, sizeof(audio_items) / sizeof(audio_items[0]), options_keys, NUM_OPTIONS_KEYS, &main_menu_page, 0.0f
};

/*-------------------------------------------------------------------*/
/*  Controls: a tab for each port (A-D), and one for the hotkeys     */
/*-------------------------------------------------------------------*/
static const window_key_t controls_keys[] = {
	{ KEY_LT, NULL },
	{ KEY_RT, "Tab" },
	{ KEY_DPAD, "Move / Change" },
	{ KEY_B, "Back" }
};
#define NUM_CONTROLS_KEYS 4

// the tab showing: ports A-D, then the hotkeys
#define TAB_HOTKEYS OPTIONS_PORTS
static int controls_tab = 0;
static const char* const tab_names[OPTIONS_PORTS + 1] = { "A", "B", "C", "D", "Hotkeys" };

// the Dreamcast buttons an NES button can be on (0: none)
static const uint32 button_masks[] = { 0, CONT_A, CONT_B, CONT_X, CONT_Y, CONT_C, CONT_Z, CONT_START, CONT_X | CONT_B, CONT_A | CONT_C };
static const char* const button_names[] = { "None", "A", "B", "X", "Y", "C", "Z", "Start", "X or B", "A or C" };
#define NUM_BUTTON_MASKS (sizeof(button_masks) / sizeof(button_masks[0]))

// NES Select's choices: each button, then each with the left trigger as well (port_options' select_on_left_trigger:
// "Left trigger" alone for None)
static const char* const select_names[] = { "None", "A", "B", "X", "Y", "C", "Z", "Start", "X or B", "A or C",
	"Left trigger", "A or LT", "B or LT", "X or LT", "Y or LT", "C or LT", "Z or LT", "Start or LT", "X, B or LT", "A, C or LT" };

static const char* const turbo_rates[] = { "30 a second", "20 a second", "15 a second", "10 a second" };

// tag: which of the port's buttons
#define PORT_A 0
#define PORT_B 1
#define PORT_SELECT 2
#define PORT_START 3
#define PORT_TURBO_A 4
#define PORT_TURBO_B 5

static uint32* port_mask(const Menu_Item* item) {
	Port_Options* port = &options.ports[controls_tab];
	switch (item -> tag) {
		case PORT_A: return &port -> a;
		case PORT_B: return &port -> b;
		case PORT_SELECT: return &port -> select;
		case PORT_START: return &port -> start;
		case PORT_TURBO_A: return &port -> turbo_a;
		default: return &port -> turbo_b;
	}
}

static int mapping_get(const Menu_Item* item) {
	uint32 mask = *port_mask(item);
	for (uint32 i = 0; i < NUM_BUTTON_MASKS; i++) {
		if (button_masks[i] == mask) {
			return i;
		}
	}
	return 0;
}

static void mapping_set(const Menu_Item* item, int value) {
	*port_mask(item) = button_masks[value];
	options_sync_pads();
}

// NES Select: the button and whether the left trigger presses it too, one choice
static int select_get(const Menu_Item* item) {
	return mapping_get(item) + (options.ports[controls_tab].select_on_left_trigger ? NUM_BUTTON_MASKS : 0);
}

static void select_set(const Menu_Item* item, int value) {
	options.ports[controls_tab].select_on_left_trigger = (value >= (int)NUM_BUTTON_MASKS);
	mapping_set(item, value % NUM_BUTTON_MASKS);
}

// tag: 0 left trigger as Select, 1 analog stick, 2 turbo rate
static int port_flag_get(const Menu_Item* item) {
	Port_Options* port = &options.ports[controls_tab];
	return (item -> tag == 0) ? port -> select_on_left_trigger : ((item -> tag == 1) ? port -> analog_dpad : port -> turbo_rate);
}

static void port_flag_set(const Menu_Item* item, int value) {
	Port_Options* port = &options.ports[controls_tab];
	if (item -> tag == 0) {
		port -> select_on_left_trigger = value;
	} else if (item -> tag == 1) {
		port -> analog_dpad = value;
	} else {
		port -> turbo_rate = value;
	}
	options_sync_pads();
}

// where this port plugs in and what it is (options_port_nes / options_port_device)
static const char* const nes_ports[] = { "NES port 1", "NES port 2", "NES port 3", "NES port 4", "Nothing" };
static const char* const port_devices[] = { "Auto", "Controller", "Zapper", "Paddle" };

static int plug_get(const Menu_Item* item) {
	return item -> tag ? options_port_device(controls_tab) : options_port_nes(controls_tab);
}

static void plug_set(const Menu_Item* item, int value) {
	if (item -> tag) {
		options_set_port_device(controls_tab, value);
	} else {
		options_set_port_nes(controls_tab, value);
	}
}

static void reset_port(const Menu_Item* item) {
	(void)item;
	options_default_port(controls_tab);
}

#define MAPPING_ROW(name, which, text) \
	{ .kind = MENU_CHOICE, .label = name, .get = mapping_get, .set = mapping_set, .choices = button_names, .count = NUM_BUTTON_MASKS, .tag = which, .help = text }

static const Menu_Item port_items[] = {
	{ .kind = MENU_CHOICE, .label = "Plugs into", .get = plug_get, .set = plug_set, .choices = nes_ports, .count = 5, .tag = 0,
		.help = "The NES port this controller plays on: by default A is player 1, B player 2, C player 3 and D player 4. Picking one another controller has swaps them. A port with nothing plugged in is ignored." },
	{ .kind = MENU_CHOICE, .label = "Is a", .get = plug_get, .set = plug_set, .choices = port_devices, .count = 4, .tag = 1,
		.help = "Paddle: the Arkanoid paddle on NES port 2 (the stick or D-pad turns it, A fires). Auto: a pad, and in light gun games player 1's controller aims the Zapper. Games with both, like Bayou Billy, start it off: switch ZAPPER 2 in the game menu (LT + RT). Zapper: this controller is one in any game. The stick or D-pad aims, A fires, B fires off screen." },
	MAPPING_ROW("NES A", PORT_A, "The Dreamcast button (or buttons) that press NES A."),
	MAPPING_ROW("NES B", PORT_B, "The Dreamcast button (or buttons) that press NES B."),
	{ .kind = MENU_CHOICE, .label = "NES Select", .get = select_get, .set = select_set, .choices = select_names, .count = 2 * NUM_BUTTON_MASKS,
		.tag = PORT_SELECT, .help = "The Dreamcast button (or buttons) that press NES Select, and whether the left trigger does too (pulled past half way, not while the right trigger is held)." },
	MAPPING_ROW("NES Start", PORT_START, "The Dreamcast button that presses NES Start. Vs. System arcade games: LT+Start inserts a coin, Start is 1P start and Select 2P start; in Zapper games pull the trigger to start."),
	// (turbo: one button each, None to Z)
	{ .kind = MENU_CHOICE, .label = "Turbo A", .get = mapping_get, .set = mapping_set, .choices = button_names, .count = 7, .tag = PORT_TURBO_A, .help = "A button that presses NES A over and over while it's held." },
	// (turbo: one button each, None to Z)
	{ .kind = MENU_CHOICE, .label = "Turbo B", .get = mapping_get, .set = mapping_set, .choices = button_names, .count = 7, .tag = PORT_TURBO_B, .help = "A button that presses NES B over and over while it's held." },
	{ .kind = MENU_CHOICE, .label = "Turbo speed", .get = port_flag_get, .set = port_flag_set, .choices = turbo_rates, .count = 4, .tag = 2, .help = "How often turbo presses the button." },
	{ .kind = MENU_TOGGLE, .label = "Analog stick moves", .get = port_flag_get, .set = port_flag_set, .tag = 1, .help = "The analog stick works as the D-pad as well, when pushed most of the way." },
	{ .kind = MENU_ACTION, .label = "Reset this port", .action = reset_port, .help = "Puts this port's buttons back as they were at first." }
};

// the hotkeys' button combinations (0: none)
static const char* const hotkey_names[] = { "Off", "Right trigger held", "RT + A", "RT + B", "RT + X", "RT + Y", "LT + RT",
	"LT + A", "LT + B", "LT + X", "LT + Y", "C + Z" };
#define NUM_HOTKEY_NAMES (sizeof(hotkey_names) / sizeof(hotkey_names[0]))

static const char* const four_scores[] = { "Auto", "Off", "On" };

// FDS games' flip disk hotkey: any combination but Off (0 in the options, from before it, is the right trigger held)
static int flip_disk_get(const Menu_Item* item) {
	(void)item;
	return (options.flip_disk_hotkey ? options.flip_disk_hotkey : FRNES_HOTKEY_RT) - 1;
}

static void flip_disk_set(const Menu_Item* item, int value) {
	(void)item;
	options.flip_disk_hotkey = value + 1;
}

// the Zapper's reticle speed (0, from options saved before it: the default)
static int zapper_speed_get(const Menu_Item* item) {
	(void)item;
	return options.emulator.zapper_speed ? options.emulator.zapper_speed : FRNES_ZAPPER_SPEED_DEFAULT;
}

static void zapper_speed_set(const Menu_Item* item, int value) {
	(void)item;
	options.emulator.zapper_speed = value;
}

static const Menu_Item hotkey_items[] = {
	{ .kind = MENU_CHOICE, .label = "Save state", .value = &options.hotkeys[HOTKEY_SAVE_STATE], .choices = hotkey_names, .count = NUM_HOTKEY_NAMES,
		.help = "Saves the game as it is right now, in slot 0: on the VMU or the SD card, as the SD Card page says." },
	{ .kind = MENU_CHOICE, .label = "Load state", .value = &options.hotkeys[HOTKEY_LOAD_STATE], .choices = hotkey_names, .count = NUM_HOTKEY_NAMES,
		.help = "Goes back to the saved state." },
	{ .kind = MENU_CHOICE, .label = "Frame display", .value = &options.hotkeys[HOTKEY_FRAME_DISPLAY], .choices = hotkey_names, .count = NUM_HOTKEY_NAMES,
		.help = "Turns the timing display on and off in a game. (The emulator uses the right trigger held for now.)" },
	{ .kind = MENU_CHOICE, .label = "Flip disk", .get = flip_disk_get, .set = flip_disk_set, .choices = hotkey_names + 1,
		.count = NUM_HOTKEY_NAMES - 1, .help = "Puts the next disk side in, in Famicom Disk System games. The right trigger held means held for a second; the game says which at its start." },
	{ .kind = MENU_CHOICE, .label = "Game menu", .value = &options.hotkeys[HOTKEY_GAME_MENU], .choices = hotkey_names, .count = NUM_HOTKEY_NAMES,
		.help = "Opens a menu over the paused game: save and load states and their slot, Game Genie codes and Zappers on and off, the video mode, filtering and frame display, reset, the disk side, back to the browser." },
	{ .kind = MENU_CHOICE, .label = "Four players", .value = &options.four_score, .choices = four_scores, .count = 3,
		.help = "The Four Score (or the Famicom's adapter) for NES ports 3 and 4: Auto for the games that take it." },
	{ .kind = MENU_NUMBER, .label = "Zapper speed", .get = zapper_speed_get, .set = zapper_speed_set, .min = FRNES_ZAPPER_SPEED_MIN,
		.max = FRNES_ZAPPER_SPEED_MAX, .help = "How fast the D-pad or the analog stick moves the Zapper's sight in light gun games (A fires, B fires off the screen)." },
	{ .kind = MENU_TEXT, .label = "" },
	{ .kind = MENU_TEXT, .label = "A+B+X+Y+Start always ends" },
	{ .kind = MENU_TEXT, .label = "the game." }
};

// the picture's place at the left of the rows, and the tabs' at the right of the title
#define CONTROLS_INDENT 225.0f
#define PICTURE_SIZE 200.0f
#define TAB_TOP 55.0f
#define TAB_HEIGHT 22.0f
#define TAB_RIGHT 596.0f
#define TAB_TEXT_SCALE 0.5f
#define TAB_RADIUS 9.0f

static float tab_width(int tab) {
	return (tab == TAB_HOTKEYS) ? 76.0f : 30.0f;
}

// the tabs at the right of the title: each a framed box (frame, fill, text each at its own depth, in front of the
// window's layers), the chosen one in the highlight colour and a little higher
static void draw_tabs(uint32 list) {
	float x = TAB_RIGHT;
	for (int tab = TAB_HOTKEYS; tab >= 0; tab--) {
		x -= tab_width(tab) + 4.0f;
		int chosen = (tab == controls_tab);
		float top = TAB_TOP + (chosen ? 0.0f : 1.0f), height = TAB_HEIGHT - (chosen ? 0.0f : 1.0f);
		if (list == PVR_LIST_OP_POLY) {
			uint32 fill = chosen ? options.GUI_SelectedTextColor : options.GUI_InsideWindowColor;
			// (the fill's corners follow the frame's: its radius less the frame's width)
			draw_cornered_layer(list, x, top, 32.5f, tab_width(tab), height, TAB_RADIUS, options.GUI_OutsideWindowColor);
			draw_cornered_layer(list, x + 2.0f, top + 2.0f, 33.5f, tab_width(tab) - 4.0f, height - 4.0f, TAB_RADIUS - 2.0f, fill);
		} else {
			float width;
			measureLine(font_mono, (char*)tab_names[tab], TAB_TEXT_SCALE, &width);
			float text_height = font_mono -> fontHeight * TAB_TEXT_SCALE;
			uint32 color = chosen ? options.GUI_InsideWindowColor : options.GUI_TextColor;
			draw_string(font_mono, list, (char*)tab_names[tab], x + (tab_width(tab) - width) / 2, top + (height - text_height) / 2, 36.0f,
				width + 2.0f, text_height, SINGLE, LEFT, color, TAB_TEXT_SCALE);
		}
	}
}

// what's in a port: a controller, an arcade stick, a light gun (and its picture), or nothing
static const char* port_device(int port, Image_Id* image) {
	maple_device_t* device = maple_enum_dev(port, 0);
	*image = IMAGE_COUNT;
	// (what KOS reports, on the console when it changes: for telling devices apart)
	static uint32 reported[OPTIONS_PORTS] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
	uint32 functions = ((device != NULL) && device -> valid) ? device -> info.functions : 0;
	if (reported[port] != functions) {
		reported[port] = functions;
		printf("controls: port %c: %.30s (functions %08lx)\n", 'A' + port, functions ? device -> info.product_name : "nothing", (unsigned long)functions);
	}
	// (a light gun that can't sync to the picture, as on VGA, answers KOS's first probe and then drops out: it's shown
	// until something else is plugged into its port)
	static uint8 gun_seen[OPTIONS_PORTS];
	if ((device == NULL) || !device -> valid) {
		if (gun_seen[port]) {
			*image = IMAGE_LIGHT_GUN;
			return "Light gun (not answering)";
		}
		return "Nothing in this port";
	}
	gun_seen[port] = (device -> info.functions & MAPLE_FUNC_LIGHTGUN) != 0;
	if (device -> info.functions & MAPLE_FUNC_LIGHTGUN) {
		*image = IMAGE_LIGHT_GUN;
		return "Light gun";
	}
	if (device -> info.functions & MAPLE_FUNC_MOUSE) {
		*image = IMAGE_MOUSE;
		return "Mouse";
	}
	if (device -> info.functions & MAPLE_FUNC_CONTROLLER) {
		if (cont_is_type(device, CONT_TYPE_ARCADE_STICK)) {
			*image = IMAGE_ARCADE_STICK;
			return "Arcade stick";
		}
		*image = IMAGE_CONTROLLER;
		return "Controller";
	}
	// (the product name is 30 characters, padded with spaces and not ended)
	static char name[31];
	snprintf(name, sizeof(name), "%.30s", device -> info.product_name);
	for (int i = (int)strlen(name) - 1; (i >= 0) && (name[i] == ' '); i--) {
		name[i] = '\0';
	}
	return name;
}

// the buttons' names over the pictures (tools/draw_art.py's places, in its 256 x 256 drawing)
typedef struct {
	Image_Id image;
	float x, y;
	const char* text;
	uint32 color;
} Picture_Label;

static const Picture_Label picture_labels[] = {
	{ IMAGE_CONTROLLER, 200, 82, "Y", 0xFFFFFFFF },
	{ IMAGE_CONTROLLER, 178, 104, "X", 0xFF303030 },
	{ IMAGE_CONTROLLER, 222, 104, "B", 0xFFFFFFFF },
	{ IMAGE_CONTROLLER, 200, 126, "A", 0xFFFFFFFF },
	{ IMAGE_CONTROLLER, 128, 172, "Start", 0xFF404448 },
	{ IMAGE_CONTROLLER, 56, 40, "Stick", 0xFF404448 },
	{ IMAGE_CONTROLLER, 67, 164, "D-pad", 0xFF404448 },
	{ IMAGE_ARCADE_STICK, 146, 124, "X", 0xFF184818 },
	{ IMAGE_ARCADE_STICK, 172, 108, "Y", 0xFF184818 },
	{ IMAGE_ARCADE_STICK, 202, 108, "Z", 0xFF184818 },
	{ IMAGE_ARCADE_STICK, 146, 160, "A", 0xFF184818 },
	{ IMAGE_ARCADE_STICK, 172, 142, "B", 0xFF184818 },
	{ IMAGE_ARCADE_STICK, 202, 142, "C", 0xFF184818 },
	{ IMAGE_ARCADE_STICK, 148, 66, "Start", 0xFF605a50 },
	{ IMAGE_ARCADE_STICK, 52, 178, "Stick", 0xFF202020 },
	{ IMAGE_LIGHT_GUN, 140, 162, "Trigger", 0xFF404040 },
	{ IMAGE_LIGHT_GUN, 80, 132, "Start", 0xFF404040 },
	{ IMAGE_LIGHT_GUN, 15, 54, "D-pad", 0xFF404040 },
	{ IMAGE_LIGHT_GUN, 15, 116, "B", 0xFF404040 },
	{ IMAGE_MOUSE, 94, 80, "Left", 0xFF404040 },
	{ IMAGE_MOUSE, 162, 80, "Right", 0xFF404040 }
};
#define LABEL_SCALE 0.45f

static void draw_picture_labels(Image_Id image, float x, float y) {
	float scale = PICTURE_SIZE / 256.0f;
	for (uint32 i = 0; i < sizeof(picture_labels) / sizeof(picture_labels[0]); i++) {
		const Picture_Label* label = &picture_labels[i];
		if (label -> image != image) {
			continue;
		}
		float width, height = font_mono -> fontHeight * LABEL_SCALE;
		measureLine(font_mono, (char*)label -> text, LABEL_SCALE, &width);
		draw_string(font_mono, PVR_LIST_TR_POLY, (char*)label -> text, x + label -> x * scale - width / 2, y + label -> y * scale - height / 2,
			34.5f, width + 2.0f, height, SINGLE, LEFT, label -> color, LABEL_SCALE);
	}
}

static void draw_controls(uint32 list) {
	draw_tabs(list);
	if ((list != PVR_LIST_TR_POLY) || (controls_tab == TAB_HOTKEYS)) {
		return;
	}
	Image_Id image;
	// (every port looked at each frame: a gun that attaches only for a moment is caught on any tab)
	for (int port = 0; port < OPTIONS_PORTS; port++) {
		port_device(port, &image);
	}
	const char* device = port_device(controls_tab, &image);
	float x = mydata.x + 15.0f, y = mydata.y + 52.0f;
	if (image != IMAGE_COUNT) {
		image_draw(image, x, y, PICTURE_SIZE, PICTURE_SIZE, 0.0f, 0.0f, 1.0f, 1.0f, 34.0f, 0xFFFFFFFF);
		draw_picture_labels(image, x, y);
	}
	char caption[64];
	int nes = options_port_nes(controls_tab);
	if (nes == OPTIONS_NES_NONE) {
		snprintf(caption, sizeof(caption), "Port %c: not plugged in", 'A' + controls_tab);
	} else {
		snprintf(caption, sizeof(caption), "Port %c: NES port %d%s", 'A' + controls_tab, nes + 1,
			(options_port_device(controls_tab) == OPTIONS_DEVICE_ZAPPER) ? ", Zapper" :
			((options_port_device(controls_tab) == OPTIONS_DEVICE_PADDLE) ? ", Paddle" : ""));
	}
	draw_string(font_mono, list, caption, x, y + PICTURE_SIZE + 2.0f, 34.0f, PICTURE_SIZE + 20.0f, 20.0f, SINGLE, LEFT, options.GUI_TextColor, 0.5f);
	draw_string(font_mono, list, (char*)device, x, y + PICTURE_SIZE + 20.0f, 34.0f, PICTURE_SIZE + 20.0f, 20.0f, SINGLE, LEFT, options.GUI_TextColor, 0.5f);
}

static const Menu_Page port_page;
static const Menu_Page hotkey_page;
static const Menu_Page gun_page;
static const Menu_Page mouse_page;

// a tab's page: the hotkeys, a light gun's (its calibration in place of the buttons), or a port's buttons
static const Menu_Page* controls_page(void) {
	if (controls_tab == TAB_HOTKEYS) {
		return &hotkey_page;
	}
	maple_device_t* device = maple_enum_dev(controls_tab, 0);
	if (device && device -> valid && (device -> info.functions & MAPLE_FUNC_MOUSE)) {
		return &mouse_page;
	}
	return (device && device -> valid && (device -> info.functions & MAPLE_FUNC_LIGHTGUN)) ? &gun_page : &port_page;
}

// LT / RT: the next tab
static int controls_input(uint32 input) {
	int step = (input & INPUT_PAGE_DOWN) ? 1 : ((input & INPUT_PAGE_UP) ? -1 : 0);
	if (step == 0) {
		return 0;
	}
	const Menu_Page* was = controls_page();
	controls_tab = (controls_tab + step + TAB_HOTKEYS + 1) % (TAB_HOTKEYS + 1);
	int highlighted = mydata.highlighted_index;
	if (was != controls_page()) {
		menu_open(controls_page());
	} else {
		// (the same row on the next port)
		mydata.highlighted_index = highlighted;
	}
	return 1;
}

static const Menu_Page port_page = {
	"Controls", port_items, sizeof(port_items) / sizeof(port_items[0]), controls_keys, NUM_CONTROLS_KEYS, &main_menu_page, 0.5f,
	draw_controls, controls_input, CONTROLS_INDENT
};

/*
 * A light gun's tab: where it plugs in and what it is, as a port's, then its calibration (one for every gun: it's the
 * TV's as much as the gun's) in place of the buttons, which a gun can't change: the trigger is A, and B fires off the
 * screen
 */
static int gun_get(const Menu_Item* item) {
	int16 values[4];
	options_gun_calibration(&values[0], &values[1], &values[2], &values[3]);
	// (the scales shown in tenths of a percent as whole percents)
	return ((item -> tag & 1) == 0) ? (values[item -> tag] + 5) / 10 : values[item -> tag];
}

static void gun_set(const Menu_Item* item, int value) {
	int16 values[4];
	options_gun_calibration(&values[0], &values[1], &values[2], &values[3]);
	values[item -> tag] = ((item -> tag & 1) == 0) ? (int16)(value * 10) : (int16)value;
	options_set_gun_calibration(values[0], values[1], values[2], values[3]);
}

static void gun_calibrate_action(const Menu_Item* item) {
	(void)item;
	gun_calibrate(controls_tab, status_line, sizeof(status_line));
	input_wait_release();
	// (what happened, over the page until A closes it)
	const char* lines[1] = { status_line };
	dialog_show("Light gun", lines, 1);
}

static void gun_reset(const Menu_Item* item) {
	(void)item;
	options_set_gun_calibration(0, 0, 0, 0);
	snprintf(status_line, sizeof(status_line), "Light gun calibration reset");
}

static const char* const reticle_modes[] = { "Shown", "Hidden" };

static const Menu_Item gun_items[] = {
	{ .kind = MENU_CHOICE, .label = "Plugs into", .get = plug_get, .set = plug_set, .choices = nes_ports, .count = 5, .tag = 0,
		.help = "The NES port this light gun plugs into. With a light gun plugged in, Zapper games take it as their Zapper." },
	{ .kind = MENU_CHOICE, .label = "Is a", .get = plug_get, .set = plug_set, .choices = port_devices, .count = 3, .tag = 1,
		.help = "Auto: the Zapper in Zapper games. Zapper: the Zapper in any game. The trigger fires, B fires off the screen. Light guns work on CRT TVs only." },
	{ .kind = MENU_ACTION, .label = "Calibrate (shoot 4 targets)", .action = gun_calibrate_action,
		.help = "Shows a target in each corner of a white screen in turn: aim carefully and pull the trigger on each. B cancels." },
	{ .kind = MENU_NUMBER, .label = "Across: scale", .get = gun_get, .set = gun_set, .min = 50, .max = 200, .format = "%d%%", .tag = 0,
		.help = "How far the aim moves across the screen for the gun's movement. Calibrating sets it." },
	{ .kind = MENU_NUMBER, .label = "Across: offset", .get = gun_get, .set = gun_set, .min = -400, .max = 400, .format = "%d px", .tag = 1,
		.help = "Moves the aim left (less) or right (more). Calibrating sets it." },
	{ .kind = MENU_NUMBER, .label = "Down: scale", .get = gun_get, .set = gun_set, .min = 50, .max = 200, .format = "%d%%", .tag = 2,
		.help = "How far the aim moves down the screen for the gun's movement. Calibrating sets it." },
	{ .kind = MENU_NUMBER, .label = "Down: offset", .get = gun_get, .set = gun_set, .min = -200, .max = 200, .format = "%d px", .tag = 3,
		.help = "Moves the aim up (less) or down (more). Calibrating sets it." },
	{ .kind = MENU_CHOICE, .label = "Reticle", .value = &options.gun_hide_reticle, .choices = reticle_modes, .count = 2,
		.help = "The crosshair over the picture where the light gun points. Hidden, you aim with the gun alone." },
	{ .kind = MENU_ACTION, .label = "Reset calibration", .action = gun_reset, .help = "Puts the light gun's calibration back to FrNES's own." },
	{ .kind = MENU_TEXT, .label = status_line }
};

static const Menu_Page gun_page = {
	"Controls", gun_items, sizeof(gun_items) / sizeof(gun_items[0]), controls_keys, NUM_CONTROLS_KEYS, &main_menu_page, 0.5f,
	draw_controls, controls_input, CONTROLS_INDENT
};

// a mouse's tab: where it plugs in and what it is, a Zapper or the paddle, never a controller (Matt; it has no buttons
// to map: the left button fires)
static const char* const mouse_devices[] = { "Auto", "Zapper", "Paddle" };

static int mouse_device_get(const Menu_Item* item) {
	(void)item;
	int device = options_port_device(controls_tab);
	return (device == OPTIONS_DEVICE_ZAPPER) ? 1 : ((device == OPTIONS_DEVICE_PADDLE) ? 2 : 0);
}

static void mouse_device_set(const Menu_Item* item, int value) {
	(void)item;
	options_set_port_device(controls_tab, (value == 1) ? OPTIONS_DEVICE_ZAPPER : ((value == 2) ? OPTIONS_DEVICE_PADDLE : OPTIONS_DEVICE_AUTO));
}

// (no Plugs into: a mouse is never a pad, and taking a pad's NES port moved that controller to the mouse's, Matt's
// controller onto NES port 3; its Zapper and paddle are NES port 2's)
static const Menu_Item mouse_items[] = {
	{ .kind = MENU_CHOICE, .label = "Is a", .get = mouse_device_get, .set = mouse_device_set, .choices = mouse_devices, .count = 3,
		.help = "Auto: the Zapper in light gun games (no light gun plugged in) and the paddle in Arkanoid games. Zapper: moving it aims, any button fires. Paddle: left and right turn the knob, any button fires." },
	{ .kind = MENU_TEXT, .label = "" },
	{ .kind = MENU_TEXT, .label = "A mouse is never a pad." }
};

static const Menu_Page mouse_page = {
	"Controls", mouse_items, sizeof(mouse_items) / sizeof(mouse_items[0]), controls_keys, NUM_CONTROLS_KEYS, &main_menu_page, 0.5f,
	draw_controls, controls_input, CONTROLS_INDENT
};

static const Menu_Page hotkey_page = {
	"Controls", hotkey_items, sizeof(hotkey_items) / sizeof(hotkey_items[0]), controls_keys, NUM_CONTROLS_KEYS, &main_menu_page, 0.5f,
	draw_controls, controls_input, 0.0f
};

/*-------------------------------------------------------------------*/
/*  VMU: which VMU takes battery saves and save states               */
/*-------------------------------------------------------------------*/
// the choices: the first with room, then each VMU there (or chosen before and gone), by its options code
#define MAX_VMU_CHOICES 10
static uint8 vmu_codes[MAX_VMU_CHOICES];
static char vmu_choice_text[MAX_VMU_CHOICES][32];
static const char* vmu_choices[MAX_VMU_CHOICES];
static int vmu_choice_count = 0;
// the VMUs' list under the rows
#define MAX_VMU_LINES 8
static char vmu_lines[MAX_VMU_LINES][40];
static int vmu_line_count = 0;

static void vmu_choice_add(uint8 code, const char* text) {
	if (vmu_choice_count >= MAX_VMU_CHOICES) {
		return;
	}
	for (int i = 0; i < vmu_choice_count; i++) {
		if (vmu_codes[i] == code) {
			return;
		}
	}
	vmu_codes[vmu_choice_count] = code;
	snprintf(vmu_choice_text[vmu_choice_count], sizeof(vmu_choice_text[0]), "%s", text);
	vmu_choices[vmu_choice_count] = vmu_choice_text[vmu_choice_count];
	vmu_choice_count++;
}

// the VMUs plugged in now, with their free blocks
static void vmu_scan() {
	vmu_choice_count = 0;
	vmu_choice_add(0, "First with room");
	for (int i = 0; i < MAX_VMU_LINES; i++) {
		vmu_lines[i][0] = '\0';
	}
	int lines = 0;
	for (int i = 0; i < 8; i++) {
		maple_device_t* vmu = maple_enum_type(i, MAPLE_FUNC_MEMCARD);
		if (vmu == NULL) {
			break;
		}
		char name[8];
		snprintf(name, sizeof(name), "%c%d", 'A' + vmu -> port, vmu -> unit);
		vmu_choice_add(1 + vmu -> port * 2 + (vmu -> unit - 1), name);
		if (lines < MAX_VMU_LINES) {
			snprintf(vmu_lines[lines++], sizeof(vmu_lines[0]), "  %s: %d blocks free", name, vmufs_free_blocks(vmu));
		}
	}
	if (lines == 0) {
		snprintf(vmu_lines[lines++], sizeof(vmu_lines[0]), "  No VMUs plugged in");
	}
	vmu_line_count = lines;
	// (one chosen before and not plugged in now stays a choice)
	for (int which = 0; which < 2; which++) {
		uint8 code = which ? options.state_vmu : options.battery_vmu;
		if (code != 0) {
			char name[24];
			snprintf(name, sizeof(name), "%c%d (not plugged in)", 'A' + (code - 1) / 2, (code - 1) % 2 + 1);
			vmu_choice_add(code, name);
		}
	}
}

// tag: 0 battery saves, 1 save states
static int vmu_choice_get(const Menu_Item* item) {
	uint8 code = item -> tag ? options.state_vmu : options.battery_vmu;
	for (int i = 0; i < vmu_choice_count; i++) {
		if (vmu_codes[i] == code) {
			return i;
		}
	}
	return 0;
}

static void vmu_choice_set(const Menu_Item* item, int value) {
	uint8 code = vmu_codes[(value >= 0) && (value < vmu_choice_count) ? value : 0];
	if (item -> tag) {
		options.state_vmu = code;
	} else {
		options.battery_vmu = code;
	}
}

/*-------------------------------------------------------------------*/
/*  Saves: where battery saves and save states go (the part for both  */
/*  first), then the VMUs and the SD card                             */
/*-------------------------------------------------------------------*/
static const char* const save_places[] = { "VMU", "SD card", "Off" };
// the SD card's line: whether the menus found one (pages_sd_present)
static char sd_line[48] = "SD card: not found";

// (not const: the VMU rows' choices are the VMUs found as the page opens, vmu_scan)
static Menu_Item saves_items[] = {
	{ .kind = MENU_CHOICE, .label = "Battery saves on", .value = &options.battery_on_sd, .choices = save_places, .count = 3,
		.help = "Where games' battery saves go: a VMU, or the SD card's FrNES/saves folder, which has room for every game. FrNES writes them on its own a few seconds after the game does. Off: none are loaded or written, every game starts fresh." },
	{ .kind = MENU_CHOICE, .label = "Save states on", .value = &options.states_on_sd, .choices = save_places, .count = 3,
		.help = "Where save states go: a VMU (compressed, a few blocks each) or the SD card's FrNES folder. Off: the save and load state hotkeys and the game menu's do nothing." },
	{ .kind = MENU_TEXT, .label = "" },
	{ .kind = MENU_TEXT, .label = "VMU" },
	{ .kind = MENU_CHOICE, .label = "  Battery saves VMU", .get = vmu_choice_get, .set = vmu_choice_set, .tag = 0,
		.help = "The VMU that keeps games' battery saves when they go on a VMU: tried first, then the others if it's missing or full." },
	{ .kind = MENU_CHOICE, .label = "  Save states VMU", .get = vmu_choice_get, .set = vmu_choice_set, .tag = 1,
		.help = "The VMU that keeps save states when they go on a VMU: tried first, then the others if it's missing or full." },
	// (a line for each VMU plugged in, then the SD card's: vmu_page_prepare fills these rows, and the page ends after
	// the last; Matt: 8 lines, most of them empty, pushed the SD card off the page)
	{ .kind = MENU_TEXT, .label = "" }, { .kind = MENU_TEXT, .label = "" }, { .kind = MENU_TEXT, .label = "" },
	{ .kind = MENU_TEXT, .label = "" }, { .kind = MENU_TEXT, .label = "" }, { .kind = MENU_TEXT, .label = "" },
	{ .kind = MENU_TEXT, .label = "" }, { .kind = MENU_TEXT, .label = "" }, { .kind = MENU_TEXT, .label = "" },
	{ .kind = MENU_TEXT, .label = "" }, { .kind = MENU_TEXT, .label = "" }
};
// (the VMU choice rows' place in saves_items, and the VMU lines' after them)
#define SAVES_VMU_BATTERY 4
#define SAVES_VMU_STATES 5
#define SAVES_LINES 6

static Menu_Page saves_page = {
	"Saves", saves_items, sizeof(saves_items) / sizeof(saves_items[0]), options_keys, NUM_OPTIONS_KEYS, &main_menu_page, 0.0f
};

static void vmu_page_prepare() {
	vmu_scan();
	saves_items[SAVES_VMU_BATTERY].choices = saves_items[SAVES_VMU_STATES].choices = vmu_choices;
	saves_items[SAVES_VMU_BATTERY].count = saves_items[SAVES_VMU_STATES].count = vmu_choice_count;
	int row = SAVES_LINES;
	for (int i = 0; i < vmu_line_count; i++) {
		saves_items[row++].label = vmu_lines[i];
	}
	saves_items[row++].label = "";
	saves_items[row++].label = sd_line;
	saves_items[row++].label = "  Files go in /sd/FrNES.";
	saves_page.count = row;
}

/*-------------------------------------------------------------------*/
/*  System                                                           */
/*-------------------------------------------------------------------*/
static const char* const hud_modes[] = { "Off", "On" };
static const char* const pacing_modes[FRNES_PACING_MODES] = { "Sound", "Vsync" };

static void save_options(const Menu_Item* item) {
	(void)item;
	snprintf(status_line, sizeof(status_line), "%s", save_options_to_VMU() ? "Options saved to the VMU" : "No VMU took the options");
}

static void load_options(const Menu_Item* item) {
	(void)item;
	snprintf(status_line, sizeof(status_line), "%s", load_options_from_VMU() ? "Options loaded from the VMU" : "No FrNES options on a VMU");
	palettes_scan();
	menu_style_windows("System Options", options_keys, NUM_OPTIONS_KEYS, 0.0f);
}

static void delete_options(const Menu_Item* item) {
	(void)item;
	snprintf(status_line, sizeof(status_line), "%s", delete_options_from_VMU() ? "Options deleted from the VMU" : "No FrNES options on a VMU");
}

static void default_options(const Menu_Item* item) {
	(void)item;
	char directory[OPTIONS_PATH_LENGTH];
	int32 index = options.browser_index;
	memcpy(directory, options.browser_directory, sizeof(directory));
	options_set_defaults();
	memcpy(options.browser_directory, directory, sizeof(directory));
	options.browser_index = index;
	palettes_scan();
	snprintf(status_line, sizeof(status_line), "Defaults restored (not saved)");
	menu_style_windows("System Options", options_keys, NUM_OPTIONS_KEYS, 0.0f);
}

static const Menu_Item system_items[] = {
	{ .kind = MENU_CHOICE, .label = "Frame display", .value = &options.emulator.hud_mode, .choices = hud_modes, .count = 2, .help = "Shows the emulator's timing over the game: how long each frame takes, and where the time goes. Holding the right trigger in a game turns it on and off." },
	{ .kind = MENU_CHOICE, .label = "Timing", .value = &options.emulator.pacing, .choices = pacing_modes, .count = FRNES_PACING_MODES, .help = "What sets the game's speed. Vsync shows one frame per screen refresh (smooth scrolling, sound resampled to match). Sound runs at the NES's own rate, held by the sound." },
	{ .kind = MENU_TEXT, .label = "" },
	{ .kind = MENU_ACTION, .label = "Save options to VMU", .action = save_options, .help = "Saves these options to a VMU, as FrNES.opt." },
	{ .kind = MENU_ACTION, .label = "Load options from VMU", .action = load_options, .help = "Loads the options saved on a VMU, replacing the ones in use." },
	{ .kind = MENU_ACTION, .label = "Delete options from VMU", .action = delete_options, .help = "Deletes FrNES.opt from the VMU. The options in use stay as they are." },
	{ .kind = MENU_ACTION, .label = "Restore defaults", .action = default_options, .help = "Puts every option back to its default (not saved until you save to the VMU)." },
	{ .kind = MENU_TEXT, .label = "" },
	{ .kind = MENU_TEXT, .label = status_line }
};

static const Menu_Page system_page = {
	"System Options", system_items, sizeof(system_items) / sizeof(system_items[0]), options_keys, NUM_OPTIONS_KEYS, &main_menu_page, 0.0f
};

/*-------------------------------------------------------------------*/
/*  Menu colors: each picked in the colour picker                    */
/*-------------------------------------------------------------------*/
static const char* const color_names[] = { "Window border", "Window inside", "Text", "Highlight", "Background" };
#define NUM_COLORS 5

static uint32* color_of(int color) {
	switch (color) {
		case 0: return &options.GUI_OutsideWindowColor;
		case 1: return &options.GUI_InsideWindowColor;
		case 2: return &options.GUI_TextColor;
		case 3: return &options.GUI_SelectedTextColor;
		default: return &options.GUI_BGColor;
	}
}

static void colors_changed() {
	const Menu_Page* page = menu_current();
	int highlighted = mydata.highlighted_index;
	menu_style_windows(page -> title, page -> keys, page -> key_count, page -> text_scale);
	mydata.highlighted_index = highlighted;
}

static void pick_color(const Menu_Item* item) {
	color_picker_open(color_names[item -> tag], color_of(item -> tag), colors_changed);
}

static void reset_colors(const Menu_Item* item) {
	(void)item;
	Options_t kept = options;
	options_set_defaults();
	uint32 defaults[NUM_COLORS];
	for (int i = 0; i < NUM_COLORS; i++) {
		defaults[i] = *color_of(i);
	}
	options = kept;
	for (int i = 0; i < NUM_COLORS; i++) {
		*color_of(i) = defaults[i];
	}
	colors_changed();
}

// (labels from color_names: not constant expressions, so filled in by color_page_prepare)
static Menu_Item color_items[] = {
	{ .kind = MENU_ACTION, .action = pick_color, .tag = 0, .help = "The windows' border. A opens the colour picker." },
	{ .kind = MENU_ACTION, .action = pick_color, .tag = 1, .help = "The inside of the windows. A opens the colour picker." },
	{ .kind = MENU_ACTION, .action = pick_color, .tag = 2, .help = "The text in the windows. A opens the colour picker." },
	{ .kind = MENU_ACTION, .action = pick_color, .tag = 3, .help = "The highlighted row's text, its bar and the focus arrow. A opens the colour picker." },
	{ .kind = MENU_ACTION, .action = pick_color, .tag = 4, .help = "Behind the windows. A opens the colour picker." },
	{ .kind = MENU_TEXT, .label = "" },
	{ .kind = MENU_ACTION, .label = "Reset the colors", .action = reset_colors, .help = "Puts the menus' colours back to FrNES's own." }
};

// a swatch of each colour at the right of its row
static void draw_colors(uint32 list) {
	if (list == PVR_LIST_OP_POLY) {
		pvr_poly_cxt_t cxt;
		pvr_poly_hdr_t hdr;
		pvr_poly_cxt_col(&cxt, list);
		pvr_poly_compile(&hdr, &cxt);
		pvr_prim(&hdr, sizeof(hdr));
		float line = mydata.font -> fontHeight * mystyle.text_scale;
		float y = mydata.y + 15.0f + mydata.header_font -> fontHeight * mystyle.header_text_scale;
		float x2 = mydata.x + mydata.width - 30.0f;
		for (int i = 0; i < NUM_COLORS; i++, y += line) {
			pvr_vertex_t v;
			v.z = 33.0f;
			v.oargb = 0;
			v.u = v.v = 0.0f;
			for (int layer = 0; layer < 2; layer++) {
				float inset = layer ? 2.0f : 0.0f;
				v.argb = layer ? *color_of(i) : 0xFF202020;
				v.z = 33.0f + layer * 0.1f;
				v.flags = PVR_CMD_VERTEX;
				v.x = x2 - 60.0f + inset; v.y = y + line - 2.0f - inset; pvr_prim(&v, sizeof(v));
				v.x = x2 - 60.0f + inset; v.y = y + 2.0f + inset; pvr_prim(&v, sizeof(v));
				v.x = x2 - inset; v.y = y + line - 2.0f - inset; pvr_prim(&v, sizeof(v));
				v.flags = PVR_CMD_VERTEX_EOL;
				v.x = x2 - inset; v.y = y + 2.0f + inset; pvr_prim(&v, sizeof(v));
			}
		}
	}
	color_picker_draw(list);
}

static int colors_input(uint32 input) {
	return color_picker_input(input);
}

static const Menu_Page color_page = {
	"Menu Colors", color_items, sizeof(color_items) / sizeof(color_items[0]), options_keys, NUM_OPTIONS_KEYS, &main_menu_page, 0.0f,
	draw_colors, colors_input, 0.0f
};

static void color_page_prepare() {
	for (int i = 0; i < NUM_COLORS; i++) {
		color_items[i].label = color_names[i];
	}
}

/*-------------------------------------------------------------------*/
/*  Help and About                                                   */
/*-------------------------------------------------------------------*/
// Help and About are Markdown files on the romdisk (frontend/assets), laid out as the page opens
#define TEXT_PAGE_LEFT (mydata.x + 15.0f)
#define TEXT_PAGE_TOP (mydata.y + 15.0f + mydata.header_font -> fontHeight * mystyle.header_text_scale)
// (the scroll bar to the right of the text, inside the window's border as the lists' is)
#define TEXT_PAGE_SCROLL_X (mydata.x + mydata.width - mystyle.border_thickness - SCROLL_BAR_WIDTH - 2.0f)
#define TEXT_PAGE_WIDTH (TEXT_PAGE_SCROLL_X - 10.0f - TEXT_PAGE_LEFT)
#define TEXT_PAGE_HEIGHT (mydata.y + mydata.height - 8.0f - TEXT_PAGE_TOP)
#define TEXT_LINE 16.0f

static float text_scroll = 0.0f;
// the links in view, in the status window
#define TEXT_LINKS 5
static char link_text[TEXT_LINKS][96];
static char* link_lines[TEXT_LINKS];

static void text_page_links() {
	int count = markdown_links(text_scroll, TEXT_PAGE_HEIGHT, link_text, TEXT_LINKS);
	for (int i = 0; i < count; i++) {
		link_lines[i] = link_text[i];
	}
	helpdata.data_strings = link_lines;
	helpdata.num_strings = count;
	helpdata.top_index = 0;
}

static void draw_text_page(uint32 list) {
	markdown_draw(list, TEXT_PAGE_LEFT, TEXT_PAGE_TOP, TEXT_PAGE_HEIGHT, text_scroll);
	// (the scroll bar while the page is longer than the window)
	if ((list == PVR_LIST_OP_POLY) && (markdown_height() > TEXT_PAGE_HEIGHT)) {
		win_draw_scroll_bar(list, TEXT_PAGE_SCROLL_X, TEXT_PAGE_TOP, TEXT_PAGE_TOP + TEXT_PAGE_HEIGHT, TEXT_PAGE_HEIGHT,
			markdown_height(), text_scroll);
	}
}

// up / down a line, the triggers a page
static int text_page_input(uint32 input) {
	float step = 0.0f;
	if (input & INPUT_UP) step -= TEXT_LINE;
	if (input & INPUT_DOWN) step += TEXT_LINE;
	if (input & INPUT_PAGE_UP) step -= TEXT_PAGE_HEIGHT - TEXT_LINE;
	if (input & INPUT_PAGE_DOWN) step += TEXT_PAGE_HEIGHT - TEXT_LINE;
	if (step == 0.0f) {
		return 0;
	}
	float last = markdown_height() - TEXT_PAGE_HEIGHT;
	text_scroll += step;
	if (text_scroll > last) {
		text_scroll = last;
	}
	if (text_scroll < 0.0f) {
		text_scroll = 0.0f;
	}
	text_page_links();
	return 1;
}

static const window_key_t scroll_keys[] = {
	{ KEY_DPAD, "Scroll" },
	{ KEY_LT, NULL },
	{ KEY_RT, "Page" },
	{ KEY_B, "Back" }
};

static const Menu_Page help_page = {
	"Help", NULL, 0, scroll_keys, 4, &main_menu_page, 0.5f, draw_text_page, text_page_input, 0.0f
};

static const Menu_Page about_page = {
	"About FrNES", NULL, 0, scroll_keys, 4, &main_menu_page, 0.5f, draw_text_page, text_page_input, 0.0f
};

static void open_text_page(const Menu_Page* page, const char* path) {
	menu_open(page);
	text_scroll = 0.0f;
	if (markdown_load(path, TEXT_PAGE_WIDTH, TEXT_PAGE_HEIGHT) != 0) {
		printf("pages: unable to read [%s]\n", path);
	}
	text_page_links();
}

/*-------------------------------------------------------------------*/
/*  Main menu                                                        */
/*-------------------------------------------------------------------*/
// tag: the page to open (NULL: the ROM browser)
static const Menu_Page* const pages[] = { NULL, &video_page, &port_page, &system_page, &color_page, &help_page, &about_page, &saves_page, &audio_page };

static void open_page(const Menu_Item* item) {
	status_line[0] = '\0';
	if (pages[item -> tag] == NULL) {
		browser_open();
	} else {
		video_items[0].choices = palettes_names();
		video_items[0].count = palettes_count();
		// (Controls opens on port A; the VMU page lists what's plugged in now)
		controls_tab = 0;
		vmu_page_prepare();
		color_page_prepare();
		if (pages[item -> tag] == &help_page) {
			open_text_page(&help_page, "/rd/help.md");
		} else if (pages[item -> tag] == &about_page) {
			open_text_page(&about_page, "/rd/about.md");
		} else {
			menu_open((pages[item -> tag] == &port_page) ? controls_page() : pages[item -> tag]);
		}
	}
}

#define MAIN_ITEM(name, page, text) { .kind = MENU_ACTION, .label = name, .action = open_page, .tag = page, .help = text }
#define MAIN_ITEMS_START \
	MAIN_ITEM("Select a ROM", 0, "Browse the CD, SD card, PC (dc-load) or romdisk for an NES or FDS game to play."), \
	MAIN_ITEM("Video Options", 1, "The palette, picture size, smoothing and the edges to hide."), \
	MAIN_ITEM("Audio Options", 8, "The sound on or off, its filter and its quality."), \
	MAIN_ITEM("Controls", 2, "Which Dreamcast buttons press the NES buttons, for each port, and the hotkeys."), \
	MAIN_ITEM("Saves", 7, "Where battery saves and save states go: the VMUs and the SD card.")
#define MAIN_ITEMS_END \
	MAIN_ITEM("System Options", 3, "Timing, the frame display, and saving these options to a VMU."), \
	MAIN_ITEM("Menu Colors", 4, "The colours of these menus."), \
	MAIN_ITEM("Help", 5, "How to use FrNES."), \
	MAIN_ITEM("About", 6, "FrNES's version, who made it and thanks.")

static const Menu_Item main_items[] = {
	MAIN_ITEMS_START,
	MAIN_ITEMS_END
};

Menu_Page main_menu_page = {
	"Main Menu", main_items, sizeof(main_items) / sizeof(main_items[0]), main_keys, 2, NULL, 0.0f
};

void pages_sd_present(int present) {
	snprintf(sd_line, sizeof(sd_line), "SD card: %s", present ? "found" : "not found");
}
