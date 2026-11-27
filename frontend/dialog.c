/*===================================================================*/
/*                                                                   */
/*  dialog.c : A message over the screen, closed with A              */
/*                                                                   */
/*===================================================================*/

#include "dialog.h"
#include "frontend.h"
#include "input.h"
#include "options.h"
#include "text_window.h"

// in front of the screen's windows (z 30-36) and the shade over them
#define DIALOG_Z_LIFT 20.0f
#define SHADE_Z 40.0f
#define SHADE_COLOR 0x80000000

// its width (wider only for a title that needs it); lines wider than its rows are wrapped onto more rows, and the
// dialog grows down to hold them
#define DIALOG_WIDTH 480.0f
#define DIALOG_MAX_WIDTH 600.0f
// the rows the lines are wrapped into
#define DIALOG_ROWS 12
#define DIALOG_TEXT_SCALE 0.55f
// the key line at the bottom: its height, and its room under the text
#define KEY_LINE 20.0f
#define KEY_ROOM 34.0f

static const window_key_t dialog_keys[] = {
	{ KEY_A, "OK" }
};

static int active = 0;
static char title[DIALOG_LINE_LENGTH];
static char text[DIALOG_ROWS][DIALOG_LINE_LENGTH];
static char* text_lines[DIALOG_ROWS];
static window_data_t data;
static window_style_t style;

// a line into rows from 'row' on, each no wider than 'room' (broken at spaces, or anywhere in a word too long for a
// row: a path); the rows used so far, the widest in *widest
static int wrap(const char* line, float room, int row, float* widest) {
	const char* at = line;
	do {
		if (row >= DIALOG_ROWS) {
			return row;
		}
		size_t length = strlen(at);
		if (length > DIALOG_LINE_LENGTH - 1) {
			length = DIALOG_LINE_LENGTH - 1;
		}
		// the most that fits, then back to the last space in it when there's one (unless all of it fits)
		size_t fit = 0;
		float width = 0.0f;
		for (size_t n = 1; n <= length; n++) {
			char part[DIALOG_LINE_LENGTH];
			memcpy(part, at, n);
			part[n] = '\0';
			float measured;
			measureLine(font_mono, part, DIALOG_TEXT_SCALE, &measured);
			if ((measured > room) && (fit > 0)) {
				break;
			}
			fit = n;
			width = measured;
		}
		size_t take = fit;
		if (fit < strlen(at)) {
			size_t space = fit;
			while ((space > 0) && (at[space] != ' ')) {
				space--;
			}
			if (space > 0) {
				take = space;
				char part[DIALOG_LINE_LENGTH];
				memcpy(part, at, take);
				part[take] = '\0';
				measureLine(font_mono, part, DIALOG_TEXT_SCALE, &width);
			}
		}
		memcpy(text[row], at, take);
		text[row][take] = '\0';
		if (width > *widest) {
			*widest = width;
		}
		row++;
		at += take;
		while (*at == ' ') {
			at++;
		}
	} while (*at);
	return row;
}

void dialog_show(const char* dialog_title, const char* const* lines, int count) {
	if (count > DIALOG_LINES) {
		count = DIALOG_LINES;
	}
	snprintf(title, sizeof(title), "%s", dialog_title);
	// (a window's rows have its width less the margins and borders: 2 x 15 + 2 x 5)
	const float sides = 40.0f;
	float widest = 0.0f;
	int rows = 0;
	for (int i = 0; (i < count) && (rows < DIALOG_ROWS); i++) {
		rows = wrap(lines[i], DIALOG_WIDTH - sides, rows, &widest);
	}
	for (int i = 0; i < rows; i++) {
		text_lines[i] = text[i];
	}
	count = rows;
	float title_width;
	measureLine(font, title, 0.8f, &title_width);
	if (title_width > widest) {
		widest = title_width;
	}

	memset(&data, 0, sizeof(data));
	data.font = font_mono;
	data.header_font = font;
	data.header_text = title;
	data.data_strings = text_lines;
	data.num_strings = count;
	data.highlighted_index = -1;

	memset(&style, 0, sizeof(style));
	style.header_text_scale = 0.8f;
	style.text_scale = DIALOG_TEXT_SCALE;
	style.border_thickness = 5.0f;
	style.border_color = options.GUI_OutsideWindowColor;
	style.inside_color = options.GUI_InsideWindowColor;
	style.left_margin = 15;
	style.header_text_color = options.GUI_TextColor;
	style.text_color = options.GUI_TextColor;
	style.selected_text_color = options.GUI_SelectedTextColor;
	style.selected_background_color = options.GUI_SelectedTextColor;
	style.max_items = count;

	// (as tall as its lines need, centred on the screen; the lines start under the header as in any window)
	float header = font -> fontHeight * style.header_text_scale;
	float lines_height = count * font_mono -> fontHeight * DIALOG_TEXT_SCALE;
	data.width = widest + sides + 4.0f;
	if (data.width < DIALOG_WIDTH) {
		data.width = DIALOG_WIDTH;
	}
	if (data.width > DIALOG_MAX_WIDTH) {
		data.width = DIALOG_MAX_WIDTH;
	}
	data.height = 15.0f + header + lines_height + KEY_ROOM;
	data.x = (640.0f - data.width) / 2;
	data.y = (480.0f - data.height) / 2;
	active = 1;
}

int dialog_active() {
	return active;
}

void dialog_handle(uint32 input) {
	if (input & (INPUT_ACCEPT | INPUT_BACK)) {
		active = 0;
	}
}

static void draw_shade() {
	pvr_poly_cxt_t cxt;
	pvr_poly_hdr_t hdr;
	pvr_poly_cxt_col(&cxt, PVR_LIST_TR_POLY);
	pvr_poly_compile(&hdr, &cxt);
	pvr_prim(&hdr, sizeof(hdr));
	pvr_vertex_t v;
	v.z = SHADE_Z;
	v.argb = SHADE_COLOR;
	v.oargb = 0;
	v.u = v.v = 0.0f;
	v.flags = PVR_CMD_VERTEX;
	v.x = 0.0f; v.y = 480.0f; pvr_prim(&v, sizeof(v));
	v.x = 0.0f; v.y = 0.0f; pvr_prim(&v, sizeof(v));
	v.x = 640.0f; v.y = 480.0f; pvr_prim(&v, sizeof(v));
	v.flags = PVR_CMD_VERTEX_EOL;
	v.x = 640.0f; v.y = 0.0f; pvr_prim(&v, sizeof(v));
}

void dialog_draw(uint32 list) {
	if (!active) {
		return;
	}
	if (list == PVR_LIST_OP_POLY) {
		// (the window's layers and the key's icon are flat coloured)
		pvr_poly_cxt_t cxt;
		pvr_poly_hdr_t hdr;
		pvr_poly_cxt_col(&cxt, list);
		pvr_poly_compile(&hdr, &cxt);
		pvr_prim(&hdr, sizeof(hdr));
	} else {
		draw_shade();
	}
	win_set_z_lift(DIALOG_Z_LIFT);
	win_draw_textwindow(&data, &style, list);
	// the key at the bottom right
	float width;
	measureLine(font_mono, (char*)dialog_keys[0].label, 0.5f, &width);
	float x = data.x + data.width - style.left_margin - 20.0f - 6.0f - width - 4.0f;
	float y = data.y + data.height - KEY_LINE - 10.0f;
	win_draw_keys(font_mono, dialog_keys, 1, x, y, style.text_color, list);
	win_set_z_lift(0.0f);
}
