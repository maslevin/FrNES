/*===================================================================*/
/*                                                                   */
/*  input.h : The menus' controller input                            */
/*                                                                   */
/*===================================================================*/

#ifndef __INPUT_H
#define __INPUT_H

#include <kos.h>

// what the menus act on, once per press (held directions and page keys repeat)
#define INPUT_UP        0x0001
#define INPUT_DOWN      0x0002
#define INPUT_LEFT      0x0004
#define INPUT_RIGHT     0x0008
#define INPUT_ACCEPT    0x0010  // A
#define INPUT_BACK      0x0020  // B
#define INPUT_X         0x0040
#define INPUT_Y         0x0080
#define INPUT_START     0x0100
#define INPUT_PAGE_UP   0x0200  // left trigger, or Z on the arcade stick
#define INPUT_PAGE_DOWN 0x0400  // right trigger, or C
#define INPUT_EXIT      0x0800  // A+B+X+Y+Start held: leave FrNES (as in a game, where it ends the game)

/**
 * Reads every controller (any of them drives the menus) once per menu frame and returns the INPUT_* events: a button
 * counts when it goes down, and directions and page keys repeat while held (after INPUT_REPEAT_DELAY frames, then every
 * INPUT_REPEAT_RATE)
 */
uint32 input_poll();

/**
 * Ignores everything held now until it's released (after a game, whose exit buttons are still down)
 */
void input_wait_release();

#endif
