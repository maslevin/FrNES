/*===================================================================*/
/*                                                                   */
/*  text_window.c : The menus' windows, their rows and key lines     */
/*                                                                   */
/*===================================================================*/

#include "text_window.h"

#include <math.h>
#include "macros.h"

// a flat box with rounded corners: a rectangle down the middle and one each side, and a fan of CORNER_TRIANGLES
// triangles in each corner, the corners' angle carried on from one to the next
void draw_cornered_layer(uint32 list, float x, float y, float z, float width, float height, float corner_radius, uint32 color) {
	pvr_poly_hdr_t hdr;
	pvr_poly_cxt_t cxt;
	pvr_vertex_t v;

	pvr_poly_cxt_col(&cxt, list);
	pvr_poly_compile(&hdr, &cxt);
	pvr_prim(&hdr, sizeof(hdr));

	// the middle
	v.flags = PVR_CMD_VERTEX;
	v.x = x + corner_radius;
	v.y = y + height;
	v.z = z;
	v.argb = color;
	v.oargb = 0;
	pvr_prim(&v, sizeof(v));

	v.y = y;
	pvr_prim(&v, sizeof(v));

	v.x = x + width - corner_radius;
	v.y = y + height;
	pvr_prim(&v, sizeof(v));

	v.flags = PVR_CMD_VERTEX_EOL;
	v.y = y;
	pvr_prim(&v, sizeof(v));

	// the left side
	v.flags = PVR_CMD_VERTEX;
	v.x = x;
	v.y = y + height - corner_radius;
	pvr_prim(&v, sizeof(v));

	v.y = y + corner_radius;
	pvr_prim(&v, sizeof(v));

	v.x = x + corner_radius;
	v.y = y + height - corner_radius;
	pvr_prim(&v, sizeof(v));

	v.flags = PVR_CMD_VERTEX_EOL;
	v.y = y + corner_radius;
	pvr_prim(&v, sizeof(v));

	// the right side
	v.flags = PVR_CMD_VERTEX;
	v.x = x + width - corner_radius;
	v.y = y + height - corner_radius;
	pvr_prim(&v, sizeof(v));

	v.y = y + corner_radius;
	pvr_prim(&v, sizeof(v));

	v.x = x + width;
	v.y = y + height - corner_radius;
	pvr_prim(&v, sizeof(v));

	v.flags = PVR_CMD_VERTEX_EOL;
	v.y = y + corner_radius;
	pvr_prim(&v, sizeof(v));

	float theta = 0.0f;
	float increment = -F_PI / (2.0f * CORNER_TRIANGLES);
	float corner_x, corner_y;
	int i;

	// the top right corner, then the top left, bottom left and bottom right
	corner_x = x + width - corner_radius;
	corner_y = y + corner_radius;
	for (i = 0; i < CORNER_TRIANGLES; i++) {
		v.flags = PVR_CMD_VERTEX;
		v.x = corner_x + corner_radius * cosf(theta);
		v.y = corner_y + corner_radius * sinf(theta);
		pvr_prim(&v, sizeof(v));

		v.x = corner_x;
		v.y = corner_y;
		pvr_prim(&v, sizeof(v));

		theta += increment;

		v.flags = PVR_CMD_VERTEX_EOL;
		v.x = corner_x + corner_radius * cosf(theta);
		v.y = corner_y + corner_radius * sinf(theta);
		pvr_prim(&v, sizeof(v));
	}

	corner_x = x + corner_radius;
	corner_y = y + corner_radius;
	for (i = 0; i < CORNER_TRIANGLES; i++) {
		v.flags = PVR_CMD_VERTEX;
		v.x = corner_x + corner_radius * cosf(theta);
		v.y = corner_y + corner_radius * sinf(theta);
		pvr_prim(&v, sizeof(v));

		v.x = corner_x;
		v.y = corner_y;
		pvr_prim(&v, sizeof(v));

		theta += increment;

		v.flags = PVR_CMD_VERTEX_EOL;
		v.x = corner_x + corner_radius * cosf(theta);
		v.y = corner_y + corner_radius * sinf(theta);
		pvr_prim(&v, sizeof(v));
	}

	corner_x = x + corner_radius;
	corner_y = y + height - corner_radius;
	for (i = 0; i < CORNER_TRIANGLES; i++) {
		v.flags = PVR_CMD_VERTEX;
		v.x = corner_x + corner_radius * cosf(theta);
		v.y = corner_y + corner_radius * sinf(theta);
		pvr_prim(&v, sizeof(v));

		v.x = corner_x;
		v.y = corner_y;
		pvr_prim(&v, sizeof(v));

		theta += increment;

		v.flags = PVR_CMD_VERTEX_EOL;
		v.x = corner_x + corner_radius * cosf(theta);
		v.y = corner_y + corner_radius * sinf(theta);
		pvr_prim(&v, sizeof(v));
	}

	corner_x = x + width - corner_radius;
	corner_y = y + height - corner_radius;
	for (i = 0; i < CORNER_TRIANGLES; i++) {
		v.flags = PVR_CMD_VERTEX;
		v.x = corner_x + corner_radius * cosf(theta);
		v.y = corner_y + corner_radius * sinf(theta);
		pvr_prim(&v, sizeof(v));

		v.x = corner_x;
		v.y = corner_y;
		pvr_prim(&v, sizeof(v));

		theta += increment;

		v.flags = PVR_CMD_VERTEX_EOL;
		v.x = corner_x + corner_radius * cosf(theta);
		v.y = corner_y + corner_radius * sinf(theta);
		pvr_prim(&v, sizeof(v));
	}
}

// a slider: its track, and a round knob at 'value' of min to max along it
void draw_scrollbar(uint32 list, float x, float y, float z, float width, float height, uint32 border_color, uint32 inside_color, int min, int max, int value) {
	draw_cornered_layer(list, x, y + (height / 2) - 3.0f, z, width, 6.0f, 5.0f, border_color);

	float knob_radius = height / 2;
	float knob_x = x + (knob_radius) + ((width - (2 * knob_radius)) * (float)value / (float)(max - min));
	float knob_y = y + knob_radius;

	float theta = 0.0f;
	float increment = -2.0f * F_PI / ((float)CIRCLE_TRIANGLES);

	pvr_poly_hdr_t hdr;
	pvr_poly_cxt_t cxt;
	pvr_vertex_t v;

	pvr_poly_cxt_col(&cxt, list);
	pvr_poly_compile(&hdr, &cxt);
	pvr_prim(&hdr, sizeof(hdr));

	for (int i = 0; i < CIRCLE_TRIANGLES; i++) {
		v.flags = PVR_CMD_VERTEX;
		v.x = knob_x + knob_radius * cosf(theta);
		v.y = knob_y + knob_radius * sinf(theta);
		v.z = z + 0.1f;
		v.argb = inside_color;
		v.oargb = 0;
		pvr_prim(&v, sizeof(v));

		v.x = knob_x;
		v.y = knob_y;
		pvr_prim(&v, sizeof(v));

		theta += increment;

		v.flags = PVR_CMD_VERTEX_EOL;
		v.x = knob_x + knob_radius * cosf(theta);
		v.y = knob_y + knob_radius * sinf(theta);
		pvr_prim(&v, sizeof(v));
	}
}

static uint32 row_color(uint32 i, window_data_t* windata, window_style_t* winstyle) {
	return (i + windata -> top_index != windata -> highlighted_index) ? winstyle -> text_color : winstyle -> selected_text_color;
}

// how far in front of the screen's other windows these are drawn (win_set_z_lift: a dialog over them)
static float z_lift = 0.0f;
#define Z(z) ((z) + z_lift)

void win_set_z_lift(float lift) {
	z_lift = lift;
}

// the scroll bar's width, at the window's right inside its border (shown while the list is longer than the window)
#define SCROLL_WIDTH SCROLL_BAR_WIDTH

// the width rows' text has, from the left margin to the right one or to just short of the scroll bar
static float row_width(window_data_t* windata, window_style_t* winstyle) {
	float right = (windata -> num_strings > winstyle -> max_items) ?
		windata -> width - winstyle -> border_thickness - SCROLL_WIDTH - 8.0f :
		windata -> width - winstyle -> left_margin - (2 * winstyle -> border_thickness);
	return right - winstyle -> left_margin - winstyle -> rows_indent - windata -> icon_width - winstyle -> rows_right;
}

// where rows' text starts
static float row_left(window_data_t* windata, window_style_t* winstyle) {
	return windata -> x + winstyle -> left_margin + winstyle -> rows_indent + windata -> icon_width;
}

// text in width at x, cut short with "..." if it's wider
static void draw_fitted(window_data_t* windata, window_style_t* winstyle, uint32 list, const char* text, float x, float yposition, float width, uint32 color) {
	float measured;
	measureLine(windata -> font, (char*)text, winstyle -> text_scale, &measured);
	if (measured <= width) {
		draw_string(windata -> font, list, (char*)text, x, yposition, Z(35.0f), width + 1.0f, windata -> height, SINGLE, LEFT, color, winstyle -> text_scale);
		return;
	}
	char cut[256];
	float dots;
	measureLine(windata -> font, "...", winstyle -> text_scale, &dots);
	size_t length = strlen(text);
	if (length > sizeof(cut) - 4) {
		length = sizeof(cut) - 4;
	}
	memcpy(cut, text, length);
	cut[length] = '\0';
	// (drop characters from the end until what's left and the dots fit)
	while (length > 0) {
		cut[--length] = '\0';
		measureLine(windata -> font, cut, winstyle -> text_scale, &measured);
		if (measured + dots <= width) {
			break;
		}
	}
	while ((length > 0) && (cut[length - 1] == ' ')) {
		cut[--length] = '\0';
	}
	strcpy(cut + length, "...");
	draw_string(windata -> font, list, cut, x, yposition, Z(35.0f), width + 1.0f, windata -> height, SINGLE, LEFT, color, winstyle -> text_scale);
}

static void draw_row_text(const char* text, uint32 i, window_data_t* windata, window_style_t* winstyle, uint32 list, float yposition) {
	draw_fitted(windata, winstyle, list, text, row_left(windata, winstyle), yposition, row_width(windata, winstyle), row_color(i, windata, winstyle));
}

// an XP check box this size, at the right of the row (drawn with the opaque decorations, win_draw_row_marks)
#define CHECK_SIZE 13.0f

static void draw_row_check(const window_row_t* row, uint32 i, window_data_t* windata, window_style_t* winstyle, uint32 list, float yposition) {
	draw_fitted(windata, winstyle, list, row -> label, row_left(windata, winstyle), yposition,
		row_width(windata, winstyle) - CHECK_SIZE - 12.0f, row_color(i, windata, winstyle));
}

// the value at the right, the label in the rest of the row (cut short if it's too long)
static void draw_row_value(const window_row_t* row, uint32 i, window_data_t* windata, window_style_t* winstyle, uint32 list, float yposition) {
	float width = row_width(windata, winstyle);
	float x = row_left(windata, winstyle);
	uint32 color = row_color(i, windata, winstyle);
	float value_width = 0.0f;
	if (row -> value != NULL) {
		measureLine(windata -> font, (char*)row -> value, winstyle -> text_scale, &value_width);
		draw_string(windata -> font, list, (char*)row -> value, x + width - value_width, yposition, Z(35.0f), value_width + 1.0f, windata -> height, SINGLE, LEFT, color, winstyle -> text_scale);
		value_width += 16.0f;
	}
	if (row -> label != NULL) {
		draw_fitted(windata, winstyle, list, row -> label, x, yposition, width - value_width, color);
	}
}

// a value left / right change: as ROW_VALUE, between a left and a right arrow (triangles, drawn with the row marks:
// draw_choice_arrows); choice_layout gives the arrows' left edges
#define CHOICE_ARROW_W 8.0f
#define CHOICE_GAP 7.0f

static float choice_layout(const window_row_t* row, window_data_t* windata, window_style_t* winstyle, float* left_arrow, float* right_arrow) {
	float width = row_width(windata, winstyle);
	float x = row_left(windata, winstyle);
	float value_width = 0.0f;
	if (row -> value != NULL) {
		measureLine(windata -> font, (char*)row -> value, winstyle -> text_scale, &value_width);
	}
	*right_arrow = x + width - CHOICE_ARROW_W;
	float value_x = *right_arrow - CHOICE_GAP - value_width;
	*left_arrow = value_x - CHOICE_GAP - CHOICE_ARROW_W;
	return value_x;
}

static void draw_row_choice(const window_row_t* row, uint32 i, window_data_t* windata, window_style_t* winstyle, uint32 list, float yposition) {
	float x = row_left(windata, winstyle);
	uint32 color = row_color(i, windata, winstyle);
	float left, right;
	float value_x = choice_layout(row, windata, winstyle, &left, &right);
	if (row -> value != NULL) {
		draw_string(windata -> font, list, (char*)row -> value, value_x, yposition, Z(35.0f), right - value_x + 1.0f, windata -> height, SINGLE, LEFT, color, winstyle -> text_scale);
	}
	if (row -> label != NULL) {
		draw_fitted(windata, winstyle, list, row -> label, x, yposition, left - x - 12.0f, color);
	}
}

// the label in the left quarter (if there is one), the slider in the rest
static void draw_row_slider(const window_row_t* row, uint32 i, window_data_t* windata, window_style_t* winstyle, uint32 list, float yposition) {
	float x = windata -> x + winstyle -> border_thickness + winstyle -> left_margin + winstyle -> rows_indent;
	float width = windata -> width - 30.0f - (winstyle -> left_margin * 2) - winstyle -> rows_indent;
	if (row -> label != NULL) {
		float labelWidth = width * 0.25f;
		draw_string(windata -> font, list, (char*)row -> label, row_left(windata, winstyle), yposition, Z(35.0f), labelWidth, windata -> height, SINGLE, LEFT, row_color(i, windata, winstyle), winstyle -> text_scale);
		x += labelWidth;
		width -= labelWidth;
	}
	draw_scrollbar(list, x, yposition, Z(35.0f), width, windata -> font -> fontHeight * winstyle -> text_scale, 0xFF000000, row_color(i, windata, winstyle), row -> min, row -> max, row -> number);
}

// a flat coloured rectangle (x1, y1)-(x2, y2) and triangle, in the list's current (untextured) context
static void draw_flat_quad(uint32 list, float x1, float y1, float x2, float y2, float z, uint32 color) {
	pvr_vertex_t v;
	(void)list;
	v.flags = PVR_CMD_VERTEX;
	v.z = z;
	v.argb = color;
	v.oargb = 0;
	v.u = v.v = 0.0f;
	v.x = x1; v.y = y2; pvr_prim(&v, sizeof(v));
	v.x = x1; v.y = y1; pvr_prim(&v, sizeof(v));
	v.x = x2; v.y = y2; pvr_prim(&v, sizeof(v));
	v.flags = PVR_CMD_VERTEX_EOL;
	v.x = x2; v.y = y1; pvr_prim(&v, sizeof(v));
}

static void draw_flat_triangle(uint32 list, float x1, float y1, float x2, float y2, float x3, float y3, float z, uint32 color) {
	pvr_vertex_t v;
	(void)list;
	v.flags = PVR_CMD_VERTEX;
	v.z = z;
	v.argb = color;
	v.oargb = 0;
	v.u = v.v = 0.0f;
	v.x = x1; v.y = y1; pvr_prim(&v, sizeof(v));
	v.x = x2; v.y = y2; pvr_prim(&v, sizeof(v));
	v.flags = PVR_CMD_VERTEX_EOL;
	v.x = x3; v.y = y3; pvr_prim(&v, sizeof(v));
}

/*
 * The scroll bar, after Windows XP's: a pale track between two square buttons, the buttons with an arrow while the
 * list can scroll that way, and a two-tone thumb whose size and place are the view's within the list. Untextured,
 * gouraud shaded (the left-to-right shading), and drawn without culling (the arrows' triangles wind both ways).
 */

// a rectangle shaded from 'left' to 'right' across
static void draw_shaded_quad(float x1, float y1, float x2, float y2, float z, uint32 left, uint32 right) {
	pvr_vertex_t v;
	v.flags = PVR_CMD_VERTEX;
	v.z = z;
	v.oargb = 0;
	v.u = v.v = 0.0f;
	v.argb = left; v.x = x1; v.y = y2; pvr_prim(&v, sizeof(v));
	v.argb = left; v.x = x1; v.y = y1; pvr_prim(&v, sizeof(v));
	v.argb = right; v.x = x2; v.y = y2; pvr_prim(&v, sizeof(v));
	v.flags = PVR_CMD_VERTEX_EOL;
	v.argb = right; v.x = x2; v.y = y1; pvr_prim(&v, sizeof(v));
}

// a 1 px frame in 'edge' around a box shaded 'left' to 'right'
static void draw_framed_box(float x1, float y1, float x2, float y2, float z, uint32 edge, uint32 left, uint32 right) {
	draw_shaded_quad(x1, y1, x2, y2, z, edge, edge);
	draw_shaded_quad(x1 + 1.0f, y1 + 1.0f, x2 - 1.0f, y2 - 1.0f, z + 0.1f, left, right);
}

/*
 * A scroll bar from top to bottom at x1 (SCROLL_WIDTH wide), in the opaque list: its track, the up and down buttons
 * (each with its arrow while the view can move that way) and the thumb, 'view' of 'total' long, at 'position' (0 to
 * total - view). The lists' and the Help and About pages' (pages.c)
 */
void win_draw_scroll_bar(uint32 list, float x1, float top, float bottom, float view, float total, float position) {
	pvr_poly_hdr_t hdr;
	pvr_poly_cxt_t cxt;
	pvr_poly_cxt_col(&cxt, list);
	cxt.gen.culling = PVR_CULLING_NONE;
	pvr_poly_compile(&hdr, &cxt);
	pvr_prim(&hdr, sizeof(hdr));

	float x2 = x1 + SCROLL_WIDTH;
	float height = bottom - top;
	float button = SCROLL_WIDTH;
	float last = total - view;

	// the track
	draw_framed_box(x1, top, x2, bottom, Z(32.0f), 0xFFB8C4DC, 0xFFF4F6FC, 0xFFE4E8F4);

	// the buttons, and their arrows while the view can scroll that way
	int can_up = position > 0.0f;
	int can_down = position < last;
	draw_framed_box(x1, top, x2, top + button, Z(32.4f), 0xFF6C8CC8, 0xFFD8E4FC, 0xFFB0C4F0);
	draw_framed_box(x1, bottom - button, x2, bottom, Z(32.4f), 0xFF6C8CC8, 0xFFD8E4FC, 0xFFB0C4F0);
	float cx = (x1 + x2) / 2;
	if (can_up) {
		draw_flat_triangle(list, cx, top + 4.0f, x1 + 4.0f, top + button - 5.0f, x2 - 4.0f, top + button - 5.0f, Z(32.8f), 0xFF203868);
	}
	if (can_down) {
		draw_flat_triangle(list, x1 + 4.0f, bottom - button + 5.0f, x2 - 4.0f, bottom - button + 5.0f, cx, bottom - 4.0f, Z(32.8f), 0xFF203868);
	}

	// the thumb: the view's share of the whole, where the view is (at least a button high)
	float track_top = top + button + 1.0f, track_height = height - 2 * button - 2.0f;
	float thumb = (total > 0.0f) ? track_height * view / total : track_height;
	if (thumb < button) {
		thumb = button;
	}
	if (thumb > track_height) {
		thumb = track_height;
	}
	float thumb_top = track_top + (track_height - thumb) * ((last > 0.0f) ? position / last : 0.0f);
	draw_framed_box(x1 + 1.0f, thumb_top, x2 - 1.0f, thumb_top + thumb, Z(32.6f), 0xFF4C6CB4, 0xFFC4D4FC, 0xFF8CA4E4);
}

static void draw_scroll_bar(window_data_t* windata, window_style_t* winstyle, Font* header_font, uint32 list) {
	float top = windata -> y + 10.0f + (header_font -> fontHeight * winstyle -> header_text_scale);
	float height = winstyle -> max_items * ((windata -> font -> fontHeight * winstyle -> text_scale) + winstyle -> line_spacing);
	float x1 = windata -> x + windata -> width - winstyle -> border_thickness - SCROLL_WIDTH - 2.0f;
	win_draw_scroll_bar(list, x1, top, top + height, (float)winstyle -> max_items, (float)windata -> num_strings,
		(float)windata -> top_index);
}

static Font* font_for_keys;

/*
 * The key line: each button's icon (in the opaque list) and its letter and label (in the translucent list), left to
 * right from (x, y); both passes walk the same layout
 */
#define KEY_SIZE 20.0f
#define KEY_TEXT_SCALE 0.5f
// the line under the key line, this far down the window (the keys take y + 9 to y + 29, the text starts at y + 36)
#define KEY_LINE_HEIGHT 32.0f
#define KEY_GAP 6.0f
#define KEY_SPACING 22.0f

static void draw_circle(float cx, float cy, float r, float z, uint32 color) {
	pvr_vertex_t v;
	v.z = z;
	v.argb = color;
	v.oargb = 0;
	v.u = v.v = 0.0f;
	const int segments = 16;
	for (int i = 0; i < segments; i++) {
		float a1 = (2.0f * F_PI * i) / segments, a2 = (2.0f * F_PI * (i + 1)) / segments;
		v.flags = PVR_CMD_VERTEX;
		v.x = cx; v.y = cy; pvr_prim(&v, sizeof(v));
		v.x = cx + r * fcos(a1); v.y = cy + r * fsin(a1); pvr_prim(&v, sizeof(v));
		v.flags = PVR_CMD_VERTEX_EOL;
		v.x = cx + r * fcos(a2); v.y = cy + r * fsin(a2); pvr_prim(&v, sizeof(v));
	}
}

static float key_icon_width(int button) {
	switch (button) {
		case KEY_START: return 2.0f * KEY_SIZE;
		case KEY_LT:
		case KEY_RT: return 1.4f * KEY_SIZE;
		default: return KEY_SIZE;
	}
}

static void draw_key_icon(int button, float x, float y, uint32 list) {
	static const uint32 face_colors[4] = { 0xFFD82828, 0xFF2858D8, 0xFFE8C020, 0xFF28A048 };
	static const char* const face_letters[4] = { "A", "B", "X", "Y" };
	float w = key_icon_width(button), h = KEY_SIZE, cx = x + w / 2, cy = y + h / 2;
	if (list == PVR_LIST_OP_POLY) {
		switch (button) {
			case KEY_A: case KEY_B: case KEY_X: case KEY_Y:
				draw_circle(cx, cy, h / 2, Z(34.0f), face_colors[button]);
				break;
			case KEY_DPAD:
				// a cross
				draw_flat_quad(list, x + h * 0.35f, y, x + h * 0.65f, y + h, Z(34.0f), 0xFF404040);
				draw_flat_quad(list, x, y + h * 0.35f, x + h, y + h * 0.65f, Z(34.0f), 0xFF404040);
				break;
			default:
				// Start and the triggers: a grey bar with their name on it
				draw_flat_quad(list, x, y + h * 0.15f, x + w, y + h * 0.85f, Z(34.0f), 0xFF404040);
				break;
		}
		return;
	}
	const char* letter = NULL;
	float scale = KEY_TEXT_SCALE;
	switch (button) {
		case KEY_A: case KEY_B: case KEY_X: case KEY_Y: letter = face_letters[button]; break;
		case KEY_START: letter = "START"; scale = 0.4f; break;
		case KEY_LT: letter = "LT"; break;
		case KEY_RT: letter = "RT"; break;
		default: break;
	}
	if (letter != NULL) {
		float width;
		measureLine(font_for_keys, (char*)letter, scale, &width);
		float height = font_for_keys -> fontHeight * scale;
		uint32 color = (button == KEY_X) ? 0xFF000000 : 0xFFFFFFFF;
		draw_string(font_for_keys, list, (char*)letter, cx - width / 2, cy - height / 2, Z(36.0f), w, h, SINGLE, LEFT, color, scale);
	}
}

/*
 * The rows' marks, in the opaque list under their text: the focused row's bar and the arrow beside it (both pulse
 * gently), and the check boxes, after Windows XP's (a dark blue frame, a pale box, a green tick)
 */
#define FOCUS_PERIOD_MS 1600

// a mix of two colours, 'amount' (0-1) of the way from a to b
static uint32 mix_color(uint32 a, uint32 b, float amount) {
	uint32 result = 0xFF000000;
	for (int shift = 0; shift < 24; shift += 8) {
		float ca = (float)((a >> shift) & 0xff), cb = (float)((b >> shift) & 0xff);
		result |= ((uint32)(ca + (cb - ca) * amount) & 0xff) << shift;
	}
	return result;
}

// a stroke from (x1, y1) to (x2, y2), 'width' across
static void draw_stroke(float x1, float y1, float x2, float y2, float width, float z, uint32 color) {
	float dx = x2 - x1, dy = y2 - y1;
	float length = fsqrt(dx * dx + dy * dy);
	if (length <= 0.0f) {
		return;
	}
	float nx = -dy / length * width / 2, ny = dx / length * width / 2;
	pvr_vertex_t v;
	v.flags = PVR_CMD_VERTEX;
	v.z = z;
	v.argb = color;
	v.oargb = 0;
	v.u = v.v = 0.0f;
	v.x = x1 + nx; v.y = y1 + ny; pvr_prim(&v, sizeof(v));
	v.x = x1 - nx; v.y = y1 - ny; pvr_prim(&v, sizeof(v));
	v.x = x2 + nx; v.y = y2 + ny; pvr_prim(&v, sizeof(v));
	v.flags = PVR_CMD_VERTEX_EOL;
	v.x = x2 - nx; v.y = y2 - ny; pvr_prim(&v, sizeof(v));
}

static void draw_check_box(float x, float y, int checked) {
	draw_framed_box(x, y, x + CHECK_SIZE, y + CHECK_SIZE, Z(32.0f), 0xFF1C5180, 0xFFDCDCD7, 0xFFF8F8F4);
	if (checked) {
		float s = CHECK_SIZE / 13.0f;
		draw_stroke(x + 3.0f * s, y + 6.5f * s, x + 5.5f * s, y + 9.0f * s, 2.2f * s, Z(32.4f), 0xFF21A121);
		draw_stroke(x + 5.0f * s, y + 9.0f * s, x + 10.0f * s, y + 3.5f * s, 2.2f * s, Z(32.4f), 0xFF21A121);
	}
}

/*
 * The focus marker: a square pyramid on its side, its tip pointing right, spinning about its axis; each side shaded
 * between two greens by how squarely it faces the screen
 */
#define PYRAMID_PERIOD_MS 1800
#define PYRAMID_DARK 0xFF0E5A1C
#define PYRAMID_LIGHT 0xFF6CF07A

static void draw_focus_pyramid(float x, float cy, float line) {
	float length = line * 0.62f, radius = line * 0.34f;
	float angle = 2.0f * F_PI * (float)(timer_ms_gettime64() % PYRAMID_PERIOD_MS) / PYRAMID_PERIOD_MS;
	// the tip, and the base's corners in the y-z plane (y up, z towards the viewer)
	float tip[3] = { length, 0.0f, 0.0f };
	float base[4][3];
	for (int k = 0; k < 4; k++) {
		float a = angle + k * F_PI / 2;
		base[k][0] = 0.0f;
		base[k][1] = radius * fcos(a);
		base[k][2] = radius * fsin(a);
	}
	// the sides facing the viewer, back to front
	int order[4];
	float depth[4];
	int faces = 0;
	for (int k = 0; k < 4; k++) {
		const float* b0 = base[k];
		const float* b1 = base[(k + 1) & 3];
		float u[3] = { b0[0] - tip[0], b0[1] - tip[1], b0[2] - tip[2] };
		float v[3] = { b1[0] - tip[0], b1[1] - tip[1], b1[2] - tip[2] };
		float n[3] = { u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0] };
		// (outward: away from the axis)
		float mid_y = (b0[1] + b1[1]) / 2, mid_z = (b0[2] + b1[2]) / 2;
		if (n[1] * mid_y + n[2] * mid_z < 0.0f) {
			n[0] = -n[0]; n[1] = -n[1]; n[2] = -n[2];
		}
		if (n[2] <= 0.0f) {
			continue;
		}
		order[faces] = k;
		depth[faces++] = mid_z;
	}
	for (int i = 1; i < faces; i++) {
		for (int j = i; (j > 0) && (depth[j] < depth[j - 1]); j--) {
			float d = depth[j]; depth[j] = depth[j - 1]; depth[j - 1] = d;
			int o = order[j]; order[j] = order[j - 1]; order[j - 1] = o;
		}
	}
	for (int i = 0; i < faces; i++) {
		int k = order[i];
		const float* b0 = base[k];
		const float* b1 = base[(k + 1) & 3];
		// (lit from the front and a little above: the side's facing, 0-1)
		float u[3] = { b0[0] - tip[0], b0[1] - tip[1], b0[2] - tip[2] };
		float v[3] = { b1[0] - tip[0], b1[1] - tip[1], b1[2] - tip[2] };
		float n[3] = { u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0] };
		float size = fsqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
		float light = (fabsf(n[2]) * 0.8f + fabsf(n[1]) * ((b0[1] + b1[1] > 0.0f) ? 0.4f : 0.0f)) / size;
		uint32 color = mix_color(PYRAMID_DARK, PYRAMID_LIGHT, (light > 1.0f) ? 1.0f : light);
		draw_flat_triangle(PVR_LIST_OP_POLY, x + tip[0], cy - tip[1], x + b0[0], cy - b0[1], x + b1[0], cy - b1[1], Z(32.0f) + 0.01f * i, color);
	}
}

/*
 * A choice row's arrows: two triangles in the scroll bar's blues, shaded from the base to a lighter tip; on the
 * highlighted row the colours swing with the focus bar's pulse (the buttons that change it, left and right)
 */
#define ARROW_DARK 0xFF203868
#define ARROW_MID 0xFF4C6CB4
#define ARROW_LIGHT 0xFF8CB4F8

static void draw_shaded_triangle(float x1, float y1, uint32 c1, float x2, float y2, uint32 c2, float x3, float y3, uint32 c3, float z) {
	pvr_vertex_t v;
	v.flags = PVR_CMD_VERTEX;
	v.z = z;
	v.oargb = 0;
	v.u = v.v = 0.0f;
	v.argb = c1; v.x = x1; v.y = y1; pvr_prim(&v, sizeof(v));
	v.argb = c2; v.x = x2; v.y = y2; pvr_prim(&v, sizeof(v));
	v.flags = PVR_CMD_VERTEX_EOL;
	v.argb = c3; v.x = x3; v.y = y3; pvr_prim(&v, sizeof(v));
}

static void draw_choice_arrows(float left, float right, float cy, int highlighted, float pulse) {
	const float h = CHOICE_ARROW_W * 0.75f;
	uint32 base = highlighted ? mix_color(ARROW_DARK, ARROW_MID, pulse) : ARROW_DARK;
	uint32 tip = highlighted ? mix_color(ARROW_MID, ARROW_LIGHT, pulse) : ARROW_MID;
	// (left: the tip at the left; right: at the right)
	draw_shaded_triangle(left, cy, tip, left + CHOICE_ARROW_W, cy - h, base, left + CHOICE_ARROW_W, cy + h, base, Z(32.2f));
	draw_shaded_triangle(right + CHOICE_ARROW_W, cy, tip, right, cy - h, base, right, cy + h, base, Z(32.2f));
}

static void draw_row_marks(window_data_t* windata, window_style_t* winstyle, Font* header_font, uint32 list) {
	pvr_poly_hdr_t hdr;
	pvr_poly_cxt_t cxt;
	pvr_poly_cxt_col(&cxt, list);
	cxt.gen.culling = PVR_CULLING_NONE;
	pvr_poly_compile(&hdr, &cxt);
	pvr_prim(&hdr, sizeof(hdr));

	float line = (windata -> font -> fontHeight * winstyle -> text_scale) + winstyle -> line_spacing;
	float y = windata -> y + 15.0f + (header_font -> fontHeight * winstyle -> header_text_scale);
	float right = row_left(windata, winstyle) + row_width(windata, winstyle);
	// (the bar and arrow take in the icon)
	float left = row_left(windata, winstyle) - windata -> icon_width;
	// (0-1 and back over the period)
	float phase = (float)(timer_ms_gettime64() % FOCUS_PERIOD_MS) / FOCUS_PERIOD_MS;
	float pulse = 0.5f - 0.5f * fcos(2.0f * F_PI * phase);
	for (int i = 0; (i < winstyle -> max_items) && (i + windata -> top_index < windata -> num_strings); i++, y += line) {
		int index = i + windata -> top_index;
		if (index == windata -> highlighted_index) {
			uint32 bar = mix_color(winstyle -> inside_color, winstyle -> selected_text_color, 0.14f + 0.08f * pulse);
			draw_cornered_layer(list, left - 4.0f, y - 1.0f, Z(31.5f), right - left + 8.0f, line + 2.0f, 4.0f, bar);
			// (draw_cornered_layer compiles its own header: the arrow and boxes want ours back)
			pvr_prim(&hdr, sizeof(hdr));
			draw_focus_pyramid(left - 15.0f, y + line / 2, line);
		}
		if ((windata -> rows != NULL) && (windata -> rows[index].kind == ROW_CHOICE)) {
			float left_arrow, right_arrow;
			choice_layout(&windata -> rows[index], windata, winstyle, &left_arrow, &right_arrow);
			draw_choice_arrows(left_arrow, right_arrow, y + line / 2, index == windata -> highlighted_index, pulse);
		}
		if ((windata -> rows != NULL) && (windata -> rows[index].kind == ROW_CHECK)) {
			draw_check_box(right - CHECK_SIZE, y + (line - CHECK_SIZE) / 2, windata -> rows[index].number != 0);
		}
	}
}

void win_draw_keys(Font* font, const window_key_t* keys, int count, float x, float y, uint32 text_color, uint32 list) {
	font_for_keys = font;
	for (int i = 0; i < count; i++) {
		const window_key_t* key = &keys[i];
		draw_key_icon(key -> button, x, y, list);
		x += key_icon_width(key -> button);
		if (key -> label == NULL) {
			x += 4.0f;
			continue;
		}
		x += KEY_GAP;
		float width;
		measureLine(font, (char*)key -> label, KEY_TEXT_SCALE, &width);
		if (list == PVR_LIST_TR_POLY) {
			float height = font -> fontHeight * KEY_TEXT_SCALE;
			draw_string(font, list, (char*)key -> label, x, y + (KEY_SIZE - height) / 2, Z(35.0f), width + 4.0f, KEY_SIZE, SINGLE, LEFT, text_color, KEY_TEXT_SCALE);
		}
		x += width + KEY_SPACING;
	}
}

static void draw_keys(window_data_t* windata, window_style_t* winstyle, uint32 list) {
	win_draw_keys(windata -> font, windata -> keys, windata -> num_keys, windata -> x + winstyle -> left_margin, windata -> y + 9.0f,
		winstyle -> text_color, list);
}

void win_draw_textwindow(window_data_t* windata, window_style_t* winstyle, uint32 list) {
	Font* header_font = (windata -> header_font != NULL) ? windata -> header_font : windata -> font;
	int i;
	// (whole pixels, as the rows have always been placed)
	int yposition;

	if (list == PVR_LIST_OP_POLY) {
		// the border, and the inside over it
		draw_cornered_layer(list, windata -> x, windata -> y, Z(30.0f), windata -> width, windata -> height, CORNER_RADIUS,
			winstyle -> border_color);
		draw_cornered_layer(list, windata -> x + winstyle -> border_thickness, windata -> y + winstyle -> border_thickness, Z(31.0f),
			windata -> width - (2 * winstyle -> border_thickness), windata -> height - (2 * winstyle -> border_thickness),
			CORNER_RADIUS - winstyle -> border_thickness, winstyle -> inside_color);

		// (the key line's icons, still in the layers' flat colour context, and the line under it, across the window
		// between its borders)
		if (windata -> num_keys > 0) {
			draw_keys(windata, winstyle, list);
			float line_y = windata -> y + KEY_LINE_HEIGHT;
			draw_flat_quad(list, windata -> x + winstyle -> border_thickness, line_y,
				windata -> x + windata -> width - winstyle -> border_thickness, line_y + 2.0f, Z(32.0f), winstyle -> border_color);
		}

		draw_row_marks(windata, winstyle, header_font, list);

		// the scroll bar down the right side, when the list is longer than the window
		if (windata -> num_strings > winstyle -> max_items) {
			draw_scroll_bar(windata, winstyle, header_font, list);
		}
	}

	if ((list == PVR_LIST_TR_POLY) && (windata -> num_keys > 0)) {
		draw_keys(windata, winstyle, list);
	}
	if (list == PVR_LIST_TR_POLY) {
		if (windata -> header_text != NULL) {
			draw_string(header_font, list, windata -> header_text, windata -> x + winstyle -> left_margin, windata -> y + 10.0f, Z(35.0f),
				windata -> width - (2 * winstyle -> left_margin), windata -> height, SINGLE, LEFT, 0xFF00FF00, winstyle -> header_text_scale);
		}

		// the rows' text
		yposition = windata -> y + 15.0 + (header_font -> fontHeight * winstyle -> header_text_scale);
		for (i = 0; (i < winstyle -> max_items) && (i < windata -> num_strings); i++) {
			if (windata -> rows != NULL) {
				const window_row_t* row = &windata -> rows[i + windata -> top_index];
				switch (row -> kind) {
					case ROW_CHECK:
						draw_row_check(row, i, windata, winstyle, list, yposition);
						break;
					case ROW_VALUE:
						draw_row_value(row, i, windata, winstyle, list, yposition);
						break;
					case ROW_CHOICE:
						draw_row_choice(row, i, windata, winstyle, list, yposition);
						break;
					case ROW_SLIDER:
						draw_row_slider(row, i, windata, winstyle, list, yposition);
						break;
					default:
						if (row -> label != NULL) {
							draw_row_text(row -> label, i, windata, winstyle, list, yposition);
						}
						break;
				}
			} else if (windata -> data_strings[i + windata -> top_index] != NULL) {
				draw_row_text(windata -> data_strings[i + windata -> top_index], i, windata, winstyle, list, yposition);
			}
			if (windata -> row_icon != NULL) {
				float size = windata -> font -> fontHeight * winstyle -> text_scale;
				windata -> row_icon(i + windata -> top_index, row_left(windata, winstyle) - windata -> icon_width, yposition, size);
			}
			yposition += (windata -> font -> fontHeight * winstyle -> text_scale) + winstyle -> line_spacing;
		}
	}
}
