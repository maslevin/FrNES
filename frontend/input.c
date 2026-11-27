/*===================================================================*/
/*                                                                   */
/*  input.c : The menus' controller input                            */
/*                                                                   */
/*===================================================================*/

#include "input.h"

#define INPUT_REPEAT_DELAY 20
#define INPUT_REPEAT_RATE 4
// the triggers count as pressed past this (0-255), released below the lower value
#define TRIGGER_PRESS 200
#define TRIGGER_RELEASE 128
#define EXIT_BUTTONS (CONT_A | CONT_B | CONT_X | CONT_Y | CONT_START)

static uint32 previous = 0;
static uint32 held_frames = 0;
// held when input_wait_release was called: ignored until released
static uint32 ignored = 0;
static uint32 trigger_state = 0;

// every controller's buttons as INPUT_* bits
static uint32 input_state(uint32* exit) {
	uint32 state = 0;
	*exit = 0;
	for (int port = 0; port < 4; port++) {
		maple_device_t* controller = maple_enum_type(port, MAPLE_FUNC_CONTROLLER);
		if (controller == NULL) {
			continue;
		}
		cont_state_t* cont = (cont_state_t*)maple_dev_status(controller);
		if (cont == NULL) {
			continue;
		}
		uint32 buttons = cont -> buttons;
		if ((buttons & EXIT_BUTTONS) == EXIT_BUTTONS) {
			*exit = 1;
		}
		state |= ((buttons & CONT_DPAD_UP) ? INPUT_UP : 0) |
			((buttons & CONT_DPAD_DOWN) ? INPUT_DOWN : 0) |
			((buttons & CONT_DPAD_LEFT) ? INPUT_LEFT : 0) |
			((buttons & CONT_DPAD_RIGHT) ? INPUT_RIGHT : 0) |
			((buttons & CONT_A) ? INPUT_ACCEPT : 0) |
			((buttons & CONT_B) ? INPUT_BACK : 0) |
			((buttons & CONT_X) ? INPUT_X : 0) |
			((buttons & CONT_Y) ? INPUT_Y : 0) |
			((buttons & CONT_START) ? INPUT_START : 0) |
			((buttons & CONT_Z) ? INPUT_PAGE_UP : 0) |
			((buttons & CONT_C) ? INPUT_PAGE_DOWN : 0);
		// the triggers with a little hysteresis, per port
		uint32 left = INPUT_PAGE_UP << (port * 2), right = INPUT_PAGE_DOWN << (port * 2);
		if (cont -> ltrig > TRIGGER_PRESS) {
			trigger_state |= left;
		} else if (cont -> ltrig < TRIGGER_RELEASE) {
			trigger_state &= ~left;
		}
		if (cont -> rtrig > TRIGGER_PRESS) {
			trigger_state |= right;
		} else if (cont -> rtrig < TRIGGER_RELEASE) {
			trigger_state &= ~right;
		}
		state |= ((trigger_state & left) ? INPUT_PAGE_UP : 0) | ((trigger_state & right) ? INPUT_PAGE_DOWN : 0);
	}
	return state;
}

uint32 input_poll() {
	uint32 exit;
	uint32 state = input_state(&exit);
	ignored &= state;
	state &= ~ignored;
	if (exit && !ignored) {
		return INPUT_EXIT;
	}
	uint32 pressed = state & ~previous;
	const uint32 repeating = INPUT_UP | INPUT_DOWN | INPUT_LEFT | INPUT_RIGHT | INPUT_PAGE_UP | INPUT_PAGE_DOWN;
	if (state == previous) {
		held_frames++;
		if ((held_frames >= INPUT_REPEAT_DELAY) && (((held_frames - INPUT_REPEAT_DELAY) % INPUT_REPEAT_RATE) == 0)) {
			pressed |= state & repeating;
		}
	} else {
		held_frames = 0;
	}
	previous = state;
	return pressed;
}

void input_wait_release() {
	uint32 exit;
	ignored = input_state(&exit);
	previous = 0;
	held_frames = 0;
}
