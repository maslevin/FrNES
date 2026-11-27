#ifndef __VIDEO_H
#define __VIDEO_H

#include <kos.h>
#include "frnes.h"

/**
 * FrNES 1.0.0 display path
 *
 * The picture is shown from PVR VQ textures in the FrNES layout: a 1024x256 non-twiddled RGB565 VQ texture whose
 * 2 kB codebook holds the NES palette (4 copies of the 64 colors, each entry 4 identical texels), so each NES pixel
 * is one index byte and NES scanline y is the 256 bytes at offset y * 256 of the index data.
 *
 * NUM_VIDEO_FRAMES textures form a ring: the PPU writes frame N into one texture through the SH4 store queues while
 * the PVR renders from another. The store queues belong to the PPU from video_frame_begin() to video_frame_end();
 * KOS's own store queue users (TA polygon submission, pvr_txr_load, sound uploads) only run outside that window.
 */
#define NUM_VIDEO_FRAMES 3

/**
 * Frame pacing (player builds; a setting the frontend's menu can switch at any time):
 *   PACE_VBLANK  one emulated frame per display refresh, each shown exactly once; the game runs at the display's
 *                rate (59.94 Hz NTSC; KOS's VGA timing about 60.09) and the sound is resampled to match
 *                (apu_synth_pace). The default.
 *   PACE_AUDIO   the NES's own rate, held by the sound buffer; the display shows the newest finished frame at each
 *                vblank, so about every 6 s one frame isn't shown (and frames that finish near a vblank can come a
 *                refresh late).
 * PAL video (50 Hz) always paces to the sound.
 */
#define PACE_AUDIO  0
#define PACE_VBLANK 1
// the starting value (FRNES_DEFAULT_PACING=PACE_AUDIO for builds that start sound paced; the frontend's default too)
#ifndef FRNES_DEFAULT_PACING
#define FRNES_DEFAULT_PACING PACE_VBLANK
#endif
extern int video_pacing;
// vblanks since video_init (counted by a vblank handler)
extern volatile uint32_t video_vblank_count;
// the display's refresh rate in Hz (measured from the vblanks, KOS's video mode until then)
float video_refresh_hz();
/**
 * Waits for the first vblank after count 'seen' (returns at once if it has already come), calling 'idle' about
 * every millisecond meanwhile (the sound stream's poll). Returns the vblank count.
 */
uint32_t video_wait_vblank(uint32_t seen, void (*idle)());
// starts counting vblanks (the player; test builds don't pace, so they don't take the interrupt)
void video_vblank_start();

// where the picture goes: the screen corners (x1, y1)-(x2, y2) show the texture's (u1, v1)-(u2, v2); set by
// video_picture_update from the frontend's settings (frnes_settings.c)
typedef struct {
    float x1, y1, x2, y2;
    float u1, v1, u2, v2;
    uint32_t filter;
} video_picture_t;
extern video_picture_t video_picture;
void video_picture_update(const frnes_settings_t* settings);

// set before video_init to have a translucent list for the saving indicator (player builds)
extern int video_translucent_overlay;
int video_init();
void video_shutdown();

/**
 * Locks the store queues onto the next free texture and points the PPU output at it
 */
void video_frame_begin();

/**
 * Releases the store queues and, if the PVR is ready (or wait is set), submits the finished texture for display.
 * Returns 1 if the frame was submitted, 0 if it was skipped because the PVR was still busy.
 */
int video_frame_end(int wait);

/**
 * Copies the index data of the last frame generated, presented or skipped (256x240 NES color indexes) out of VRAM,
 * for screenshots. Call between video_frame_end() and the next frame's rendering
 */
void video_read_frame(uint8_t* out);

// the last frame shown again with the overlays (the game menu while the game is paused), waiting for the PVR
void video_redraw(void);

/**
 * Breadcrumbs for hangs that mask interrupts (nothing gets printed): the player marks each phase of a frame here,
 * stored through the uncached alias so they are in RAM, not the cache, when the console stops. After the SH4 watchdog
 * resets the console (record and playback builds, main.c)
 * (dc-load-ip comes back), tools/crumbs.py reads them out with dc-tool-ip -d at VIDEO_CRUMBS_ADDRESS. Each entry keeps
 * 16 bits: [0] 0x4652 ('FR'), [1] count, [2] frame, [3] phase, [4] TMU2 counter, then the last 32 marks
 * ((phase << 12) | (frame & 0xfff)) at [8 + (count & 31)].
 */
#define CRUMB_CPU 1
#define CRUMB_DEVICES 2
#define CRUMB_SYNTH 3
#define CRUMB_FRAME_END 4
#define CRUMB_PVR_READY 5
#define CRUMB_PVR_SCENE 6
#define CRUMB_PVR_OVERLAY 7
#define CRUMB_PVR_FINISH 8
#define CRUMB_SOUND_WAIT 9
#define CRUMB_AFTER_WAIT 10
#define CRUMB_FRAME_BEGIN 12
#define CRUMB_INPUT 13
#define CRUMB_FRAME_DONE 14
// (the last 40 entries of the PVR's palette RAM, which nothing here uses (the textures are VQ) and which keep 16 bits
// each: the SH4 watchdog's reset into the BIOS and dc-load-ip zeroes or fills main RAM and video RAM, but not these)
#define VIDEO_CRUMBS_ADDRESS 0xa05f9f60
extern uint32_t video_crumb_frame;
static inline void video_crumb(uint32_t phase) {
    volatile uint32_t* c = (volatile uint32_t*)VIDEO_CRUMBS_ADDRESS;
    uint32_t n = (c[1] + 1) & 0xffff;
    c[0] = 0x4652;
    c[1] = n;
    c[2] = video_crumb_frame;
    c[3] = phase;
    c[4] = *(volatile uint32_t*)0xffd80024;
    c[8 + (n & 31)] = (phase << 12) | (video_crumb_frame & 0xfff);
}

#endif
