#include "video.h"
#include "save_indicator.h"
#include "frame_hud.h"
#include "ppu.h"
#include <kos/genwait.h>
#include <dc/vblank.h>
#include "mapper.h"

#define __ALIGN32__ __attribute__ ((aligned (32)))

#define VQ_CODEBOOK_SIZE 2048
#define VQ_TEXTURE_WIDTH 1024
#define VQ_TEXTURE_HEIGHT 256
#define VQ_INDEX_SIZE (256 * VQ_TEXTURE_HEIGHT)

typedef struct {
    uint8_t codebook[VQ_CODEBOOK_SIZE];
    uint8_t indices[VQ_INDEX_SIZE];
} vq_texture_t;

// FrNES's default NES palette (RGB565)
static const uint16_t nes_palette_565[64] = {
    0x630C, 0x016F, 0x10B3, 0x3013, 0x500F, 0x6008, 0x6020, 0x50E0,
    0x3180, 0x1220, 0x0260, 0x0260, 0x0208, 0x0000, 0x0000, 0x0000,
    0xAD55, 0x1319, 0x3A1F, 0x695F, 0x98F9, 0xB0F0, 0xB165, 0x9A40,
    0x6B40, 0x3C00, 0x1460, 0x0465, 0x03F0, 0x0000, 0x0000, 0x0000,
    0xFFFF, 0x659F, 0x8C9F, 0xC3DF, 0xEB5F, 0xFB7A, 0xFBEF, 0xECC5,
    0xC5C0, 0x8EA0, 0x6705, 0x470F, 0x467A, 0x4A69, 0x0000, 0x0000,
    0xFFFF, 0xBF1F, 0xD69F, 0xE65F, 0xF61F, 0xFE3D, 0xFE59, 0xF6B5,
    0xE712, 0xD772, 0xBF95, 0xB799, 0xB75D, 0xBDD7, 0x0000, 0x0000
};

static pvr_init_params_t pvr_params = {
    // opaque polygons only
    { PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0 },
    // vertex buffer size
    512 * 1024
};

// the ring of textures, allocated as one block so the store queue mapping covers all of them
static vq_texture_t* textures = NULL;
static uint32_t work_index = 0;
static int32_t displayed_index = -1;
// the texture of the last frame generated, presented or not (a skipped frame's texture is reused by the next one)
static int32_t finished_index = -1;
static int frame_open = 0;

uint32_t video_crumb_frame;

// (the starting value: FRNES_DEFAULT_PACING, video.h)
int video_pacing = FRNES_DEFAULT_PACING;
volatile uint32_t video_vblank_count = 0;
static int video_vblank_handle = -1;
// set while the main thread waits in video_wait_vblank: only then does the handler switch to it
static volatile int video_vblank_waiting = 0;

// the refresh rate measured over the last VIDEO_RATE_VBLANKS vblanks (0 until the first measurement)
#define VIDEO_RATE_VBLANKS 120
static volatile float video_measured_hz = 0.0f;
static uint64_t video_rate_start_us = 0;

static void video_vblank(uint32 code, void* data) {
    (void)code;
    (void)data;
    video_vblank_count++;
    if ((video_vblank_count % VIDEO_RATE_VBLANKS) == 0) {
        uint64_t now_us = timer_us_gettime64();
        if (video_rate_start_us) {
            video_measured_hz = (float)VIDEO_RATE_VBLANKS * 1000000.0f / (float)(now_us - video_rate_start_us);
        }
        video_rate_start_us = now_us;
    }
    if (video_vblank_waiting) {
        genwait_wake_all((void*)&video_vblank_count);
        // (switch to the woken thread on the way out of the interrupt, as KOS's PVR handler does, not at the next
        // tick; never while it runs a frame: a reschedule from the interrupt then costs ~1.5 ms of emulation)
        thd_schedule(1);
    }
}

float video_refresh_hz() {
    // (measured: KOS's VGA timing runs at about 60.09 Hz, its NTSC modes at 59.94)
    if (video_measured_hz > 0.0f) {
        return video_measured_hz;
    }
    return (vid_mode && (vid_mode -> flags & VID_PAL)) ? 50.0f : 59.94f;
}

uint32_t video_wait_vblank(uint32_t seen, void (*idle)()) {
    video_vblank_waiting = 1;
    while (video_vblank_count == seen) {
        if (idle) {
            idle();
        }
        // (a wake between the check and the wait is caught by the timeout)
        genwait_wait((void*)&video_vblank_count, "FrNES vblank", 1);
    }
    video_vblank_waiting = 0;
    return video_vblank_count;
}

// player builds set this before video_init: a translucent list for the saving indicator (save_indicator.c)
int video_translucent_overlay = 0;
static uint32_t video_frames_submitted = 0;

#ifdef CAPTURE_MARKER
/*
 * Capture builds (CAPTURE_MARKER=1, tools/capture/): the picture is drawn at exactly 2x (512x480, centred) and the
 * 64 pixel bars on each side carry the number of the frame shown (ppu_frame_count when it was submitted), 16 blocks
 * of 30 lines each, most significant bit at the top: white for 1 on the left, the inverse on the right, so a capture
 * can be matched to the emulated frame and checked even through composite blur.
 */
static void video_capture_marker(uint32_t frame) {
    pvr_poly_cxt_t context;
    pvr_poly_hdr_t header;
    pvr_poly_cxt_col(&context, PVR_LIST_OP_POLY);
    pvr_poly_compile(&header, &context);
    pvr_prim(&header, sizeof(header));
    for (uint32_t bit = 0; bit < 16; bit++) {
        uint32_t one = (frame >> (15 - bit)) & 1;
        for (uint32_t side = 0; side < 2; side++) {
            float x0 = side ? 584.0f : 8.0f, x1 = x0 + 48.0f;
            float y0 = bit * 30.0f + 3.0f, y1 = y0 + 24.0f;
            pvr_vertex_t vertex;
            vertex.z = 1.0f;
            vertex.argb = (one ^ side) ? 0xffffffff : 0xff000000;
            vertex.oargb = 0;
            vertex.u = vertex.v = 0.0f;
            vertex.flags = PVR_CMD_VERTEX;
            vertex.x = x0; vertex.y = y1; pvr_prim(&vertex, sizeof(vertex));
            vertex.x = x0; vertex.y = y0; pvr_prim(&vertex, sizeof(vertex));
            vertex.x = x1; vertex.y = y1; pvr_prim(&vertex, sizeof(vertex));
            vertex.flags = PVR_CMD_VERTEX_EOL;
            vertex.x = x1; vertex.y = y0; pvr_prim(&vertex, sizeof(vertex));
        }
    }
}
#endif

#ifdef CAPTURE_VGA
/*
 * Capture builds (CAPTURE_VGA=1): 640x480 VGA timing (31 kHz, progressive) whatever the cable detect pins say.
 * PURPLE's VGA box doesn't register as one, so KOS gives it 480i NTSC; the box passes 31 kHz RGB through all the
 * same. KOS's vid_set_mode_ex refuses a VGA mode for another cable type, so this is its VGA path with the cable
 * type forced (KOS 2.x hardware/video.c), and vid_mode points at a copy of the builtin VGA mode, as pvr_init expects.
 */
static vid_mode_t video_forced_mode;

static void video_force_vga() {
    vid_mode_t* mode = &video_forced_mode;
    memcpy(mode, &vid_builtin[DM_640x480_VGA], sizeof(*mode));
    mode -> pm = PM_RGB565;
    mode -> fb_size = mode -> width * mode -> height * 2;
    mode -> fb_count = 1;
    vid_set_enabled(false);
    vid_border_color(0, 0, 0);
    mode -> flags &= ~VID_INTERLACE;
    PVR_SET(PVR_FB_CFG_1, (mode -> pm << 2) | (1 << 23));
    // (RGB565 with dither)
    PVR_SET(PVR_FB_CFG_2, 1 | 8);
    PVR_SET(PVR_RENDER_MODULO, (mode -> width * 2) / 8);
    PVR_SET(PVR_FB_SIZE, (((mode -> width * 2) / 4) - 1) | (1 << 20) | ((mode -> height - 1) << 10));
    PVR_SET(PVR_VPOS_IRQ, (mode -> scanint1 << 16) | (mode -> scanint2 << 1));
    PVR_SET(PVR_IL_CFG, 0x100);
    PVR_SET(PVR_BORDER_X, (mode -> borderx1 << 16) | mode -> borderx2);
    PVR_SET(PVR_BORDER_Y, (mode -> bordery1 << 16) | mode -> bordery2);
    PVR_SET(PVR_SCAN_CLK, (mode -> scanlines << 16) | mode -> clocks);
    PVR_SET(PVR_VIDEO_CFG, PVR_GET(PVR_VIDEO_CFG) & ~0x100);
    PVR_SET(PVR_BITMAP_X, mode -> bitmapx);
    PVR_SET(PVR_BITMAP_Y, (mode -> bitmapy << 16) | mode -> bitmapy);
    vid_mode = mode;
    mode -> fb_curr = ~0;
    vid_flip(0);
    // cable type bits (CT_VGA = 0)
    *((vuint32*)0xa0702c00) = (*((vuint32*)0xa0702c00) & 0xfffffcff) | ((CT_VGA & 3) << 8);
    vid_set_enabled(true);
    printf(" ...CAPTURE_VGA: 640x480 VGA timing forced (cable detected as %d)\n", vid_check_cable());
    // the capture card takes several seconds to lock onto the new timing: wait, so a capture sees the run from its
    // first frames
#ifndef CAPTURE_VGA_SETTLE_MS
#define CAPTURE_VGA_SETTLE_MS 8000
#endif
    thd_sleep(CAPTURE_VGA_SETTLE_MS);
}
#endif

int video_init() {
    printf("video - initializing\n");
    {
        volatile uint32_t* c = (volatile uint32_t*)VIDEO_CRUMBS_ADDRESS;
        for (uint32_t i = 0; i < 40; i++) {
            c[i] = 0;
        }
        c[0] = 0x4652;
    }
#ifdef CAPTURE_VGA
    video_force_vga();
#endif
    printf(" ...pvr_init\n");
    if (video_translucent_overlay) {
        pvr_params.opb_sizes[PVR_LIST_TR_POLY] = PVR_BINSIZE_16;
    }
    pvr_init(&pvr_params);
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);

    printf(" ...allocating %u textures (%u bytes)\n", NUM_VIDEO_FRAMES, (unsigned)(NUM_VIDEO_FRAMES * sizeof(vq_texture_t)));
    textures = (vq_texture_t*)pvr_mem_malloc(NUM_VIDEO_FRAMES * sizeof(vq_texture_t));
    if (textures == NULL) {
        printf(" ...ERROR: out of VRAM\n");
        return -1;
    }
    printf(" ...textures at [0x%08lx] - [0x%08lx]\n", (uint32_t)textures, (uint32_t)textures + NUM_VIDEO_FRAMES * sizeof(vq_texture_t));
    if (((uint32_t)textures & 0x1f) != 0) {
        printf(" ...ERROR: textures are not 32 byte aligned\n");
        return -1;
    }
    // the store queue mapping covers 2 MB starting at the 1 MB page of the first texture
    uint32_t start_page = ((uint32_t)textures & 0x00ffffff) >> 20;
    uint32_t end_page = (((uint32_t)textures & 0x00ffffff) + NUM_VIDEO_FRAMES * sizeof(vq_texture_t) - 1) >> 20;
    if (end_page - start_page > 1) {
        printf(" ...ERROR: textures span more than 2 MB of pages\n");
        return -1;
    }

    // codebook: entry i is 4 texels of NES color (i % 64), the upper index bits are free for the renderer; the colors
    // are the frontend's palette when it chose one (frnes_settings.palette_set, frnes_palette())
    __ALIGN32__ static uint16_t codebook[VQ_CODEBOOK_SIZE / 2];
    // (a Vs. System game's RGB PPU has its own palette, which the game's colors are chosen for)
    const uint16_t* palette = vs_palette();
    if (palette == NULL) {
        palette = frnes_settings.palette_set ? frnes_palette() : nes_palette_565;
    }
    for (uint32_t i = 0; i < 256; i++) {
        for (uint32_t t = 0; t < 4; t++) {
            codebook[(i << 2) + t] = palette[i & 63];
        }
    }
    printf(" ...uploading codebooks\n");
    for (uint32_t i = 0; i < NUM_VIDEO_FRAMES; i++) {
        pvr_txr_load(codebook, textures[i].codebook, VQ_CODEBOOK_SIZE);
        pvr_txr_load(ppu_framebuffer, textures[i].indices, PPU_FRAMEBUFFER_WIDTH * PPU_FRAMEBUFFER_HEIGHT);
    }
    work_index = 0;
    displayed_index = -1;
    finished_index = -1;
    frame_open = 0;
    printf(" ...done\n");
    return 0;
}

void video_vblank_start() {
    // (the refresh rate measured again for this game: across the menus between two games, the first measurement took
    // 120 vblanks over minutes, a rate near 0 Hz that put the frame loop on the sound's pacing)
    video_measured_hz = 0.0f;
    video_rate_start_us = 0;
    if (video_vblank_handle < 0) {
        video_vblank_handle = vblank_handler_add(video_vblank, NULL);
    }
}

void video_shutdown() {
    printf("video - shutting down\n");
    if (frame_open) {
        sq_wait();
        frame_open = 0;
    }
    ppu_set_output(ppu_framebuffer, 0);
    if (video_vblank_handle >= 0) {
        vblank_handler_remove(video_vblank_handle);
        video_vblank_handle = -1;
    }
    pvr_shutdown();
    printf(" ...done\n");
}

/*
 * VIDEO_TXR_LOAD is the comparison baseline: the PPU renders to ppu_framebuffer in main RAM and the whole frame is
 * copied to the texture with pvr_txr_load (store queues, outside frame generation) when the frame ends.
 */
void video_frame_begin() {
    // store queue writes go through the TA's 64-bit texture path, like pvr_txr_load. With the MMU on, the two
    // UTLB entries KOS reserves for store queue translation (0x3e / 0x3f, 1 MB each) are pointed at the 2 MB of
    // VRAM around this address (frnes_sq_map: LDTLB rather than KOS's sq_lock, see memory.c). The store queues
    // are ours until the frame ends: the sound stream uses SPU DMA, apu_synth_poll runs between frames.
    uint8_t* indices = textures[work_index].indices;
    uint8_t* sq = (uint8_t*)frnes_sq_map((void*)(PVR_TA_TEX_MEM | ((uint32_t)indices & 0x00ffffff)));
    ppu_set_output(sq, 1);
    frame_open = 1;
}

int video_frame_end(int wait) {
    if (frame_open) {
        // drain both store queues before giving them back
        sq_wait();
        frame_open = 0;
    }
    ppu_set_output(ppu_framebuffer, 0);
    finished_index = work_index;

    if (!wait && (pvr_check_ready() != 0)) {
        // PVR still busy with the previous frame, skip presenting this one and reuse the texture
        return 0;
    }

    video_crumb(CRUMB_PVR_READY);
    if (wait) {
        pvr_wait_ready();
    }
    pvr_scene_begin();
    video_crumb(CRUMB_PVR_SCENE);
    pvr_list_begin(PVR_LIST_OP_POLY);

    pvr_poly_cxt_t context;
    pvr_poly_hdr_t header;
    pvr_poly_cxt_txr(&context, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED | PVR_TXRFMT_VQ_ENABLE,
        VQ_TEXTURE_WIDTH, VQ_TEXTURE_HEIGHT, &textures[work_index], video_picture.filter);
    // (u and v clamped: at a picture edge between screen pixels, as 8:7's, a sample a hair past the texture's edge
    // would wrap round to the other side's column)
    context.txr.uv_clamp = PVR_UVCLAMP_UV;
    pvr_poly_compile(&header, &context);
    pvr_prim(&header, sizeof(header));

#ifdef CAPTURE_MARKER
    // exactly 2x, centred, with the frame number in the side bars
    const video_picture_t picture = { 64.0f, 0.0f, 576.0f, 480.0f, 0.0f, 0.0f, 1.0f, 240.0f / 256.0f, PVR_FILTER_NONE };
#else
    // the picture mode and crop (video_picture_update)
    const video_picture_t picture = video_picture;
#endif
    pvr_vertex_t vertex;
    vertex.flags = PVR_CMD_VERTEX;
    vertex.z = 1.0f;
    vertex.argb = 0xffffffff;
    vertex.oargb = 0;
    vertex.x = picture.x1; vertex.y = picture.y2; vertex.u = picture.u1; vertex.v = picture.v2; pvr_prim(&vertex, sizeof(vertex));
    vertex.x = picture.x1; vertex.y = picture.y1; vertex.u = picture.u1; vertex.v = picture.v1; pvr_prim(&vertex, sizeof(vertex));
    vertex.x = picture.x2; vertex.y = picture.y2; vertex.u = picture.u2; vertex.v = picture.v2; pvr_prim(&vertex, sizeof(vertex));
    vertex.flags = PVR_CMD_VERTEX_EOL;
    vertex.x = picture.x2; vertex.y = picture.y1; vertex.u = picture.u2; vertex.v = picture.v1; pvr_prim(&vertex, sizeof(vertex));
#ifdef CAPTURE_MARKER
    video_capture_marker(ppu_frame_count);
#endif

    pvr_list_finish();
    if (video_translucent_overlay) {
        // the player's overlays: the saving indicator and the frame display
        video_crumb(CRUMB_PVR_OVERLAY);
        pvr_list_begin(PVR_LIST_TR_POLY);
        save_indicator_draw(video_frames_submitted);
        frame_hud_draw();
        pvr_list_finish();
    }
    video_crumb(CRUMB_PVR_FINISH);
    pvr_scene_finish();
    video_frames_submitted++;

    displayed_index = work_index;
    work_index = (work_index + 1) % NUM_VIDEO_FRAMES;
    return 1;
}

/*
 * The last frame shown again, with the overlays (the game menu, while the game is paused): waits for the PVR, and
 * leaves the texture ring as it was
 */
FRNES_COLD_TEXT void video_redraw(void) {
    // (the last frame finished: displayed_index, which nothing else reads, would become a global of its own)
    int32_t index = finished_index;
    if (index < 0) {
        return;
    }
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_poly_cxt_t context;
    pvr_poly_hdr_t header;
    pvr_poly_cxt_txr(&context, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED | PVR_TXRFMT_VQ_ENABLE,
        VQ_TEXTURE_WIDTH, VQ_TEXTURE_HEIGHT, &textures[index], video_picture.filter);
    // (u and v clamped: at a picture edge between screen pixels, as 8:7's, a sample a hair past the texture's edge
    // would wrap round to the other side's column)
    context.txr.uv_clamp = PVR_UVCLAMP_UV;
    pvr_poly_compile(&header, &context);
    pvr_prim(&header, sizeof(header));
    const video_picture_t picture = video_picture;
    pvr_vertex_t vertex;
    vertex.flags = PVR_CMD_VERTEX;
    vertex.z = 1.0f;
    vertex.argb = 0xffffffff;
    vertex.oargb = 0;
    vertex.x = picture.x1; vertex.y = picture.y2; vertex.u = picture.u1; vertex.v = picture.v2; pvr_prim(&vertex, sizeof(vertex));
    vertex.x = picture.x1; vertex.y = picture.y1; vertex.u = picture.u1; vertex.v = picture.v1; pvr_prim(&vertex, sizeof(vertex));
    vertex.x = picture.x2; vertex.y = picture.y2; vertex.u = picture.u2; vertex.v = picture.v2; pvr_prim(&vertex, sizeof(vertex));
    vertex.flags = PVR_CMD_VERTEX_EOL;
    vertex.x = picture.x2; vertex.y = picture.y1; vertex.u = picture.u2; vertex.v = picture.v1; pvr_prim(&vertex, sizeof(vertex));
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    frame_hud_draw();
    pvr_list_finish();
    pvr_scene_finish();
}

void video_read_frame(uint8_t* out) {
    // the last frame generated: call before the next frame starts writing to the texture
    int32_t index = finished_index;
    if (index < 0) {
        memset(out, 0, PPU_FRAMEBUFFER_WIDTH * PPU_FRAMEBUFFER_HEIGHT);
        return;
    }
    // read back through the uncached 64-bit VRAM view, which is how the TA path laid the data out
    uint32_t offset = ((uint32_t)textures[index].indices) & 0x007fffff;
    uint32_t* src = (uint32_t*)(PVR_RAM_INT_BASE + offset);
    uint32_t* dst = (uint32_t*)out;
    for (uint32_t i = 0; i < (PPU_FRAMEBUFFER_WIDTH * PPU_FRAMEBUFFER_HEIGHT) / 4; i++) {
        dst[i] = src[i];
    }
}
