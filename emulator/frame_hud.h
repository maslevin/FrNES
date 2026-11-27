#ifndef FRAME_HUD_H
#define FRAME_HUD_H

#include <stdint.h>

// the player's frame display (see frame_hud.c)
#define HUD_NOT_PRESENTED 1
#define HUD_SOUND_LOW 2

// makes the glyph texture and starts PRFC1 (at start-up); 0 on success
int frame_hud_init();
// off -> on (operand cache misses) -> on (instruction cache misses) -> off
void frame_hud_cycle_mode();
// sets the mode: 0 off, 1 on (operand cache misses), 2 on (instruction cache misses)
void frame_hud_set_mode(uint32_t mode);
uint32_t frame_hud_mode(void);
// a frame's time (microseconds): emulation (CPU and devices), the APU's sound and expansion audio (apu_synth_frame),
// the rest of the frame's work (presenting it, saves) and waiting for sound
typedef struct {
    uint32_t emu_us, synth_us, expansion_us, other_us, wait_us;
} hud_times_t;
// between frames: the frame's times, flags, and its whole length
void frame_hud_record(const hud_times_t* times, uint32_t flags, uint64_t frame_us);
// around the frame's wait for sound (the cache miss share leaves it out)
void frame_hud_wait_begin();
void frame_hud_wait_end();
// the frame's emulation window, its start (frame_hud.c: frame_hud_record's state for hud_perf_begin) and its end: the
// cache loss (IC and OC, alternating), the PPU's and the expansion sound's times (TMU1 PC samples) while the display is
// on, the bar segments and text lines they show (frame_hud_perf.c, built without LTO)
void frame_hud_emu_begin(uint32_t frame);
typedef struct {
    uint32_t frame, mode, ring_head, text_frames;
    void* text_texture;
    void* bars_texture;
} hud_perf_view_t;
uint32_t hud_perf_begin(const hud_perf_view_t* view);
void hud_perf_start(void);
void hud_perf_end(void);
// a line of text at the bottom of the screen for 'frames' frames (in any mode, and without frame_hud_init)
void frame_hud_message(const char* text, uint32_t frames);
// (after video_init: the PVR started again, the message's texture is made anew)
void frame_hud_message_reset(void);
// inside video_frame_end's translucent list
void frame_hud_draw();
// a glyph's row (0-6) as 8 pixels, bit 7 the leftmost (the 5x7 glyph in columns 1-5)
uint8_t frame_hud_glyph_row(char c, uint32_t row);
// drawn first inside frame_hud_draw, in the translucent list (the NSF player's spectrum analyzer), NULL for none
extern void (*frame_hud_overlay)(void);

#endif
