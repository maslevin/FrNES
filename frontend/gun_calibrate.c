/*
 * The light gun's calibration (the Controls page's light gun rows): a white screen with a target in each corner in
 * turn, shot with the gun. The gun latches the PVR's H and V counters where it sees the beam (KOS's gun mode, every
 * other frame: the frames between answer its buttons), so each shot gives the counters for a known screen point, and
 * the four give the scale and offset the emulator maps the counters with (mapper_boards.c's zapper_gun_input): screen
 * x = (H - bitmapx) * scale + offset, H past the line's end read from 0 (a count before bitmapx is the next line's),
 * and y the same from V - bitmapy, doubled when interlaced. B (on the gun or a controller) gives up.
 */
#include <kos.h>
#include <stdio.h>
#include "gun_calibrate.h"
#include "frontend.h"
#include "options.h"

// the targets' middles: top left, top right, bottom right, bottom left
#define TARGET_LEFT 64.0f
#define TARGET_RIGHT 576.0f
#define TARGET_TOP 64.0f
#define TARGET_BOTTOM 416.0f
static const float targets[4][2] = {
	{ TARGET_LEFT, TARGET_TOP }, { TARGET_RIGHT, TARGET_TOP }, { TARGET_RIGHT, TARGET_BOTTOM }, { TARGET_LEFT, TARGET_BOTTOM }
};
// frames after the trigger is pulled before the counters are taken (two gun mode frames)
#define SHOT_FRAMES 4
// frames a gun that's stopped answering is waited for (5 seconds)
#define GONE_FRAMES 300

static void quad(float x1, float y1, float x2, float y2, float z, uint32 color) {
	pvr_vertex_t v;
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

// a frame: the white screen, the target (rings and a cross), the line of text
static void draw(int target, const char* text) {
	pvr_poly_hdr_t header;
	pvr_poly_cxt_t context;
	pvr_wait_ready();
	pvr_scene_begin();
	pvr_list_begin(PVR_LIST_OP_POLY);
	pvr_poly_cxt_col(&context, PVR_LIST_OP_POLY);
	pvr_poly_compile(&header, &context);
	pvr_prim(&header, sizeof(header));
	quad(0.0f, 0.0f, 640.0f, 480.0f, 1.0f, 0xFFFFFFFF);
	if (target < 4) {
		float x = targets[target][0], y = targets[target][1];
		quad(x - 24.0f, y - 24.0f, x + 24.0f, y + 24.0f, 2.0f, 0xFFD02020);
		quad(x - 18.0f, y - 18.0f, x + 18.0f, y + 18.0f, 3.0f, 0xFFFFFFFF);
		quad(x - 12.0f, y - 12.0f, x + 12.0f, y + 12.0f, 4.0f, 0xFFD02020);
		quad(x - 6.0f, y - 6.0f, x + 6.0f, y + 6.0f, 5.0f, 0xFFFFFFFF);
		quad(x - 30.0f, y - 1.0f, x + 30.0f, y + 1.0f, 6.0f, 0xFF000000);
		quad(x - 1.0f, y - 30.0f, x + 1.0f, y + 30.0f, 6.0f, 0xFF000000);
	}
	pvr_list_finish();
	pvr_list_begin(PVR_LIST_TR_POLY);
	float width;
	measureLine(font_mono, (char*)text, 0.6f, &width);
	draw_string(font_mono, PVR_LIST_TR_POLY, (char*)text, (640.0f - width) / 2, 220.0f, 10.0f, width + 2.0f, 30.0f, SINGLE, LEFT,
		0xFF000000, 0.6f);
	const char* cancel = "B cancels";
	measureLine(font_mono, (char*)cancel, 0.5f, &width);
	draw_string(font_mono, PVR_LIST_TR_POLY, (char*)cancel, (640.0f - width) / 2, 252.0f, 10.0f, width + 2.0f, 30.0f, SINGLE, LEFT,
		0xFF606060, 0.5f);
	pvr_list_finish();
	pvr_scene_finish();
}

// B on the gun or any controller
static int cancelled(void) {
	for (int port = 0; port < 4; port++) {
		maple_device_t* device = maple_enum_dev(port, 0);
		cont_state_t* state = (device && (device -> info.functions & MAPLE_FUNC_CONTROLLER)) ? (cont_state_t*)maple_dev_status(device) : NULL;
		if (state && (state -> buttons & CONT_B)) {
			return 1;
		}
	}
	return 0;
}

int gun_calibrate(int port, char* result, size_t size) {
	int h[4], v[4];
	uint32 frame = 0;
	int target = 0, wait = 0, held = 1, gone = 0;
	char text[48];
	int last_x = maple_state.gun_x, last_y = maple_state.gun_y;
	uint32 changes = 0, frames_since = 0;
	while (target < 4) {
		// (once a display frame, as the emulator's loop: KOS's maple sends the gun mode command with the next vblank's
		// polls, and reads the counters when they're answered)
		vid_waitvbl();
		if ((maple_state.gun_x != last_x) || (maple_state.gun_y != last_y)) {
			last_x = maple_state.gun_x;
			last_y = maple_state.gun_y;
			changes++;
		}
		if (++frames_since == 60) {
			printf("gun_calibrate: H %d V %d, %lu changes in the last second, gun port %d\n", last_x, last_y,
				(unsigned long)changes, maple_state.gun_port);
			frames_since = changes = 0;
		}
		// (a gun that drops out, as one far from the TV can for a moment, is waited for: GONE_FRAMES before giving up)
		maple_device_t* gun = maple_enum_dev(port, 0);
		if ((gun == NULL) || !gun -> valid || !(gun -> info.functions & MAPLE_FUNC_LIGHTGUN)) {
			if (++gone >= GONE_FRAMES) {
				snprintf(result, size, "The light gun stopped answering");
				return -1;
			}
			if (cancelled()) {
				snprintf(result, size, "Calibration cancelled");
				return -1;
			}
			wait = 0;
			held = 1;
			draw(target, "Light gun not answering...");
			continue;
		}
		gone = 0;
		if (cancelled()) {
			snprintf(result, size, "Calibration cancelled");
			return -1;
		}
		// (gun mode every other frame, as the emulator does)
		if (++frame & 1) {
			maple_state.gun_port = port;
		}
		cont_state_t* state = (cont_state_t*)maple_dev_status(gun);
		int pulled = state && (state -> buttons & CONT_A);
		if (wait > 0) {
			if (--wait == 0) {
				int gx = maple_state.gun_x, gy = maple_state.gun_y;
				if ((gx > 0) && (gy > 0)) {
					h[target] = (gx < (int)vid_mode -> bitmapx) ? (gx + vid_mode -> clocks + 1) : gx;
					v[target] = gy;
					printf("gun_calibrate: target %d (%d, %d): H %d V %d\n", target + 1, (int)targets[target][0],
						(int)targets[target][1], gx, gy);
					target++;
				}
			}
		} else if (pulled && !held) {
			wait = SHOT_FRAMES;
		}
		held = pulled;
		snprintf(text, sizeof(text), "Shoot the target (%d of 4)", (target < 4) ? target + 1 : 4);
		draw(target, text);
	}
	// (the left and right targets' H, the top and bottom ones' V, each pair averaged)
	float h_left = (h[0] + h[3]) / 2.0f, h_right = (h[1] + h[2]) / 2.0f;
	float v_top = (v[0] + v[1]) / 2.0f, v_bottom = (v[2] + v[3]) / 2.0f;
	float lines = (vid_mode -> flags & VID_INTERLACE) ? 2.0f : 1.0f;
	if ((h_right - h_left < 100.0f) || (v_bottom - v_top < 50.0f / lines)) {
		snprintf(result, size, "Those shots didn't line up: try again");
		return -1;
	}
	float x_scale = (TARGET_RIGHT - TARGET_LEFT) / (h_right - h_left);
	float x_offset = TARGET_LEFT - (h_left - vid_mode -> bitmapx) * x_scale;
	float y_scale = (TARGET_BOTTOM - TARGET_TOP) / ((v_bottom - v_top) * lines);
	float y_offset = TARGET_TOP - (v_top - vid_mode -> bitmapy) * lines * y_scale;
	options_set_gun_calibration((int16)(x_scale * 1000.0f + 0.5f), (int16)(x_offset + ((x_offset < 0) ? -0.5f : 0.5f)),
		(int16)(y_scale * 1000.0f + 0.5f), (int16)(y_offset + ((y_offset < 0) ? -0.5f : 0.5f)));
	printf("gun_calibrate: x scale %.3f offset %.1f, y scale %.3f offset %.1f\n", (double)x_scale, (double)x_offset,
		(double)y_scale, (double)y_offset);
	snprintf(result, size, "Light gun calibrated");
	return 0;
}
