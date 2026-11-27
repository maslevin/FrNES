/*
 * The game menu (game_menu.h): a panel over the paused game, in the frame display's glyphs. It runs its own loop at
 * the end of a frame (state_hotkey_run): the frame is put on the screen first (video_frame_end), then shown again
 * with the panel each vblank (video_redraw), the sound stream fed silence (apu_synth_muted, apu_synth_poll) so the AICA
 * doesn't loop its last buffer. Up / down choose, left / right change the slot or switch a code, A does it, B or Start
 * go back to the game. Built without LTO, as the boards are: it's cold, and its code moves nothing that runs every
 * frame.
 */
#include <kos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game_menu.h"
#include "frame_hud.h"
#include "video.h"
#include "apu_synth.h"
#include "state.h"
#include "gamegenie.h"
#include "frnes.h"
#include "fds.h"
#include "cpu.h"
#include "memory.h"
#include "cartridge.h"
#include "input.h"
#include "mapper.h"

/*
 * The text as a VQ texture whose codebook entries are 4 texels in a row (the NES picture's layout, video.c): an index
 * byte is 4 pixels, so a glyph's 8 pixel row is 2 bytes. Entry (color << 4) | pattern: the pattern's bits, the
 * leftmost first, in that color's ink, the rest clear. The indices are made in RAM and loaded whole once the PVR is
 * done with the last frame
 */
#define MENU_TEXTURE_W 256
#define MENU_TEXTURE_H 256
#define MENU_ROW_BYTES (MENU_TEXTURE_W / 4)
#define MENU_CODEBOOK 2048
#define MENU_INK_TEXT 0
#define MENU_INK_CHOSEN 1
#define MENU_INK_TITLE 2
#define MENU_COLUMNS 24
#define MENU_LINE 12
#define MENU_SCALE 2.0f

enum { ITEM_RESUME, ITEM_SAVE, ITEM_LOAD, ITEM_SLOT, ITEM_CODE, ITEM_ZAPPER, ITEM_PICTURE, ITEM_SMOOTH, ITEM_HUD, ITEM_RESET, ITEM_DISK, ITEM_EXIT };

typedef struct {
    uint8_t kind, code;
} menu_item_t;

// the menu's state (load time and rare events only: last in .bss, see memory.h)
static struct {
    pvr_ptr_t texture;          // the codebook, then the indices
    uint8_t* indices;           // (in RAM, on the heap while the menu is open)
    menu_item_t items[16];
    uint32_t count, selected;
    // the Game Genie codes switched off (bit per code), for the game (crc) they were switched for
    uint8_t codes_off;
    uint32_t codes_crc;
} menu FRNES_COLD_BSS;

static int menu_has_code(int i) {
    const char (*codes)[9] = frnes_cheats();
    return (codes != NULL) && codes[i][0];
}

static void menu_items() {
    menu.count = 0;
    menu.items[menu.count++] = (menu_item_t){ ITEM_RESUME, 0 };
    menu.items[menu.count++] = (menu_item_t){ ITEM_SAVE, 0 };
    menu.items[menu.count++] = (menu_item_t){ ITEM_LOAD, 0 };
    menu.items[menu.count++] = (menu_item_t){ ITEM_SLOT, 0 };
    for (int i = 0; i < FRNES_CHEATS; i++) {
        if (menu_has_code(i)) {
            menu.items[menu.count++] = (menu_item_t){ ITEM_CODE, (uint8_t)i };
        }
    }
    // each Zapper the game has, on or off (Bayou Billy: the pad for its driving and fighting, the Zapper for its
    // shooting)
    for (uint8_t port = 0; port < 2; port++) {
        if (input_port_device_active(port) == INPUT_DEVICE_ZAPPER) {
            menu.items[menu.count++] = (menu_item_t){ ITEM_ZAPPER, port };
        }
    }
    menu.items[menu.count++] = (menu_item_t){ ITEM_PICTURE, 0 };
    menu.items[menu.count++] = (menu_item_t){ ITEM_SMOOTH, 0 };
    menu.items[menu.count++] = (menu_item_t){ ITEM_HUD, 0 };
    menu.items[menu.count++] = (menu_item_t){ ITEM_RESET, 0 };
    if (cartridge_mapper_number == 20) {
        menu.items[menu.count++] = (menu_item_t){ ITEM_DISK, 0 };
    }
    menu.items[menu.count++] = (menu_item_t){ ITEM_EXIT, 0 };
}

// a string at character column col of text line 'line' (12 pixels apart), in an ink
static void menu_put(uint32_t line, uint32_t col, const char* str, uint32_t ink) {
    for (; *str && (col < MENU_TEXTURE_W / 8); str++, col++) {
        for (uint32_t row = 0; row < 7; row++) {
            uint8_t bits = frame_hud_glyph_row(*str, row);
            uint8_t* at = &menu.indices[(line * MENU_LINE + row) * MENU_ROW_BYTES + col * 2];
            at[0] = (ink << 4) | (bits >> 4);
            at[1] = (ink << 4) | (bits & 15);
        }
    }
}

static void menu_text() {
    memset(menu.indices, 0, MENU_ROW_BYTES * MENU_TEXTURE_H);
    menu_put(0, 0, "     GAME MENU", MENU_INK_TITLE);
    char line[MENU_COLUMNS + 1];
    const char (*codes)[9] = frnes_cheats();
    for (uint32_t i = 0; i < menu.count; i++) {
        const menu_item_t* item = &menu.items[i];
        switch (item -> kind) {
            case ITEM_RESUME: snprintf(line, sizeof(line), "RESUME"); break;
            case ITEM_SAVE: snprintf(line, sizeof(line), "SAVE STATE %u", state_slot); break;
            case ITEM_LOAD: snprintf(line, sizeof(line), "LOAD STATE %u", state_slot); break;
            case ITEM_SLOT: snprintf(line, sizeof(line), "SLOT        %u", state_slot); break;
            case ITEM_CODE: {
                char code[9];
                snprintf(code, sizeof(code), "%s", codes[item -> code]);
                for (char* c = code; *c; c++) {
                    *c = ((*c >= 'a') && (*c <= 'z')) ? (*c - 32) : *c;
                }
                snprintf(line, sizeof(line), "CODE %-8s  %s", code, (menu.codes_off & (1 << item -> code)) ? "OFF" : "ON");
            } break;
            case ITEM_ZAPPER: snprintf(line, sizeof(line), "ZAPPER %u       %s", item -> code + 1, zapper_enabled(item -> code) ? "ON" : "OFF"); break;
            case ITEM_PICTURE: {
                static const char* const modes[FRNES_PICTURE_MODES] = { "FULL", "2X", "8:7" };
                uint32_t mode = (frnes_settings.picture < FRNES_PICTURE_MODES) ? frnes_settings.picture : 0;
                snprintf(line, sizeof(line), "VIDEO MODE     %s", modes[mode]);
            } break;
            case ITEM_SMOOTH: snprintf(line, sizeof(line), "FILTERING      %s", frnes_settings.bilinear ? "BILINEAR" : "OFF"); break;
            case ITEM_HUD: snprintf(line, sizeof(line), "FRAME DISPLAY  %s", frame_hud_mode() ? "ON" : "OFF"); break;
            case ITEM_RESET: snprintf(line, sizeof(line), "RESET"); break;
            case ITEM_DISK: snprintf(line, sizeof(line), "NEXT DISK SIDE"); break;
            case ITEM_EXIT: snprintf(line, sizeof(line), "EXIT TO MENUS"); break;
        }
        menu_put(i + 2, 1, line, (i == menu.selected) ? MENU_INK_CHOSEN : MENU_INK_TEXT);
    }
    pvr_txr_load(menu.indices, (uint8_t*)menu.texture + MENU_CODEBOOK, MENU_ROW_BYTES * MENU_TEXTURE_H);
}

// the codebook: every pattern of 4 pixels in each ink, on clear
static int menu_codebook() {
    static const uint16_t inks[3] = { 0xffff, 0xfff6, 0xfff8 };
    uint16_t* codebook = malloc(MENU_CODEBOOK);
    if (codebook == NULL) {
        return -1;
    }
    for (uint32_t e = 0; e < 256; e++) {
        uint32_t ink = e >> 4, pattern = e & 15;
        for (uint32_t t = 0; t < 4; t++) {
            codebook[e * 4 + t] = ((ink < 3) && (pattern & (8 >> t))) ? inks[ink] : 0x0000;
        }
    }
    pvr_txr_load(codebook, menu.texture, MENU_CODEBOOK);
    free(codebook);
    return 0;
}

static void menu_quad(float x, float y, float w, float h, uint32_t argb, float u1, float v1) {
    pvr_vertex_t v;
    v.flags = PVR_CMD_VERTEX;
    v.z = 4.0f;
    v.argb = argb;
    v.oargb = 0;
    v.x = x;     v.y = y + h; v.u = 0.0f; v.v = v1;   pvr_prim(&v, sizeof(v));
    v.x = x;     v.y = y;     v.u = 0.0f; v.v = 0.0f; pvr_prim(&v, sizeof(v));
    v.x = x + w; v.y = y + h; v.u = u1;   v.v = v1;   pvr_prim(&v, sizeof(v));
    v.flags = PVR_CMD_VERTEX_EOL;
    v.x = x + w; v.y = y;     v.u = u1;   v.v = 0.0f; pvr_prim(&v, sizeof(v));
}

// (frame_hud_overlay while the menu is open)
static void menu_draw(void) {
    const float w = MENU_COLUMNS * 8 * MENU_SCALE, h = (menu.count + 3) * MENU_LINE * MENU_SCALE;
    const float x = (640.0f - w) / 2.0f, y = (480.0f - h) / 2.0f;
    pvr_poly_cxt_t context;
    pvr_poly_hdr_t header;
    pvr_poly_cxt_col(&context, PVR_LIST_TR_POLY);
    pvr_poly_compile(&header, &context);
    pvr_prim(&header, sizeof(header));
    menu_quad(x - 12.0f, y - 12.0f, w + 24.0f, h + 12.0f, 0xd0000000, 0, 0);
    // (the chosen item's bar)
    menu_quad(x, y + (menu.selected + 2) * MENU_LINE * MENU_SCALE - 4.0f, w, MENU_LINE * MENU_SCALE, 0xa0207020, 0, 0);
    pvr_poly_cxt_txr(&context, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED | PVR_TXRFMT_VQ_ENABLE,
        MENU_TEXTURE_W, MENU_TEXTURE_H, menu.texture, PVR_FILTER_NONE);
    pvr_poly_compile(&header, &context);
    pvr_prim(&header, sizeof(header));
    const float th = (menu.count + 3) * MENU_LINE;
    menu_quad(x, y, MENU_TEXTURE_W * MENU_SCALE, th * MENU_SCALE, 0xffffffff, 1.0f, th / MENU_TEXTURE_H);
}

// the Game Genie codes put on again without the ones switched off
static void menu_apply_codes() {
    const char (*codes)[9] = frnes_cheats();
    if (codes == NULL) {
        return;
    }
    char on[FRNES_CHEATS][9];
    for (int i = 0; i < FRNES_CHEATS; i++) {
        snprintf(on[i], sizeof(on[i]), "%s", (menu.codes_off & (1 << i)) ? "" : codes[i]);
    }
    gamegenie_apply((const char (*)[9])on, &gamegenie_undo);
}

// the NES's reset button: the CPU through its reset vector (stack down 3, interrupts off), the APU silenced, the PPU's
// rendering and NMI off; the board and the RAM as they are
static void menu_reset() {
    // (the console's reset button, made at the next input point: mapper.h; a recording takes it in)
    frnes_soft_reset();
}

static void menu_disk_side() {
    int side = fds_next_side();
    if (side < 0) {
        return;
    }
    if (fds_drive_busy()) {
        frame_hud_message("DISK BUSY", 120);
        return;
    }
    char message[24];
    snprintf(message, sizeof(message), "DISK %d SIDE %c", side / 2 + 1, 'A' + (side & 1));
    frame_hud_message(message, 120);
}

static uint32_t menu_buttons() {
    maple_device_t* pad = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t* state = pad ? (cont_state_t*)maple_dev_status(pad) : NULL;
    return state ? state -> buttons : 0;
}

int game_menu_run(void) {
    if (menu.codes_crc != currentCRC32) {
        menu.codes_crc = currentCRC32;
        menu.codes_off = 0;
    }
    // (the frame finished so far on the screen, the store queues given back: the PVR, its texture loads and the sound
    // stream are the menu's from here)
    video_frame_end(1);
    pvr_wait_ready();
    menu.texture = pvr_mem_malloc(MENU_CODEBOOK + MENU_ROW_BYTES * MENU_TEXTURE_H);
    menu.indices = malloc(MENU_ROW_BYTES * MENU_TEXTURE_H);
    if ((menu.texture == NULL) || (menu.indices == NULL) || (menu_codebook() != 0)) {
        if (menu.texture != NULL) {
            pvr_mem_free(menu.texture);
        }
        free(menu.indices);
        menu.texture = NULL;
        menu.indices = NULL;
        return 0;
    }
    menu_items();
    menu.selected = 0;
    menu_text();
    int muted = apu_synth_muted;
    apu_synth_muted = 1;
    void (*overlay)(void) = frame_hud_overlay;
    frame_hud_overlay = menu_draw;
    // (the buttons held as it opens don't count until let go)
    uint32_t held = menu_buttons();
    int done = 0, exit_game = 0;
    while (!done) {
        apu_synth_poll();
        video_redraw();
        uint32_t buttons = menu_buttons();
        uint32_t pressed = buttons & ~held;
        held = buttons;
        if (!pressed) {
            continue;
        }
        const menu_item_t* item = &menu.items[menu.selected];
        if (pressed & CONT_DPAD_UP) {
            menu.selected = (menu.selected + menu.count - 1) % menu.count;
        } else if (pressed & CONT_DPAD_DOWN) {
            menu.selected = (menu.selected + 1) % menu.count;
        } else if ((pressed & (CONT_DPAD_LEFT | CONT_DPAD_RIGHT)) && (item -> kind == ITEM_SLOT || item -> kind == ITEM_SAVE ||
            item -> kind == ITEM_LOAD)) {
            state_slot = (pressed & CONT_DPAD_RIGHT) ? ((state_slot + 1) % 10) : ((state_slot + 9) % 10);
        } else if ((pressed & (CONT_DPAD_LEFT | CONT_DPAD_RIGHT | CONT_A)) && (item -> kind == ITEM_PICTURE)) {
            // (the picture's corners: the paused frame behind the menu is drawn with them at once, the game's frames
            // after it; the frontend keeps the mode, frontend.c)
            uint32_t mode = (frnes_settings.picture < FRNES_PICTURE_MODES) ? frnes_settings.picture : 0;
            mode = (pressed & CONT_DPAD_LEFT) ? (mode + FRNES_PICTURE_MODES - 1) : (mode + 1);
            frnes_settings.picture = mode % FRNES_PICTURE_MODES;
            video_picture_update(&frnes_settings);
        } else if ((pressed & (CONT_DPAD_LEFT | CONT_DPAD_RIGHT | CONT_A)) && (item -> kind == ITEM_SMOOTH)) {
            frnes_settings.bilinear = !frnes_settings.bilinear;
            video_picture_update(&frnes_settings);
        } else if ((pressed & (CONT_DPAD_LEFT | CONT_DPAD_RIGHT | CONT_A)) && (item -> kind == ITEM_HUD)) {
            frame_hud_set_mode(frame_hud_mode() ? 0 : 1);
        } else if ((pressed & (CONT_DPAD_LEFT | CONT_DPAD_RIGHT | CONT_A)) && (item -> kind == ITEM_ZAPPER)) {
            zapper_set_enabled(item -> code, !zapper_enabled(item -> code));
        } else if ((pressed & (CONT_DPAD_LEFT | CONT_DPAD_RIGHT | CONT_A)) && (item -> kind == ITEM_CODE)) {
            menu.codes_off ^= 1 << item -> code;
            menu_apply_codes();
        } else if (pressed & (CONT_B | CONT_START)) {
            done = 1;
        } else if (pressed & CONT_A) {
            switch (item -> kind) {
                case ITEM_RESUME: done = 1; break;
                case ITEM_SAVE: state_slot_save(state_slot); done = 1; break;
                case ITEM_LOAD: done = (state_slot_load(state_slot) == 0); break;
                case ITEM_RESET: menu_reset(); done = 1; break;
                case ITEM_DISK: menu_disk_side(); done = 1; break;
                case ITEM_EXIT: done = exit_game = 1; break;
            }
        }
        // (the texture rewritten only once the PVR is done rendering from it: written during a render, the panel flickered)
        pvr_wait_ready();
        menu_text();
    }
    frame_hud_overlay = overlay;
    apu_synth_muted = muted;
    pvr_mem_free(menu.texture);
    free(menu.indices);
    menu.texture = NULL;
    menu.indices = NULL;
    return exit_game;
}
