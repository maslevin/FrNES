#include <kos.h>

#include "frnes.h"
#include "memory.h"
#include "video.h"

/*
 * The frontend's settings for the emulator (frnes.h). Kept out of the hot data sources (tools/hot_data.py): they are
 * read once a game, or once a frame for the picture's corners, so they don't need a place in .hotdata and mustn't
 * move what is there.
 */
// (last in .bss with the cold state, FRNES_COLD_BSS: it grew with FRNES_PADS 4, and where it was its size moved the
// .bss after it onto other operand cache sets)
frnes_settings_t frnes_settings FRNES_COLD_BSS;
#ifndef FRNES_HOST
// (its old place, as large as it was with 2 pads, 0x30: what came after it in .bss stays on the same operand cache sets;
// written once below, so the link keeps it)
__attribute__((aligned(4), section(".bss.frnes_settings"))) static char frnes_settings_hole[0x30];
#endif
video_picture_t video_picture;

// player builds' mapping, as main.c had it: A = A, X or B = B, Y or the left trigger = Select
void frnes_default_settings(frnes_settings_t* settings) {
#ifndef FRNES_HOST
    ((volatile char*)frnes_settings_hole)[0] = 0;
#endif
    memset(settings, 0, sizeof(*settings));
    settings -> picture = FRNES_PICTURE_FULL;
    // (the build's starting pacing, video.h)
    settings -> pacing = (FRNES_DEFAULT_PACING == PACE_VBLANK) ? FRNES_PACING_VBLANK : FRNES_PACING_SOUND;
    settings -> hud_mode = 1;
    settings -> zapper_speed = FRNES_ZAPPER_SPEED_DEFAULT;
    for (uint32_t i = 0; i < FRNES_PADS; i++) {
        settings -> pads[i].a_buttons = CONT_A;
        settings -> pads[i].b_buttons = CONT_X | CONT_B;
        settings -> pads[i].select_buttons = CONT_Y;
        settings -> pads[i].select_on_left_trigger = 1;
    }
}

// (players that never call frnes_default_settings get the defaults too)
__attribute__((constructor)) static void frnes_settings_init() {
    frnes_default_settings(&frnes_settings);
    video_picture_update(&frnes_settings);
}

// the picture's corners on the screen and in the texture, from the picture mode and the crop
void video_picture_update(const frnes_settings_t* settings) {
    float left = 0.0f, width = 640.0f;
    switch (settings -> picture) {
        case FRNES_PICTURE_2X:
            width = 512.0f;
            break;
        case FRNES_PICTURE_8_7:
            width = 512.0f * 8.0f / 7.0f;
            break;
        default:
            break;
    }
    left = (640.0f - width) / 2.0f;
    uint32_t crop_left = (settings -> crop_left <= FRNES_CROP_MAX_X) ? settings -> crop_left : FRNES_CROP_MAX_X;
    uint32_t crop_right = (settings -> crop_right <= FRNES_CROP_MAX_X) ? settings -> crop_right : FRNES_CROP_MAX_X;
    uint32_t crop_top = (settings -> crop_top <= FRNES_CROP_MAX_Y) ? settings -> crop_top : FRNES_CROP_MAX_Y;
    uint32_t crop_bottom = (settings -> crop_bottom <= FRNES_CROP_MAX_Y) ? settings -> crop_bottom : FRNES_CROP_MAX_Y;
    float pixel_width = width / 256.0f;
    // 240 lines on 480: each NES line is 2 screen lines
    video_picture.x1 = left + (float)crop_left * pixel_width;
    video_picture.x2 = left + width - (float)crop_right * pixel_width;
    video_picture.y1 = (float)(crop_top * 2);
    video_picture.y2 = 480.0f - (float)(crop_bottom * 2);
    // the texture: 256 NES pixels across u 0-1, 256 lines down v 0-1 (240 used)
    video_picture.u1 = (float)crop_left / 256.0f;
    video_picture.u2 = (float)(256 - crop_right) / 256.0f;
    video_picture.v1 = (float)crop_top / 256.0f;
    video_picture.v2 = (float)(240 - crop_bottom) / 256.0f;
    video_picture.filter = settings -> bilinear ? PVR_FILTER_BILINEAR : PVR_FILTER_NONE;
}
