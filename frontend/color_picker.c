/*===================================================================*/
/*                                                                   */
/*  color_picker.c : A colour picked from a hue / saturation field   */
/*                                                                   */
/*===================================================================*/

#include "color_picker.h"
#include <math.h>
#include "frontend.h"
#include "input.h"
#include "options.h"
#include "text_window.h"

// in front of the screen's windows (as a dialog is)
#define PICKER_Z 50.0f
#define PICKER_WIDTH 440.0f
#define PICKER_HEIGHT 250.0f
#define PICKER_LEFT ((640.0f - PICKER_WIDTH) / 2)
#define PICKER_TOP ((480.0f - PICKER_HEIGHT) / 2)

// the field: hue across, saturation down (full at the top); the brightness bar beside it
#define FIELD_LEFT (PICKER_LEFT + 20.0f)
#define FIELD_TOP (PICKER_TOP + 52.0f)
#define FIELD_WIDTH 252.0f
#define FIELD_HEIGHT 150.0f
#define BAR_LEFT (FIELD_LEFT + FIELD_WIDTH + 14.0f)
#define BAR_WIDTH 20.0f
#define PREVIEW_LEFT (BAR_LEFT + BAR_WIDTH + 18.0f)
#define PREVIEW_SIZE 56.0f
#define HUE_STEPS 12

static int active = 0;
static const char* picker_name = "";
static uint32* target = NULL;
static uint32 original = 0;
static void (*on_change)(void) = NULL;
// the colour as hue (0-1), saturation (0-1), value (0-1)
static float hue = 0.0f, saturation = 0.0f, value = 1.0f;

static const window_key_t picker_keys[] = {
	{ KEY_DPAD, "Colour" },
	{ KEY_LT, NULL },
	{ KEY_RT, "Brightness" },
	{ KEY_A, "Keep" },
	{ KEY_B, "Cancel" }
};

static uint32 hsv_to_argb(float h, float s, float v) {
	float r = v, g = v, b = v;
	if (s > 0.0f) {
		float sector = (h - floorf(h)) * 6.0f;
		int i = (int)sector;
		float f = sector - i;
		float p = v * (1.0f - s), q = v * (1.0f - s * f), t = v * (1.0f - s * (1.0f - f));
		switch (i) {
			case 0: r = v; g = t; b = p; break;
			case 1: r = q; g = v; b = p; break;
			case 2: r = p; g = v; b = t; break;
			case 3: r = p; g = q; b = v; break;
			case 4: r = t; g = p; b = v; break;
			default: r = v; g = p; b = q; break;
		}
	}
	return 0xFF000000 | ((uint32)(r * 255.0f + 0.5f) << 16) | ((uint32)(g * 255.0f + 0.5f) << 8) | (uint32)(b * 255.0f + 0.5f);
}

static void argb_to_hsv(uint32 color, float* h, float* s, float* v) {
	float r = ((color >> 16) & 0xff) / 255.0f, g = ((color >> 8) & 0xff) / 255.0f, b = (color & 0xff) / 255.0f;
	float max = fmaxf(r, fmaxf(g, b)), min = fminf(r, fminf(g, b)), delta = max - min;
	*v = max;
	*s = (max > 0.0f) ? delta / max : 0.0f;
	if (delta <= 0.0f) {
		// (a grey: the hue stays where it was)
		return;
	}
	float hh;
	if (max == r) {
		hh = (g - b) / delta;
	} else if (max == g) {
		hh = 2.0f + (b - r) / delta;
	} else {
		hh = 4.0f + (r - g) / delta;
	}
	hh /= 6.0f;
	*h = hh - floorf(hh);
}

static void apply() {
	*target = hsv_to_argb(hue, saturation, value);
	if (on_change != NULL) {
		on_change();
	}
}

void color_picker_open(const char* name, uint32* color, void (*changed)(void)) {
	picker_name = name;
	target = color;
	original = *color;
	on_change = changed;
	argb_to_hsv(*color, &hue, &saturation, &value);
	active = 1;
}

static float clamp01(float x) {
	return (x < 0.0f) ? 0.0f : ((x > 1.0f) ? 1.0f : x);
}

int color_picker_input(uint32 input) {
	if (!active) {
		return 0;
	}
	if (input & INPUT_ACCEPT) {
		active = 0;
		return 1;
	}
	if (input & INPUT_BACK) {
		*target = original;
		if (on_change != NULL) {
			on_change();
		}
		active = 0;
		return 1;
	}
	float dh = 0.0f, ds = 0.0f, dv = 0.0f;
	// the D-pad a step at a time (repeating while held), the stick smoothly, the triggers for the brightness
	if (input & INPUT_LEFT) dh -= 1.0f / 48.0f;
	if (input & INPUT_RIGHT) dh += 1.0f / 48.0f;
	if (input & INPUT_UP) ds += 1.0f / 30.0f;
	if (input & INPUT_DOWN) ds -= 1.0f / 30.0f;
	if (input & INPUT_PAGE_UP) dv -= 1.0f / 20.0f;
	if (input & INPUT_PAGE_DOWN) dv += 1.0f / 20.0f;
	maple_device_t* pad = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
	cont_state_t* state = pad ? (cont_state_t*)maple_dev_status(pad) : NULL;
	if (state != NULL) {
		int x = state -> joyx, y = state -> joyy;
		if ((x > 24) || (x < -24)) dh += x / 128.0f / 90.0f;
		if ((y > 24) || (y < -24)) ds -= y / 128.0f / 60.0f;
	}
	if ((dh == 0.0f) && (ds == 0.0f) && (dv == 0.0f)) {
		return 1;
	}
	hue += dh;
	hue -= floorf(hue);
	saturation = clamp01(saturation + ds);
	value = clamp01(value + dv);
	apply();
	return 1;
}

static void quad(float x1, float y1, float x2, float y2, float z, uint32 top_left, uint32 top_right, uint32 bottom_left, uint32 bottom_right) {
	pvr_vertex_t v;
	v.z = z;
	v.oargb = 0;
	v.u = v.v = 0.0f;
	v.flags = PVR_CMD_VERTEX;
	v.argb = bottom_left; v.x = x1; v.y = y2; pvr_prim(&v, sizeof(v));
	v.argb = top_left; v.x = x1; v.y = y1; pvr_prim(&v, sizeof(v));
	v.argb = bottom_right; v.x = x2; v.y = y2; pvr_prim(&v, sizeof(v));
	v.flags = PVR_CMD_VERTEX_EOL;
	v.argb = top_right; v.x = x2; v.y = y1; pvr_prim(&v, sizeof(v));
}

static void frame(float x1, float y1, float x2, float y2, float z, uint32 color) {
	quad(x1 - 2.0f, y1 - 2.0f, x2 + 2.0f, y2 + 2.0f, z, color, color, color, color);
}

void color_picker_draw(uint32 list) {
	if (!active) {
		return;
	}
	if (list == PVR_LIST_TR_POLY) {
		// a shade over the screen, the title, the keys
		pvr_poly_cxt_t cxt;
		pvr_poly_hdr_t hdr;
		pvr_poly_cxt_col(&cxt, list);
		pvr_poly_compile(&hdr, &cxt);
		pvr_prim(&hdr, sizeof(hdr));
		quad(0.0f, 0.0f, 640.0f, 480.0f, 40.0f, 0x80000000, 0x80000000, 0x80000000, 0x80000000);
		draw_string(font, list, (char*)picker_name, PICKER_LEFT + 18.0f, PICKER_TOP + 12.0f, PICKER_Z + 5.0f, PICKER_WIDTH - 36.0f, 40.0f, SINGLE, LEFT,
			0xFF00FF00, 0.8f);
		draw_string(font_mono, list, "Was", PREVIEW_LEFT, FIELD_TOP + PREVIEW_SIZE + 4.0f, PICKER_Z + 5.0f, 60.0f, 20.0f, SINGLE, LEFT, options.GUI_TextColor, 0.5f);
		draw_string(font_mono, list, "Now", PREVIEW_LEFT, FIELD_TOP + 2 * PREVIEW_SIZE + 24.0f, PICKER_Z + 5.0f, 60.0f, 20.0f, SINGLE, LEFT, options.GUI_TextColor, 0.5f);
		char text[16];
		snprintf(text, sizeof(text), "%06lX", (unsigned long)(*target & 0xffffff));
		draw_string(font_mono, list, text, PREVIEW_LEFT, FIELD_TOP + 2 * PREVIEW_SIZE + 42.0f, PICKER_Z + 5.0f, 80.0f, 20.0f, SINGLE, LEFT, options.GUI_TextColor, 0.5f);
		win_set_z_lift(PICKER_Z - 30.0f);
		win_draw_keys(font_mono, picker_keys, 5, PICKER_LEFT + 18.0f, PICKER_TOP + PICKER_HEIGHT - 32.0f, options.GUI_TextColor, list);
		win_set_z_lift(0.0f);
		return;
	}
	pvr_poly_cxt_t cxt;
	pvr_poly_hdr_t hdr;
	pvr_poly_cxt_col(&cxt, list);
	cxt.gen.culling = PVR_CULLING_NONE;
	pvr_poly_compile(&hdr, &cxt);
	// the window
	draw_cornered_layer(list, PICKER_LEFT, PICKER_TOP, PICKER_Z, PICKER_WIDTH, PICKER_HEIGHT, 16.0f, options.GUI_OutsideWindowColor);
	draw_cornered_layer(list, PICKER_LEFT + 5.0f, PICKER_TOP + 5.0f, PICKER_Z + 0.1f, PICKER_WIDTH - 10.0f, PICKER_HEIGHT - 10.0f, 11.0f,
		options.GUI_InsideWindowColor);
	pvr_prim(&hdr, sizeof(hdr));
	// the field: each hue step shaded from full saturation at the top to grey at the bottom, at the brightness chosen
	float z = PICKER_Z + 1.0f;
	frame(FIELD_LEFT, FIELD_TOP, FIELD_LEFT + FIELD_WIDTH, FIELD_TOP + FIELD_HEIGHT, z, 0xFF202020);
	uint32 grey = hsv_to_argb(0.0f, 0.0f, value);
	for (int i = 0; i < HUE_STEPS; i++) {
		float x1 = FIELD_LEFT + FIELD_WIDTH * i / HUE_STEPS, x2 = FIELD_LEFT + FIELD_WIDTH * (i + 1) / HUE_STEPS;
		uint32 left = hsv_to_argb((float)i / HUE_STEPS, 1.0f, value), right = hsv_to_argb((float)(i + 1) / HUE_STEPS, 1.0f, value);
		quad(x1, FIELD_TOP, x2, FIELD_TOP + FIELD_HEIGHT, z + 0.1f, left, right, grey, grey);
	}
	// the brightness bar: this hue and saturation from bright at the top to black
	frame(BAR_LEFT, FIELD_TOP, BAR_LEFT + BAR_WIDTH, FIELD_TOP + FIELD_HEIGHT, z, 0xFF202020);
	uint32 bright = hsv_to_argb(hue, saturation, 1.0f);
	quad(BAR_LEFT, FIELD_TOP, BAR_LEFT + BAR_WIDTH, FIELD_TOP + FIELD_HEIGHT, z + 0.1f, bright, bright, 0xFF000000, 0xFF000000);
	// the cursors: a ring in the field, a notch on the bar
	float cx = FIELD_LEFT + hue * FIELD_WIDTH, cy = FIELD_TOP + (1.0f - saturation) * FIELD_HEIGHT;
	quad(cx - 6.0f, cy - 1.0f, cx + 6.0f, cy + 1.0f, z + 0.3f, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000);
	quad(cx - 1.0f, cy - 6.0f, cx + 1.0f, cy + 6.0f, z + 0.3f, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000);
	quad(cx - 7.0f, cy - 2.0f, cx + 7.0f, cy + 2.0f, z + 0.2f, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
	quad(cx - 2.0f, cy - 7.0f, cx + 2.0f, cy + 7.0f, z + 0.2f, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
	float by = FIELD_TOP + (1.0f - value) * FIELD_HEIGHT;
	quad(BAR_LEFT - 5.0f, by - 2.0f, BAR_LEFT + BAR_WIDTH + 5.0f, by + 2.0f, z + 0.3f, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
	quad(BAR_LEFT - 6.0f, by - 3.0f, BAR_LEFT + BAR_WIDTH + 6.0f, by + 3.0f, z + 0.2f, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000);
	// was and now
	frame(PREVIEW_LEFT, FIELD_TOP, PREVIEW_LEFT + PREVIEW_SIZE, FIELD_TOP + PREVIEW_SIZE, z, 0xFF202020);
	quad(PREVIEW_LEFT, FIELD_TOP, PREVIEW_LEFT + PREVIEW_SIZE, FIELD_TOP + PREVIEW_SIZE, z + 0.1f, original, original, original, original);
	float now_top = FIELD_TOP + PREVIEW_SIZE + 20.0f;
	frame(PREVIEW_LEFT, now_top, PREVIEW_LEFT + PREVIEW_SIZE, now_top + PREVIEW_SIZE, z, 0xFF202020);
	quad(PREVIEW_LEFT, now_top, PREVIEW_LEFT + PREVIEW_SIZE, now_top + PREVIEW_SIZE, z + 0.1f, *target, *target, *target, *target);
	// (the key icons' circles and bars)
	win_set_z_lift(PICKER_Z - 30.0f);
	win_draw_keys(font_mono, picker_keys, 5, PICKER_LEFT + 18.0f, PICKER_TOP + PICKER_HEIGHT - 32.0f, options.GUI_TextColor, list);
	win_set_z_lift(0.0f);
}
