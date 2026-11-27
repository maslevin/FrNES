/*===================================================================*/
/*                                                                   */
/*  viewer.c : A game's boxes and manual, over the screen            */
/*                                                                   */
/*===================================================================*/

#include "viewer.h"
#include "frontend.h"
#include "input.h"
#include "options.h"
#include "text_window.h"

#include <math.h>

// in front of everything else on the screen
#define VIEWER_Z 70.0f
#define ZOOM_MAX 4.0f

static int active = 0;
static Art_Game game;
// showing: the box (US or Famicom), or the manual's pages
static int manual = 0;
static int famicom = 0;
// (in the manual: the Famicom one's pages, pagej000.dt..)
static int manual_j = 0;
static int page = 0;
static float zoom = 1.0f;
// the middle of the view, as a fraction of the page (0-1 each way)
static float view_x = 0.5f, view_y = 0.5f;
static Art_Texture shown;
static char loaded[16] = "";

// the box's keys: A only with a manual, X only with both boxes
static window_key_t box_keys[3];
static int box_key_count = 0;

// the manual's keys: X only with both manuals
static window_key_t manual_keys[5];
static int manual_key_count = 0;

// the manual showing's pages
static int manual_pages() {
	return manual_j ? game.pages_j : game.pages;
}

// the texture for what's showing, loaded when that changes
static void show(const char* which) {
	if (strcmp(which, loaded) == 0) {
		return;
	}
	art_free(&shown);
	loaded[0] = '\0';
	if (art_load(&game, which, &shown) == 0) {
		snprintf(loaded, sizeof(loaded), "%s", which);
	}
}

static void show_current() {
	char which[16];
	if (manual) {
		snprintf(which, sizeof(which), manual_j ? "pagej%03d" : "page%03d", page);
	} else {
		snprintf(which, sizeof(which), "%s", (famicom || !game.has_box) ? "box_j" : "box");
	}
	show(which);
}

void viewer_open(const Art_Game* art) {
	game = *art;
	manual = 0;
	manual_j = 0;
	famicom = !game.has_box;
	page = 0;
	zoom = 1.0f;
	view_x = view_y = 0.5f;
	loaded[0] = '\0';
	show_current();
	box_key_count = 0;
	if ((game.pages > 0) || (game.pages_j > 0)) {
		box_keys[box_key_count++] = (window_key_t){ KEY_A, "Manual" };
	}
	manual_key_count = 0;
	manual_keys[manual_key_count++] = (window_key_t){ KEY_DPAD, "Page / move" };
	manual_keys[manual_key_count++] = (window_key_t){ KEY_LT, NULL };
	manual_keys[manual_key_count++] = (window_key_t){ KEY_RT, "Zoom" };
	if ((game.pages > 0) && (game.pages_j > 0)) {
		manual_keys[manual_key_count++] = (window_key_t){ KEY_X, "US / Famicom manual" };
	}
	manual_keys[manual_key_count++] = (window_key_t){ KEY_B, "Back" };
	if (game.has_box && game.has_box_j) {
		box_keys[box_key_count++] = (window_key_t){ KEY_X, "US / Famicom box" };
	}
	box_keys[box_key_count++] = (window_key_t){ KEY_B, "Back" };
	active = 1;
}

int viewer_active() {
	return active;
}

static float clamp(float x, float low, float high) {
	return (x < low) ? low : ((x > high) ? high : x);
}

int viewer_input(uint32 input) {
	if (!active) {
		return 0;
	}
	if (input & INPUT_BACK) {
		if (manual) {
			// (back from the manual to the box)
			manual = 0;
			show_current();
		} else {
			art_free(&shown);
			loaded[0] = '\0';
			active = 0;
		}
		return 1;
	}
	if (!manual) {
		if ((input & INPUT_ACCEPT) && ((game.pages > 0) || (game.pages_j > 0))) {
			// (the manual of the box's country when it has one, else the other)
			int showing_famicom = famicom || !game.has_box;
			int want_j = (showing_famicom && (game.pages_j > 0)) || (game.pages == 0);
			// (the page kept from last time in the same manual)
			if (want_j != manual_j) {
				page = 0;
			}
			manual_j = want_j;
			manual = 1;
			zoom = 1.0f;
			view_x = view_y = 0.5f;
		}
		if ((input & INPUT_X) && game.has_box && game.has_box_j) {
			famicom = !famicom;
		}
		show_current();
		return 1;
	}
	// the manual: X switches between the US and Famicom ones (from the first page), at full page left / right turn the
	// page; zoomed in, the D-pad moves (and the stick, always)
	if ((input & INPUT_X) && (game.pages > 0) && (game.pages_j > 0)) {
		manual_j = !manual_j;
		page = 0;
		zoom = 1.0f;
		view_x = view_y = 0.5f;
	}
	int pages = manual_pages();
	float step = 0.08f / zoom;
	if (zoom <= 1.0f) {
		if ((input & INPUT_RIGHT) && (page + 1 < pages)) {
			page++;
		}
		if ((input & INPUT_LEFT) && (page > 0)) {
			page--;
		}
	} else {
		if (input & INPUT_LEFT) view_x -= step;
		if (input & INPUT_RIGHT) view_x += step;
	}
	if (input & INPUT_UP) view_y -= step;
	if (input & INPUT_DOWN) view_y += step;
	if (input & INPUT_PAGE_DOWN) zoom = fminf(ZOOM_MAX, zoom * 1.5f);
	if (input & INPUT_PAGE_UP) zoom = fmaxf(1.0f, zoom / 1.5f);
	if (input & INPUT_START) {
		// (Start: the next page, zoomed in or not, back at its top left)
		if (page + 1 < pages) {
			page++;
			view_x = view_y = 0.0f;
		}
	}
	maple_device_t* pad = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
	cont_state_t* state = pad ? (cont_state_t*)maple_dev_status(pad) : NULL;
	if (state != NULL) {
		int x = state -> joyx, y = state -> joyy;
		if ((x > 24) || (x < -24)) view_x += x / 128.0f * 0.02f / zoom;
		if ((y > 24) || (y < -24)) view_y += y / 128.0f * 0.02f / zoom;
	}
	// (the view stays on the page)
	float half = 0.5f / zoom;
	view_x = clamp(view_x, half, 1.0f - half);
	view_y = clamp(view_y, half, 1.0f - half);
	show_current();
	return 1;
}

static void draw_shade(uint32 list) {
	pvr_poly_cxt_t cxt;
	pvr_poly_hdr_t hdr;
	pvr_poly_cxt_col(&cxt, list);
	pvr_poly_compile(&hdr, &cxt);
	pvr_prim(&hdr, sizeof(hdr));
	pvr_vertex_t v;
	v.z = VIEWER_Z - 1.0f;
	v.argb = 0xE0000000;
	v.oargb = 0;
	v.u = v.v = 0.0f;
	v.flags = PVR_CMD_VERTEX;
	v.x = 0.0f; v.y = 480.0f; pvr_prim(&v, sizeof(v));
	v.x = 0.0f; v.y = 0.0f; pvr_prim(&v, sizeof(v));
	v.x = 640.0f; v.y = 480.0f; pvr_prim(&v, sizeof(v));
	v.flags = PVR_CMD_VERTEX_EOL;
	v.x = 640.0f; v.y = 0.0f; pvr_prim(&v, sizeof(v));
}

void viewer_draw(uint32 list) {
	if (!active) {
		return;
	}
	if (list != PVR_LIST_TR_POLY) {
		// (the key icons' circles and bars)
		pvr_poly_cxt_t cxt;
		pvr_poly_hdr_t hdr;
		pvr_poly_cxt_col(&cxt, list);
		pvr_poly_compile(&hdr, &cxt);
		pvr_prim(&hdr, sizeof(hdr));
		win_set_z_lift(VIEWER_Z);
		win_draw_keys(font_mono, manual ? manual_keys : box_keys, manual ? manual_key_count : box_key_count, 20.0f, 452.0f, 0xFFFFFFFF, list);
		win_set_z_lift(0.0f);
		return;
	}
	draw_shade(list);
	char caption[128];
	if (manual) {
		float half = 0.5f / zoom;
		art_draw(&shown, 0.0f, 0.0f, 640.0f, 444.0f, VIEWER_Z + 1.0f, view_x - half, view_y - half, view_x + half, view_y + half);
		snprintf(caption, sizeof(caption), "%s%s: page %d of %d%s", game.name, ((game.pages > 0) && (game.pages_j > 0)) ?
			(manual_j ? " (Famicom manual)" : " (US manual)") : "", page + 1, manual_pages(), (zoom > 1.0f) ? " (zoomed)" : "");
	} else {
		art_draw(&shown, 20.0f, 20.0f, 600.0f, 410.0f, VIEWER_Z + 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		snprintf(caption, sizeof(caption), "%s (%s box)%s", game.name, (famicom || !game.has_box) ? "Famicom" : "US",
			(game.pages || game.pages_j) ? "" : ": no manual");
	}
	draw_string(font_mono, list, caption, 20.0f, 432.0f, VIEWER_Z + 2.0f, 600.0f, 20.0f, SINGLE, LEFT, 0xFFFFFFFF, 0.5f);
	win_set_z_lift(VIEWER_Z);
	win_draw_keys(font_mono, manual ? manual_keys : box_keys, manual ? manual_key_count : box_key_count, 20.0f, 452.0f, 0xFFFFFFFF, list);
	win_set_z_lift(0.0f);
}
