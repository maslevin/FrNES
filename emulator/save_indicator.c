#include "save_indicator.h"
#include <kos.h>
#include <string.h>
#include <stdio.h>
#include "frame_hud.h"

/**
 * The "saving, don't turn the console off" indicator: a VMU and "SAVING / DON'T POWER OFF" drawn procedurally into a
 * 256x64 ARGB4444 texture once at start-up (outside frame generation, the upload uses the store queues), and drawn by
 * video_frame_end as one textured translucent quad in the bottom right corner while save_indicator_active is set. To
 * swap in final art, replace draw_icon() / draw_text() (or load the texture from a file) and keep the rest.
 */
#define ICON_SIZE 64
#define TEXTURE_W 256
volatile int save_indicator_active = 0;
static pvr_ptr_t icon_texture = NULL;

static uint16_t argb4444(uint32_t a, uint32_t r, uint32_t g, uint32_t b) {
    return (uint16_t)((a << 12) | (r << 8) | (g << 4) | b);
}

// inside a rectangle with corners rounded by radius r
static int in_rounded(int x, int y, int x0, int y0, int x1, int y1, int r) {
    if ((x < x0) || (x > x1) || (y < y0) || (y > y1)) {
        return 0;
    }
    int cx = (x < x0 + r) ? x0 + r : ((x > x1 - r) ? x1 - r : x);
    int cy = (y < y0 + r) ? y0 + r : ((y > y1 - r) ? y1 - r : y);
    return ((x - cx) * (x - cx) + (y - cy) * (y - cy)) <= r * r;
}

static void draw_icon(uint16_t* pixels) {
    const uint16_t clear = argb4444(0, 0, 0, 0);
    const uint16_t outline = argb4444(15, 3, 3, 4);
    const uint16_t body = argb4444(15, 13, 13, 14);
    const uint16_t screen = argb4444(15, 7, 10, 7);
    const uint16_t ink = argb4444(15, 2, 4, 2);
    const uint16_t pad = argb4444(15, 5, 5, 6);
    const uint16_t button = argb4444(15, 13, 3, 3);
    for (int y = 0; y < ICON_SIZE; y++) {
        for (int x = 0; x < ICON_SIZE; x++) {
            uint16_t c = clear;
            if (in_rounded(x, y, 12, 2, 51, 61, 8)) {
                c = in_rounded(x, y, 14, 4, 49, 59, 6) ? body : outline;
            }
            // the screen, with a down arrow (saving to the card)
            if ((x >= 18) && (x <= 45) && (y >= 9) && (y <= 32)) {
                c = ((x == 18) || (x == 45) || (y == 9) || (y == 32)) ? outline : screen;
                int ax = x - 31, ay = y - 12;
                if (((ax >= -2) && (ax <= 3) && (ay >= 0) && (ay <= 9)) ||
                    ((ay >= 10) && (ay <= 16) && (ax >= -8 + (ay - 10)) && (ax <= 9 - (ay - 10)))) {
                    c = ink;
                }
            }
            // d-pad
            if (((x >= 18) && (x <= 30) && (y >= 44) && (y <= 48)) || ((x >= 22) && (x <= 26) && (y >= 40) && (y <= 52))) {
                c = pad;
            }
            // A / B buttons
            if (((x - 39) * (x - 39) + (y - 48) * (y - 48) <= 9) || ((x - 45) * (x - 45) + (y - 43) * (y - 43) <= 9)) {
                c = button;
            }
            pixels[y * TEXTURE_W + x] = c;
        }
    }
}

// the text's letters, 5x7 ('#' set)
static const char glyph_chars[] = "SAVINGDOTPWERF'";
static const char* const glyph_rows[][7] = {
    { " ####", "#    ", "#    ", " ### ", "    #", "    #", "#### " },
    { " ### ", "#   #", "#   #", "#####", "#   #", "#   #", "#   #" },
    { "#   #", "#   #", "#   #", "#   #", "#   #", " # # ", "  #  " },
    { " ### ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### " },
    { "#   #", "##  #", "# # #", "#  ##", "#   #", "#   #", "#   #" },
    { " ### ", "#   #", "#    ", "# ###", "#   #", "#   #", " ### " },
    { "#### ", "#   #", "#   #", "#   #", "#   #", "#   #", "#### " },
    { " ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### " },
    { "#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  " },
    { "#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    " },
    { "#   #", "#   #", "#   #", "# # #", "# # #", "## ##", "#   #" },
    { "#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#####" },
    { "#### ", "#   #", "#   #", "#### ", "# #  ", "#  # ", "#   #" },
    { "#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    " },
    { "  #  ", "  #  ", " #   ", "     ", "     ", "     ", "     " },
};

// 'str' at (x0, y0), each glyph pixel 2x2, with a 1 pixel shadow
static void draw_string(uint16_t* pixels, int x0, int y0, const char* str) {
    const uint16_t ink = argb4444(15, 15, 15, 15), shadow = argb4444(15, 1, 1, 1);
    for (; *str; str++, x0 += 12) {
        const char* p = strchr(glyph_chars, *str);
        if ((*str == ' ') || !p) {
            continue;
        }
        const char* const* rows = glyph_rows[p - glyph_chars];
        for (int pass = 0; pass < 2; pass++) {
            for (int y = 0; y < 7; y++) {
                for (int x = 0; x < 5; x++) {
                    if (rows[y][x] != '#') {
                        continue;
                    }
                    for (int dy = 0; dy < 2; dy++) {
                        for (int dx = 0; dx < 2; dx++) {
                            int px = x0 + x * 2 + dx + (pass ? 0 : 1), py = y0 + y * 2 + dy + (pass ? 0 : 1);
                            pixels[py * TEXTURE_W + px] = pass ? ink : shadow;
                        }
                    }
                }
            }
        }
    }
}

// the text beside the icon, on a dark panel
static void draw_text(uint16_t* pixels) {
    const uint16_t panel = argb4444(11, 0, 0, 0);
    for (int y = 0; y < ICON_SIZE; y++) {
        for (int x = ICON_SIZE; x < TEXTURE_W; x++) {
            pixels[y * TEXTURE_W + x] = in_rounded(x, y, ICON_SIZE, 6, TEXTURE_W - 2, ICON_SIZE - 7, 6) ? panel : 0;
        }
    }
    draw_string(pixels, ICON_SIZE + 8, 14, "SAVING");
    draw_string(pixels, ICON_SIZE + 8, 36, "DON'T POWER OFF");
}

int save_indicator_init() {
    static uint16_t pixels[TEXTURE_W * ICON_SIZE] __attribute__((aligned(32)));
    icon_texture = pvr_mem_malloc(sizeof(pixels));
    if (icon_texture == NULL) {
        return -1;
    }
    draw_icon(pixels);
    draw_text(pixels);
    pvr_txr_load(pixels, icon_texture, sizeof(pixels));
    return 0;
}

// a save state's result (SAVE_INDICATOR_STATE_RESULT) as a message
__attribute__((cold, noinline)) static void save_indicator_result(int result) {
    save_indicator_active = 0;
    int code = -result - 1;
    char message[24];
    snprintf(message, sizeof(message), (code & 1) ? "STATE %d SAVED" : "STATE %d NOT SAVED", code >> 1);
    frame_hud_message(message, 120);
}

void save_indicator_draw(uint32_t frame) {
    int active = save_indicator_active;
    if (active <= 0) {
        if (active < 0) {
            save_indicator_result(active);
        }
        return;
    }
    if (icon_texture == NULL) {
        return;
    }
    pvr_poly_cxt_t context;
    pvr_poly_hdr_t header;
    pvr_poly_cxt_txr(&context, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, TEXTURE_W, ICON_SIZE,
        icon_texture, PVR_FILTER_BILINEAR);
    pvr_poly_compile(&header, &context);
    pvr_prim(&header, sizeof(header));
    // pulses between 50 and 100% opacity, about once a second
    uint32_t phase = frame % 60;
    uint32_t alpha = 128 + ((phase < 30 ? phase : 60 - phase) * 127) / 30;
    const float w = TEXTURE_W, size = ICON_SIZE, x0 = 640.0f - 16.0f - w, y0 = 480.0f - 16.0f - size;
    pvr_vertex_t vertex;
    vertex.flags = PVR_CMD_VERTEX;
    vertex.z = 2.0f;
    vertex.argb = (alpha << 24) | 0x00ffffff;
    vertex.oargb = 0;
    vertex.x = x0;        vertex.y = y0 + size; vertex.u = 0.0f; vertex.v = 1.0f; pvr_prim(&vertex, sizeof(vertex));
    vertex.x = x0;        vertex.y = y0;        vertex.u = 0.0f; vertex.v = 0.0f; pvr_prim(&vertex, sizeof(vertex));
    vertex.x = x0 + w;    vertex.y = y0 + size; vertex.u = 1.0f; vertex.v = 1.0f; pvr_prim(&vertex, sizeof(vertex));
    vertex.flags = PVR_CMD_VERTEX_EOL;
    vertex.x = x0 + w;    vertex.y = y0;        vertex.u = 1.0f; vertex.v = 0.0f; pvr_prim(&vertex, sizeof(vertex));
}
