#ifndef SAVE_INDICATOR_H
#define SAVE_INDICATOR_H

#include <stdint.h>

// while above 0, video_frame_end draws the saving indicator (see save_indicator.c); below 0, the result of a save state
// written in the background, which the next draw shows as a message
extern volatile int save_indicator_active;
#define SAVE_INDICATOR_STATE_RESULT(slot, saved) (-(1 + 2 * (slot) + ((saved) ? 1 : 0)))
// makes the indicator's texture (at start-up: the upload uses the store queues); 0 on success
int save_indicator_init();
// called by video_frame_end inside its translucent list
void save_indicator_draw(uint32_t frame);

#endif
