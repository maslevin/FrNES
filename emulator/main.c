#include <kos.h>
#include <stdlib.h>

#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "memory.h"
#include "movie.h"
#include "cartridge.h"
#include "mapper.h"
#include "input.h"
#include "vmu_save.h"
#include "save_indicator.h"
#include "frame_hud.h"
#include <dc/wdt.h>
#include "trigger_gesture.h"
#include "video.h"
#include "apu_synth.h"
#include "input_recording.h"
#include "fds.h"
#include "frnes.h"
#include "state.h"
#include "gamegenie.h"

/*
 * Player build options (see the Makefile): FRNES_ROM_PATH picks the ROM (player_main.c), FRNES_RECORD records the controller from
 * power on and FRNES_PLAYBACK_PATH replays a recording instead of reading the controller.
 *
 * The recording is kept in RAM while playing (writing over /pc/ blocks for long enough to starve the sound
 * stream). Holding both triggers fully for a second stops recording and saves it (the game keeps running), and
 * exiting saves it if it hasn't been. It goes to /pc/recordings/<ROM CRC>_NNN.frm, the first unused NNN. (The left
 * trigger is Select, except while the right one is pressed, so the gesture doesn't record Select presses.)
 */
// the ROM frnes_run is playing (the VMU save's description is its file name)
// (the game being played: the VMU saves' descriptions, state_hotkeys.c's too)
const char* frnes_rom_path = "";
// frames between FPS lines (the autosave test build reports every 30)
#ifdef FRNES_AUTOSAVE_TEST
#define FPS_INTERVAL 30
#else
#define FPS_INTERVAL 300
#endif
// the frame display (frame_hud.c): on unless built with HUD=0; holding the right trigger alone for a second cycles it
// off / operand cache misses / instruction cache misses (on FDS games a tap does, and the hold changes the disk side)
#ifndef FRNES_FRAME_HUD
#define FRNES_FRAME_HUD 1
#endif
#define HUD_TOGGLE_FRAMES 60
// (FDS games: the right trigger changes the disk side, see fds_hand_change; how long the new side is shown)
#define FDS_MESSAGE_FRAMES 120
#define RECORDING_DIRECTORY "/pc/recordings"

#define EXIT_BUTTONS (CONT_A | CONT_B | CONT_X | CONT_Y | CONT_START)
// a Vs. System game's coin 1: the left trigger with Start (the pad's Select and Start are left out while they're held)
#define VS_COIN_PRESSED(state) (((state) -> ltrig > LEFT_TRIGGER_PRESS) && ((state) -> buttons & CONT_START))
// the left trigger pressed (0-255; mapper_boards.c's pads take it as Select); the right trigger idle below
#define LEFT_TRIGGER_PRESS 128
#define RIGHT_TRIGGER_IDLE 64
int initialize_frnes(const char* filepath) {
    if (initialize_memory() == -1) {
        return -1;
    }

    if (initialize_registers() == -1) {
        return -1;
    }

    if (initialize_ppu_memory() == -1) {
        return -1;
    }

    if (load_nes_file(filepath) == -1) {
        return -1;
    }

    // Mapper 0 without mapper framework

	/* Set ROM Banks */
	if (num_8k_ROM_pages > 3) {
        initialize_rom(ROM_pages[0], ROM_pages[1], ROM_pages[2], ROM_pages[3]);
	} else if (num_8k_ROM_pages > 1) {
        initialize_rom(ROM_pages[0], ROM_pages[1], ROM_pages[0], ROM_pages[1]);
	} else {
        initialize_rom(ROM_pages[0], ROM_pages[0], ROM_pages[0], ROM_pages[0]);
	}

	/* Set PPU Banks */
    if (initialize_ppu_pattern_tables(VROM_pages[0], VROM_pages[1], VROM_pages[2], VROM_pages[3], VROM_pages[4], VROM_pages[5], VROM_pages[6], VROM_pages[7]) == -1) {
        return -1;
    }

    /* Nametable mirroring from the iNES header (flags 6 bit 0: 0 = horizontal, 1 = vertical) */
    set_ppu_nametable_mirroring(cartridge_single_screen ? ((cartridge_single_screen == 1) ? NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW : NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH) :
        ((NesHeader.byInfo1 & 1) ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL));


/*
    // right now cartridge.c is initializing wram configuration, but theoretically it would be better if we were doing that here, based on the results of cartridge state to keep 
    // all initialization in the same place
    if (initialize_wram() == -1) {
        return -1;
    }
*/        

    if (mapper_init(cartridge_mapper_number) == -1) {
        return -1;
    }

    if (initialize_cpu() == -1) {
        return -1;
    }

    if (initialize_ppu() == -1) {
        return -1;
    }

    if (initialize_apu() == -1) {
        return -1;
    }
    return 0;
}

void shutdown_frnes() {
    shutdown_memory();

    unload_nes_file();

    shutdown_cpu();

    shutdown_ppu();

    shutdown_apu();
}

/*
 * Battery backed PRG RAM: loaded from /pc/saves/<ROM CRC>.sav at start and written back at exit (playback builds
 * leave it alone so that replays start the same). The game writes it through the $6000 MMU window (cached), so the
 * save reads each 8 kB bank back through that window. Nothing is written unless the game ran for a while (an exit
 * right after start, e.g. with no controller attached, must not replace a real save) and the RAM changed since it
 * was loaded; the file is written to <name>.tmp first and then renamed over the old one.
 */
#define SAVE_MIN_FRAMES 300
// (FRNES_SAVE_DIRECTORY: elsewhere, e.g. a dev setup serving another folder as /pc)
#ifndef FRNES_SAVE_DIRECTORY
#define FRNES_SAVE_DIRECTORY "/pc/saves"
#endif
#define SAVE_DIRECTORY FRNES_SAVE_DIRECTORY
// the save file's contents: WRAM, or for a mapper keeping less than 8 kB itself (MMC6's 1 kB at $7000-$7FFF) an 8 kB
// image of $6000-$7FFF with it in each of its mirrors, as old FrNES (which treated it as 8 kB of WRAM) saved it
#define BATTERY_IMAGE_SIZE 0x2000
#define BATTERY_IMAGE_MAPPER_OFFSET 0x1000
// the CRC of the battery RAM as last loaded or saved (to the VMU), and as last written to SAVE_DIRECTORY (dev setups'
// /pc, at the game's end only: see battery_write)
static uint32_t battery_saved_crc;
static uint32_t battery_file_crc FRNES_COLD_BSS;
#ifdef FRNES_AUTOSAVE_TEST
// the VMU had this game's save at start (the forced test save then isn't written to the VMU)
static int autosave_test_existing;
#endif

// The battery RAM as the game sees it: the mapper's own (MMC6), or all of WRAM. The game writes WRAM through the
// $6000 window (write-back, cached at the window's addresses), so the window's lines are written back first and WRAM
// is read uncached: the window's mapping (MMC5 / FME-7 banking) is never touched.
__attribute__((cold, noinline)) static const uint8_t* battery_data(uint32_t* size) {
    const uint8_t* data = mapper_battery_data(size);
    if (data) {
        mapper_battery_prepare();
        return data;
    }
    *size = WRAM_size;
    // the mapped bank's lines, through the window's addresses (the cache is indexed by them); the other banks were
    // written back when they were unmapped (set_wram_bank_pointer)
    wram_window_writeback();
    return (const uint8_t*)(((uintptr_t)WRAM & 0x1fffffff) | 0xa0000000);
}

__attribute__((cold, noinline)) static uint32_t battery_crc() {
    uint32_t size;
    const uint8_t* data = battery_data(&size);
    return input_recording_crc32(data, size);
}

// battery RAM, or memory the mapper keeps itself (MMC6's RAM, Bandai's EEPROMs: headers often don't set the bit).
// Worked out once, on the first call after the ROM is loaded: it is checked every frame, and mapper_battery_data()
// isn't cheap for every board (the FDS builds its save from both disk sides, ~130 kB, which took 60 fps to 24)
// (a ROM the game database says has no battery, though its header has one: saved only if a save was there to load)
static int battery_db_save_missing = 0;
static int battery_backed_cache = -1;
static int battery_backed() {
    int backed = battery_backed_cache;
    if (backed < 0) {
        uint32_t size;
        backed = (mapper_battery_data(&size) != NULL) ||
            ((NesHeader.byInfo1 & 2) && (WRAM != NULL) && (WRAM_size >= 0x2000));
        battery_backed_cache = backed;
    }
    return backed && !battery_db_save_missing;
}

// where battery saves go (frnes.h's FRNES_BATTERY_*: the frontend's choice, kept across games)
static uint8_t battery_place FRNES_COLD_BSS;

FRNES_COLD_TEXT void battery_set_place(uint8_t place) {
    battery_place = (place < FRNES_BATTERY_PLACES) ? place : FRNES_BATTERY_VMU;
}

static const char* battery_directory(void) {
    return (battery_place == FRNES_BATTERY_SD) ? FRNES_BATTERY_SD_DIRECTORY : SAVE_DIRECTORY;
}

static void battery_path(char* path, size_t size) {
    snprintf(path, size, "%s/%08lX.sav", battery_directory(), (unsigned long)currentCRC32);
}

// a malloc'd copy of the battery RAM; NULL when out of memory
__attribute__((cold, noinline)) static uint8_t* battery_snapshot(uint32_t* size) {
    const uint8_t* data = battery_data(size);
    if (*size < BATTERY_IMAGE_SIZE) {
        // the mapper's own RAM, as an 8 kB image (see battery_load)
        uint8_t* image = calloc(1, BATTERY_IMAGE_SIZE);
        if (image) {
            for (uint32_t mirror = BATTERY_IMAGE_MAPPER_OFFSET; mirror + *size <= BATTERY_IMAGE_SIZE; mirror += *size) {
                memcpy(image + mirror, data, *size);
            }
        }
        *size = BATTERY_IMAGE_SIZE;
        return image;
    }
    uint8_t* copy = malloc(*size);
    if (copy) {
        memcpy(copy, data, *size);
    }
    return copy;
}

// the battery RAM is loaded from the VMU (old FrNES's saves too), else from /pc/saves
// the battery RAM is loaded from the VMU (old FrNES's saves too), else from /pc/saves
__attribute__((cold, noinline)) static void battery_load() {
    if (battery_place == FRNES_BATTERY_OFF) {
        // (no battery saves: neither loaded nor written, battery_backed() then false)
        printf("SAVE off: battery saves are turned off\n");
        battery_db_save_missing = 1;
        return;
    }
    uint32_t size;
    uint8_t* data = mapper_battery_data(&size);
    // (a mapper's save larger than the 8 kB image, the FDS's written sides, is loaded as it is)
    uint8_t direct = data && (size > BATTERY_IMAGE_SIZE);
    uint32_t image_size = direct ? size : (data ? BATTERY_IMAGE_SIZE : WRAM_size);
    uint8_t* image = direct ? data : (data ? malloc(image_size) : WRAM);
    if (image == NULL) {
        return;
    }
    vmu_save_list();
#ifdef FRNES_VMU_BACKUP
    // (test builds that only load: keep a copy of everything on the VMUs first)
    vmu_save_backup_all("/pc/vmu_backup");
#endif
    // a save in /pc/saves (dev setup, dcload) wins over the VMU's; on the SD card, the card's only
    char path[64];
    battery_path(path, sizeof(path));
    int got = -1;
    file_t f = fs_open(path, O_RDONLY);
    if (f >= 0) {
        got = fs_read(f, image, image_size);
        fs_close(f);
        printf("SAVE loaded %d bytes from [%s]\n", got, path);
    } else if (battery_place == FRNES_BATTERY_SD) {
        printf("SAVE none at [%s]\n", path);
    } else {
        got = vmu_save_load(currentCRC32, image, image_size);
#ifdef FRNES_AUTOSAVE_TEST
        // (the test build forces a save: never over one that was there)
        autosave_test_existing = (got >= 0);
#endif
        if (got < 0) {
            printf("SAVE none on the VMU or at [%s]\n", path);
        }
    }
    if (cartridge_db_no_battery && (got <= 0)) {
        printf("SAVE off: no battery in the game database and no save to keep\n");
        battery_db_save_missing = 1;
    }
    if (direct) {
        if (got == (int)size) {
            mapper_battery_loaded();
        } else {
            // (a missing or short save: the mapper's memory as loaded from the ROM)
            mapper_battery_reset();
        }
    } else if (data) {
        if (got >= (int)(BATTERY_IMAGE_MAPPER_OFFSET + size)) {
            memcpy(data, image + BATTERY_IMAGE_MAPPER_OFFSET, size);
            mapper_battery_loaded();
            uint32_t used = 0, elsewhere = 0;
            for (uint32_t i = 0; i < (uint32_t)got; i++) {
                if (image[i]) {
                    if ((i >= BATTERY_IMAGE_MAPPER_OFFSET) && (i < BATTERY_IMAGE_MAPPER_OFFSET + size)) {
                        used++;
                    } else {
                        elsewhere++;
                    }
                }
            }
            printf("SAVE %u kB image: %lu non-zero bytes in the mapper's RAM at $%04lX, %lu elsewhere\n", got / 1024,
                (unsigned long)used, (unsigned long)(0x6000 + BATTERY_IMAGE_MAPPER_OFFSET), (unsigned long)elsewhere);
        } else if (got == (int)size) {
            memcpy(data, image, size);
            mapper_battery_loaded();
        }
        free(image);
    } else if (got > 0) {
        dcache_wback_range((uintptr_t)WRAM, WRAM_size);
    }
}

// a whole file written at 'path' (created or replaced), and a file's contents copied to another: where the file system
// has no rename (KOS's FAT driver, the SD card's), the save's .tmp / .bak steps are done this way. 0 when written
static FRNES_COLD_TEXT int save_file_write(const char* path, const void* data, uint32_t size) {
    file_t f = fs_open(path, O_WRONLY | O_TRUNC | O_CREAT);
    if (f < 0) {
        return -1;
    }
    int ok = (fs_write(f, data, size) == (ssize_t)size);
    fs_close(f);
    return ok ? 0 : -1;
}

static FRNES_COLD_TEXT int save_file_copy(const char* from, const char* to) {
    file_t f = fs_open(from, O_RDONLY);
    if (f < 0) {
        return -1;
    }
    uint32_t size = fs_total(f);
    uint8_t* data = malloc(size ? size : 1);
    int ok = data && (fs_read(f, data, size) == (ssize_t)size);
    fs_close(f);
    int result = ok ? save_file_write(to, data, size) : -1;
    free(data);
    return result;
}

// writes a snapshot to the VMU when one takes it, otherwise to /pc/saves (keeping the previous file as .bak); frees
// it. 0 when it was written
// (battery_write's places)
#define BATTERY_TO_VMU 1
#define BATTERY_TO_FILE 2

// the VMU, and /pc/saves as well when it's there (dev setup, where it's what gets loaded) and 'to' says so: the
// autosaves during a game only write the VMU (/pc's file, its .tmp, rename and backup are dc-load-ip calls that each
// stop the console for milliseconds: they made Zelda II's frames late), the save at the game's end both
__attribute__((cold, noinline)) static int battery_write(uint8_t* snapshot, uint32_t size, uint32_t to) {
    int vmu = -1;
    if (to & BATTERY_TO_VMU) {
#ifdef FRNES_AUTOSAVE_TEST
        if (autosave_test_existing) {
            printf("AUTOSAVE test: the VMU had a save for this game at start, not writing over it\n");
        } else {
            vmu = vmu_save_write(currentCRC32, snapshot, size, frnes_rom_path);
        }
#else
        vmu = vmu_save_write(currentCRC32, snapshot, size, frnes_rom_path);
#endif
    }
    if (!(to & BATTERY_TO_FILE)) {
        free(snapshot);
        return vmu;
    }
    char path[64], temporary[72];
    battery_path(path, sizeof(path));
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (battery_place == FRNES_BATTERY_SD) {
        fs_mkdir("/sd/FrNES");
    }
    fs_mkdir(battery_directory());
    if (save_file_write(temporary, snapshot, size) != 0) {
        printf("SAVE unable to write [%s], [%s] kept\n", temporary, path);
        free(snapshot);
        return vmu;
    }
    // the previous save becomes <CRC>.sav.bak, so a bad write can't lose it. (dcload's rename is a link and an
    // unlink, won't link over an existing file, and reports a failure of the unlink as a failed rename: what
    // counts is whether the save is in place. KOS's FAT driver has no rename: there the files are copied / written)
    char backup[72];
    snprintf(backup, sizeof(backup), "%s.bak", path);
    file_t previous = fs_open(path, O_RDONLY);
    if (previous >= 0) {
        fs_close(previous);
        fs_unlink(backup);
        fs_rename(path, backup);
        file_t moved = fs_open(backup, O_RDONLY);
        if ((moved < 0) && (save_file_copy(path, backup) == 0)) {
            moved = fs_open(backup, O_RDONLY);
        }
        if (moved < 0) {
            printf("SAVE unable to keep the previous save as [%s], [%s] kept, the new one is in [%s]\n", backup, path, temporary);
            free(snapshot);
            return vmu;
        }
        fs_close(moved);
        fs_unlink(path);
    }
    fs_rename(temporary, path);
    file_t check = fs_open(path, O_RDONLY);
    if ((check < 0) && (save_file_write(path, snapshot, size) == 0)) {
        check = fs_open(path, O_RDONLY);
    }
    free(snapshot);
    if (check < 0) {
        printf("SAVE rename failed, the save is in [%s]\n", temporary);
        return vmu;
    }
    fs_close(check);
    fs_unlink(temporary);
    printf("SAVE wrote %lu bytes to [%s], the previous save (if any) is in [%s]\n", (unsigned long)size, path, backup);
    // (the SD card's file system keeps it in its cache until a sync: the frontend's)
    if ((strncmp(path, "/sd/", 4) == 0) && autosave_file_sync) {
        autosave_file_sync();
    }
    return 0;
}

/**
 * Autosave: between frames, once the game has written $6000-$7FFF and then left it alone for AUTOSAVE_SETTLE_FRAMES
 * (3 s), and the RAM's CRC differs from the last save, a snapshot is handed to a low priority thread that writes
 * it (bzip2 + the VMU, or /pc/saves) while the frames go on; the main loop only waits for sound with thd_pass(), which
 * is where the thread runs. The saving indicator is on screen (and the VMU shows its saving picture) until the write
 * is done. Exiting waits for a write in flight, then saves once more if the RAM changed after it.
 */
#define AUTOSAVE_SETTLE_FRAMES 180
// (autosave_crc_offset with autosave_crc_size 0: a file to write, autosave_file_write; otherwise a VMU state's slot)
#define AUTOSAVE_FILE_JOB 0xffffffff
// (set by the frontend: sd_card_sync, which puts the SD card's file system cache on the card; NULL in the player builds.
// A cold pointer rather than a weak symbol, which LTO handles badly)
void (*autosave_file_sync)(void) FRNES_COLD_BSS;
// set when a file write failed (movie.c then writes the whole file again)
volatile int autosave_file_failed FRNES_COLD_BSS;
// the file writes done, for syncing every AUTOSAVE_FILE_SYNCS of them
#define AUTOSAVE_FILE_SYNCS 6
static uint32_t autosave_file_saves FRNES_COLD_BSS;
static semaphore_t autosave_semaphore;
static volatile int autosave_busy = 0;
static uint8_t* autosave_snapshot;
// the snapshot, and where in it the battery RAM itself is (battery_crc's bytes: the mapper's own RAM sits in an image)
static uint32_t autosave_size, autosave_crc_offset, autosave_crc_size;
static volatile uint32_t autosave_frame;
static kthread_t* autosave_kthread;
// the main loop is waiting for sound (thd_pass): the only time the autosave thread works
static volatile int autosave_main_waiting;

/*
 * KOS hands the CPU at every timer tick and thd_pass() to the first ready thread whatever its priority, so a busy
 * low priority thread took 10 ms slices out of the frames. The autosave thread works in small pieces (4 kB of
 * compression, 16 kB of CRC) and between them gives the CPU back until the main loop is waiting for sound.
 */
// (vblank pacing waits feeding the sound stream)
static void pace_idle() {
    apu_synth_poll();
}

// (pacing by the sound: the longest the frame loop waits for it to take its samples, 2 frames, when it stops taking
// them: the AICA's stream stalled. The game goes on rather than hang; said once)
#define PACE_SOUND_WAIT_MAX_US 33000
// (its text in the cold .rodata: a string literal would go among the others, moving those after it)
// (said once: its flag at the end of frame_hud_perf.c's variables, the last in .bss, so no variable moves)
extern uint8_t pace_sound_wait_gave_up;
static const char pace_sound_wait_text[] FRNES_COLD_RODATA = "PACE the sound took no samples for %u us: going on without waiting for it\n";
static FRNES_COLD_TEXT void pace_sound_wait_give_up(void) {
    if (!pace_sound_wait_gave_up) {
        pace_sound_wait_gave_up = 1;
        printf(pace_sound_wait_text, (unsigned)PACE_SOUND_WAIT_MAX_US);
    }
}

static void autosave_yield() {
    if (thd_get_current() != autosave_kthread) {
        return;
    }
    do {
        thd_pass();
    } while (!autosave_main_waiting);
}

static uint32_t autosave_crc32(const uint8_t* data, uint32_t size) {
    uint32_t crc = 0;
    for (uint32_t done = 0; done < size; done += 16384) {
        crc = input_recording_crc32_update(crc, data + done, ((size - done) < 16384) ? size - done : 16384);
        autosave_yield();
    }
    return crc;
}

static void* autosave_thread(void* arg) {
    (void)arg;
    for (;;) {
        sem_wait(&autosave_semaphore);
        uint64_t start = timer_us_gettime64();
        if ((autosave_crc_size == 0) && (autosave_crc_offset == AUTOSAVE_FILE_JOB)) {
            // a file write (autosave_file_write: input_recording_image's), a piece at a time
            const char* path = (const char*)autosave_snapshot;
            uint32_t size = autosave_size - (strlen(path) + 1);
            int written = (input_recording_write_image(autosave_snapshot, autosave_size, autosave_yield) == 0);
            if (!written) {
                autosave_file_failed = 1;
            }
            // (the SD card's file system keeps it in its cache until a sync: autosave_file_sync, the frontend's. Its
            // FAT, directory and FSInfo writes at every recording save (5 s) made a late frame each time: every
            // AUTOSAVE_FILE_SYNCS saves, so a power cut loses up to 30 s; the save at the recording's end syncs)
            if ((strncmp(path, "/sd/", 4) == 0) && autosave_file_sync && ((++autosave_file_saves % AUTOSAVE_FILE_SYNCS) == 0)) {
                autosave_file_sync();
            }
            printf("AUTOSAVE %s %lu bytes to [%s] in %lu ms\n", written ? "wrote" : "couldn't write", (unsigned long)size, path,
                (unsigned long)((timer_us_gettime64() - start) / 1000));
            free(autosave_snapshot);
            autosave_busy = 0;
            continue;
        }
        if (autosave_crc_size == 0) {
            // a save state for the VMU (autosave_state_write, its slot in autosave_crc_offset): the result is shown by
            // the indicator's next draw
            save_indicator_active = 1;
            int slot = (int)autosave_crc_offset;
            int result = vmu_state_write(currentCRC32, slot, autosave_snapshot, autosave_size, frnes_rom_path);
            free(autosave_snapshot);
            printf("STATE %d to the VMU took %lu ms (frame %lu)\n", slot, (unsigned long)((timer_us_gettime64() - start) / 1000), (unsigned long)autosave_frame);
            save_indicator_active = SAVE_INDICATOR_STATE_RESULT(slot, result == 0);
            autosave_busy = 0;
            continue;
        }
        // (the CRC here rather than between frames: an unchanged save is dropped without the indicator)
        autosave_yield();
        uint32_t crc = autosave_crc32(autosave_snapshot + autosave_crc_offset, autosave_crc_size);
        if (crc == battery_saved_crc) {
            free(autosave_snapshot);
        } else {
            save_indicator_active = 1;
            printf("AUTOSAVE the battery RAM changed and has had no writes for %u frames, saving\n", AUTOSAVE_SETTLE_FRAMES);
            // (on the SD card the file is the save, written now; with the VMU, dev setups' /pc file waits for the end)
            int to_sd = (battery_place == FRNES_BATTERY_SD);
            if (battery_write(autosave_snapshot, autosave_size, to_sd ? BATTERY_TO_FILE : BATTERY_TO_VMU) == 0) {
                battery_saved_crc = crc;
                if (to_sd) {
                    battery_file_crc = crc;
                }
            }
            printf("AUTOSAVE took %lu ms (frame %lu)\n", (unsigned long)((timer_us_gettime64() - start) / 1000), (unsigned long)autosave_frame);
        }
        save_indicator_active = 0;
        autosave_busy = 0;
    }
    return NULL;
}

// the frame of the last write to the battery RAM, and whether a save is waiting for it to settle (autosave_check)
static uint32_t autosave_last_write = 0, autosave_waiting = 0;

// the thread outlives a game: made for the first one that saves (its battery RAM, or a state on the VMU) and later
// games reuse it; 0 when there's none
__attribute__((cold, noinline)) static int autosave_thread_create() {
    if (autosave_kthread == NULL) {
        sem_init(&autosave_semaphore, 0);
        autosave_kthread = thd_create(1, autosave_thread, NULL);
        if (autosave_kthread) {
            thd_set_prio(autosave_kthread, PRIO_DEFAULT + 5);
            vmu_save_yield = autosave_yield;
        }
    }
    return autosave_kthread != NULL;
}

// waits for the thread's write in flight (it only works while the main loop says it's waiting)
__attribute__((cold, noinline)) void autosave_wait() {
    if (!autosave_busy) {
        return;
    }
    int waiting = autosave_main_waiting;
    autosave_main_waiting = 1;
    while (autosave_busy) {
        thd_sleep(10);
    }
    autosave_main_waiting = waiting;
}

// a file written by the thread while the game goes on (movie.h: a recording's periodic saves); not when it's busy
__attribute__((cold, noinline)) int autosave_file_write(uint8_t* image, uint32_t size) {
    if (autosave_busy || !autosave_thread_create()) {
        return -1;
    }
    autosave_snapshot = image;
    autosave_size = size;
    autosave_crc_offset = AUTOSAVE_FILE_JOB;
    autosave_crc_size = 0;
    autosave_busy = 1;
    sem_signal(&autosave_semaphore);
    return 0;
}

// a save state for the VMU, compressed and written by the thread while the game goes on, as the battery RAM is (after
// a write in flight); takes data. 0 when it was handed over
__attribute__((cold, noinline)) int autosave_state_write(uint8_t* data, uint32_t size, int slot) {
    autosave_wait();
    if (!autosave_thread_create()) {
        free(data);
        return -1;
    }
    autosave_snapshot = data;
    autosave_size = size;
    autosave_crc_offset = (uint32_t)slot;
    autosave_crc_size = 0;
    autosave_busy = 1;
    sem_signal(&autosave_semaphore);
    return 0;
}

__attribute__((cold, noinline)) static void autosave_start() {
    // (the file as loaded: battery_save writes it at the game's end if the RAM changed)
    battery_file_crc = battery_saved_crc;
#ifdef FRNES_NO_SAVE
    return;
#endif
    autosave_thread_create();
    autosave_last_write = autosave_waiting = 0;
    prg_ram_written = 0;
    mapper_save_written = 0;
}

// between frames: a write to $6000-$7FFF restarts the settle time; once it has passed with no more writes and the
// RAM differs from the last save, a snapshot goes to the thread
// (between frames: only a copy of the battery RAM, the FDS's save brought up to date block by block first; the CRC
// and the write are the thread's)
__attribute__((cold, noinline)) static void autosave_settled() {
#ifdef FRNES_NO_SAVE
    return;
#endif
#ifdef FRNES_AUTOSAVE_TEST
    uint64_t t0 = timer_us_gettime64();
#endif
    uint32_t raw_size;
    battery_data(&raw_size);
    autosave_snapshot = battery_snapshot(&autosave_size);
#ifdef FRNES_AUTOSAVE_TEST
    printf("AUTOSAVE test: snapshot %lu us (%lu bytes)\n", (unsigned long)(timer_us_gettime64() - t0), (unsigned long)autosave_size);
#endif
    if (autosave_snapshot == NULL) {
        return;
    }
    autosave_crc_offset = (raw_size < BATTERY_IMAGE_SIZE) ? BATTERY_IMAGE_MAPPER_OFFSET : 0;
    autosave_crc_size = raw_size;
    autosave_busy = 1;
    sem_signal(&autosave_semaphore);
}

// an input recording event (input_recording.h, 0 for none), live or replayed, applied before every frame runs
static inline void recording_event(uint16_t event) {
    if (cartridge_mapper_number != 20) {
        return;
    }
    if ((event & 0xff00) == INPUT_RECORDING_EVENT_FDS_SIDE) {
        fds_insert_side(event & 0xff);
    }
    fds_frame();
}

/*
 * FDS side changes by hand: the right trigger held alone for a second (trigger_gesture.h) puts in the next side,
 * after the last the first; the disk is out for a second (fds_insert_side) and the new side is shown on screen.
 * Not while the drive is reading or writing (DISK BUSY on screen instead).
 * Returns the change as an input recording event (0 for none).
 */
static uint16_t fds_hand_change() {
    int side = fds_next_side();
    if (side < 0) {
        return 0;
    }
    if (fds_drive_busy()) {
        frame_hud_message("DISK BUSY", FDS_MESSAGE_FRAMES);
        return 0;
    }
    char message[16];
    snprintf(message, sizeof(message), "DISK %ld SIDE %c", (long)(side / 2 + 1), 'A' + (int)(side & 1));
    frame_hud_message(message, FDS_MESSAGE_FRAMES);
    return INPUT_RECORDING_EVENT_FDS_SIDE | side;
}

static inline void autosave_check(uint32_t frame) {
    autosave_frame = frame;
    // (a board saving its own memory, the FDS's disk sides, says when it changed: its PRG RAM isn't the save)
    volatile uint8_t* written = board_saves ? &mapper_save_written : &prg_ram_written;
    if (*written) {
        *written = 0;
        autosave_last_write = frame;
        autosave_waiting = 1;
    } else if (autosave_waiting && !autosave_busy && ((frame - autosave_last_write) >= AUTOSAVE_SETTLE_FRAMES)) {
        autosave_waiting = 0;
        autosave_settled();
    }
}

// on exit: waits for a write in flight, then saves if the RAM changed since the last save
__attribute__((cold, noinline)) static void battery_save(uint32_t frames) {
#ifdef FRNES_NO_SAVE
    printf("SAVE not written: built with FRNES_NO_SAVE\n");
    return;
#endif
    autosave_wait();
    if (frames < SAVE_MIN_FRAMES) {
        printf("SAVE not written: the game only ran %lu frames\n", (unsigned long)frames);
        return;
    }
    uint32_t crc = battery_crc();
    // (the VMU is up to date after an autosave; the file only gets written here)
    uint32_t to = ((crc != battery_saved_crc) && (battery_place != FRNES_BATTERY_SD) ? BATTERY_TO_VMU : 0) |
        ((crc != battery_file_crc) ? BATTERY_TO_FILE : 0);
    if (!to) {
        printf("SAVE not written: the battery RAM didn't change since it was loaded or saved\n");
        return;
    }
    uint32_t size;
    uint8_t* snapshot = battery_snapshot(&size);
    if (snapshot == NULL) {
        printf("SAVE out of memory\n");
        return;
    }
    if (battery_write(snapshot, size, to) == 0) {
        battery_saved_crc = battery_file_crc = crc;
    }
}

#if defined(FRNES_RECORD) || defined(FRNES_PLAYBACK_PATH)
/*
 * Record builds (dev, /pc is dc-load-ip's): the recording goes to /pc every RECORDING_FLUSH_FRAMES frames (written to
 * <name>.tmp, then put in place), so a hang or crash leaves it usable; the first write picks the file's name. Record
 * and playback builds: a watchdog (TMU1, twice a second) and the fatal exception handlers print where the SH4 and the
 * 6502 were when the frames stopped coming, then go back to dc-load-ip.
 */
#define RECORD_WATCHDOG_SECONDS 5
#if defined(FRNES_PLAYBACK_PATH) && defined(FRNES_MOVIE_HASH)
/*
 * MOVIE_HASH=1 playback builds: each frame's hash of the 2 kB of RAM and the CPU's registers at the frame's end, as the
 * replay test builds take it (test_main.c's ac_frame_hash, their AC FS lines), kept in memory and printed after the
 * movie as MOVIE HASH <first frame> <16 hashes> lines: the player's frame loop playing a movie against the replays'
 */
#define MOVIE_HASH_RAM ((const uint8_t*)RAM_ADDR)
static FRNES_COLD_TEXT uint32_t movie_frame_hash(void) {
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < 0x800; i++) {
        hash = (hash ^ MOVIE_HASH_RAM[i]) * 16777619u;
    }
    hash = (hash ^ cpu_registers -> PC) * 16777619u;
    hash = (hash ^ cpu_registers -> A) * 16777619u;
    hash = (hash ^ cpu_registers -> X) * 16777619u;
    hash = (hash ^ cpu_registers -> Y) * 16777619u;
    hash = (hash ^ cpu_registers -> S) * 16777619u;
    hash = (hash ^ cpu_registers -> SP) * 16777619u;
    return hash;
}

static FRNES_COLD_TEXT void movie_hash_print(const uint32_t* hashes, uint32_t count) {
    for (uint32_t first = 0; first < count; first += 16) {
        char line[200];
        int length = snprintf(line, sizeof(line), "MOVIE HASH %lu", (unsigned long)(first + 1));
        for (uint32_t i = first; (i < first + 16) && (i < count); i++) {
            length += snprintf(line + length, sizeof(line) - length, " %08lx", (unsigned long)hashes[i]);
        }
        printf("%s\n", line);
    }
}
#endif

static volatile uint32_t record_watchdog_frame;
static uint32_t record_watchdog_last, record_watchdog_ticks;
static volatile int record_fatal_entered;

static void record_breadcrumb(const char* what, irq_t code, irq_context_t* ctx) {
    dbgio_enable();
    printf("%s evt=%03lx frame=%lu pc=%08lx pr=%08lx sr=%08lx 6502pc=%04X mapper=%lu thd=%08lx\n", what, (unsigned long)code,
        (unsigned long)record_watchdog_frame, (unsigned long)ctx -> pc, (unsigned long)ctx -> pr, (unsigned long)ctx -> sr,
        cpu_registers -> PC, (unsigned long)cartridge_mapper_number, (unsigned long)thd_current);
    for (uint32_t i = 0; i < 16; i += 4) {
        printf("%s r%lu-r%lu %08lx %08lx %08lx %08lx\n", what, (unsigned long)i, (unsigned long)(i + 3), (unsigned long)ctx -> r[i],
            (unsigned long)ctx -> r[i + 1], (unsigned long)ctx -> r[i + 2], (unsigned long)ctx -> r[i + 3]);
    }
}

static void record_fatal_handler(irq_t code, irq_context_t* ctx, void* data) {
    (void)data;
    if (!record_fatal_entered) {
        record_fatal_entered = 1;
        record_breadcrumb("FATAL", code, ctx);
        apu_synth_silence();
    }
    arch_real_exit(1);
}

/*
 * HW_WATCHDOG=1 builds also arm the SH4's watchdog to reset the console. At its largest divider (4096) it overflows
 * in a few milliseconds (KOS's comments say 1.34 s: kicked once a frame it reset the console at once, and kicked at
 * 100 Hz still during start-up), so TMU1 kicks it at 1000 Hz while the frames come. A hang with interrupts masked
 * stops TMU1 too, and the reset follows; dc-load-ip comes back and tools/crumbs.py reads where the frame was (video.h).
 */
#ifdef FRNES_HW_WATCHDOG
#define RECORD_WATCHDOG_HZ 1000
#else
#define RECORD_WATCHDOG_HZ 2
#endif

static void record_watchdog_handler(irq_t code, irq_context_t* ctx, void* data) {
    (void)data;
    timer_clear(TMU1);
    if (record_watchdog_frame != record_watchdog_last) {
        record_watchdog_last = record_watchdog_frame;
        record_watchdog_ticks = 0;
    } else {
        ++record_watchdog_ticks;
    }
#ifdef FRNES_HW_WATCHDOG
    if (record_watchdog_ticks < RECORD_WATCHDOG_SECONDS * RECORD_WATCHDOG_HZ) {
        wdt_pet();
    }
#endif
    // (a few samples of where it's stuck before giving up)
    if (record_watchdog_ticks >= RECORD_WATCHDOG_SECONDS * RECORD_WATCHDOG_HZ) {
        if (((record_watchdog_ticks - RECORD_WATCHDOG_SECONDS * RECORD_WATCHDOG_HZ) % (RECORD_WATCHDOG_HZ / 2)) == 0) {
            record_breadcrumb("WATCHDOG", code, ctx);
        }
    }
    if (record_watchdog_ticks >= (RECORD_WATCHDOG_SECONDS + 2) * RECORD_WATCHDOG_HZ) {
        timer_stop(TMU1);
        apu_synth_silence();
        arch_real_exit(1);
    }
}

static void record_debug_start() {
    static const irq_t codes[] = { EXC_DATA_ADDRESS_READ, EXC_DATA_ADDRESS_WRITE, EXC_ILLEGAL_INSTR, EXC_SLOT_ILLEGAL_INSTR,
        EXC_DTLB_MISS_READ, EXC_DTLB_MISS_WRITE, EXC_FPU, EXC_DOUBLE_FAULT };
    for (uint32_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
        irq_set_handler(codes[i], record_fatal_handler, NULL);
    }
    irq_set_handler(EXC_TMU1_TUNI1, record_watchdog_handler, NULL);
    timer_prime(TMU1, RECORD_WATCHDOG_HZ, 1);
    timer_clear(TMU1);
    timer_start(TMU1);
#ifdef FRNES_HW_WATCHDOG
    wdt_enable_watchdog(0, WDT_CLK_DIV_4096, WDT_RST_MANUAL);
#endif
}
#endif

/*
 * The console goes quiet while the game runs: started through dc-tool-ip, printf is a dc-load-ip syscall over the
 * network that now and then never returns, and the game freezes with the sound stopped (the MM3 freezes on BLUE and
 * YELLOW, ~8700 and ~10200 frames in: the FPS line every 300 frames; two reproduced on RED with HW_WATCHDOG and
 * breadcrumbs around it). Everything printed between start-up and exit (the FPS and HUD lines, FDS disk events, mapper
 * messages) is dropped; FPS_LOG=1 builds keep the console on. (CD builds have no dc-load-ip: nothing changes there.)
 */
#if !defined(FRNES_FPS_LOG) && !defined(FRNES_AUTOSAVE_TEST) && !defined(FRNES_HANG_TEST)
#define CONSOLE_QUIET() dbgio_disable()
#define CONSOLE_ON() dbgio_enable()
#else
#define CONSOLE_QUIET() ((void)0)
#define CONSOLE_ON() ((void)0)
#endif


/*
 * The movie paths of frnes_run_movie, cold and out of line: with a movie in play the frame loop's share is a test of the
 * mode and one call a frame. Inlined, the frontend's frnes_run_movie (its menus pass a movie or a recording directory,
 * so nothing folds away) was 0x2b0 bytes larger and with apu_synth_frame over the main slot's 0x4000
 */
// the movie or the recording to start (from power on; movie.h), 0 unless the movie can't be played
static FRNES_COLD_TEXT int movie_start(movie_t* movie, const char* movie_path, const char* record_directory) {
    memset(movie, 0, sizeof(*movie));
    if (movie_path) {
        return movie_open(movie, movie_path, currentCRC32);
    }
    if (record_directory) {
        // (the game's path: frnes_rom_path, set by frnes_run_movie, so its call here stays as it was)
        movie_record_begin(movie, currentCRC32, record_directory, frnes_rom_path);
    }
    return 0;
}

// a playing movie's input for frame 'input_frame': the pads latched and the event applied; 1 when the loop has to stop
// (the movie has run out, or A+B+X+Y+Start on the controller, which a movie doesn't otherwise need)
static FRNES_COLD_TEXT int movie_frame_input(movie_t* movie, uint32_t input_frame, const cont_state_t* state) {
    int stop = state && ((state -> buttons & EXIT_BUTTONS) == EXIT_BUTTONS);
    uint8_t pad1, pad2;
    uint16_t event;
    if (!movie_input(movie, input_frame, &pad1, &pad2, &event)) {
        CONSOLE_ON();
        stop = 1;
    }
    cpu_registers -> PAD1_Latch = pad1;
    cpu_registers -> PAD2_Latch = pad2;
    recording_event(event);
    return stop;
}

// a recording's frame: the pads as the game takes them and the event (both triggers held a second stop it)
static FRNES_COLD_TEXT void movie_record_input(movie_t* movie, uint32_t frame, uint16_t event, const cont_state_t* state) {
    if (movie_record_frame(movie, frame, cpu_registers -> PAD1_Latch, cpu_registers -> PAD2_Latch, event, state -> ltrig,
        state -> rtrig)) {
        frame_hud_message("RECORDING STOPPED", 120);
    }
}

/*
 * Plays a ROM (.nes, or .fds with disksys.rom) until the exit combination, then shuts the emulator down and returns,
 * so it can be called again for another game. The PVR is the emulator's while it runs: video_init() starts it and
 * video_shutdown() stops it, so a caller that draws with the PVR shuts it down first and starts it again after.
 * Returns 0, or -1 if the ROM couldn't be loaded.
 *
 * The emulation loop runs once per batch: in the hot code group, last (zz), so that code linked elsewhere doesn't
 * move it in the instruction cache
 */
FRNES_HOT(zz_main) __attribute__((noinline)) int frnes_run_movie(const char* rom_path, const char* movie_path, const char* record_directory) {
    frnes_rom_path = rom_path;
    battery_backed_cache = -1;
    battery_db_save_missing = 0;
    if (initialize_frnes(rom_path) == -1) {
        printf("AN ERROR OCCURRED INITIALIZING EMULATOR\n");
        shutdown_frnes();
        return -1;
    }
    // Game Genie codes, on the ROM now it's loaded and mapped (the bytes they changed kept, to take them off again)
    gamegenie_apply(frnes_cheats(), &gamegenie_undo);
    // a movie (movie.h) in place of the pads, or the pads recorded into one: from power on, so the game's battery RAM
    // isn't loaded, nor saved after
    movie_t movie;
    if (movie_start(&movie, movie_path, record_directory) != 0) {
        gamegenie_free(&gamegenie_undo);
        shutdown_frnes();
        return 0;
    }
    const int movie_playing = (movie.mode == MOVIE_PLAYING);
    const int movie_from_power_on = (movie.mode != MOVIE_OFF);
    if (!movie_from_power_on && battery_backed()) {
        battery_load();
        battery_saved_crc = battery_crc();
        autosave_start();
    }

#if defined(FRNES_PLAYBACK_PATH) && defined(FRNES_MOVIE_HASH)
    uint32_t movie_hash_count = 0;
    const uint32_t movie_hash_capacity = movie_playing ? movie.recording.header.num_frames + 16 : 0;
    uint32_t* movie_hashes = movie_playing ? malloc(movie_hash_capacity * sizeof(uint32_t)) : NULL;
#ifdef FRNES_PERF_EVENT
    // (PERF_EVENT=0x.. measurement builds: that SH4 performance counter event counted on PRFC1 over each frame's
    // work, the pacing's waits left out, printed after the movie as MOVIE PERF lines like the MOVIE HASH ones:
    // tools/simplay's replay against the console)
    uint32_t* movie_perf = movie_playing ? malloc(movie_hash_capacity * sizeof(uint32_t)) : NULL;
    uint64_t movie_perf_last = 0;
    perf_cntr_start(PRFC1, FRNES_PERF_EVENT, PMCR_COUNT_CPU_CYCLES);
#endif
#endif

    video_picture_update(&frnes_settings);
    video_pacing = (frnes_settings.pacing == FRNES_PACING_VBLANK) ? PACE_VBLANK : PACE_AUDIO;
    video_translucent_overlay = 1;
    if (video_init() == -1) {
        printf("AN ERROR OCCURRED INITIALIZING VIDEO\n");
    }
    frame_hud_message_reset();
    video_vblank_start();
    if (save_indicator_init() != 0) {
        printf("SAVE indicator: out of VRAM\n");
    }
#if FRNES_FRAME_HUD
    if (frame_hud_init() != 0) {
        printf("HUD: out of VRAM\n");
    }
    frame_hud_set_mode(frnes_settings.hud_mode);
    uint64_t hud_emu_start_us = timer_us_gettime64(), hud_frame_start_us = hud_emu_start_us;
    frame_hud_emu_begin(0);
    uint32_t hud_toggle_frames = 0;
#endif
    // (how to get back to the menus, for the game's first seconds)
    frame_hud_message(state_start_message(movie_playing), 240);
    trigger_gesture_t right_trigger = { 0 };
    // the save / load state hotkeys (state.h): the ones held, and the one pressed, done at the frame's end
    uint8_t state_hotkeys_held = state_hotkeys_held_start(), state_action = 0;
#ifdef FRNES_SOUND_OFF_TEST
    // (SOUND_OFF_TEST=1 player builds: the Sound Off option's timer pacing, without the frontend)
    apu_synth_set_sound_enabled(0);
#endif
    if (apu_synth_init() == -1) {
        printf("AN ERROR OCCURRED INITIALIZING SOUND\n");
    }
#if defined(FRNES_RECORD) || defined(FRNES_PLAYBACK_PATH)
    // (after the sound's start-up, which spins on TMU1)
    record_debug_start();
#endif
    video_frame_begin();

    uint32 cycles = 0;
    // when the pads take the controllers' state: at vblank (scanline 241, as Mesen 2), or at the frame's end for
    // recordings made before that (input_recording.h's INPUT_RECORDING_FLAG_VBLANK)
    int input_at_vblank = 1, input_due = 0, vblank_input_done = 0;
    if (movie_playing) {
        input_at_vblank = movie_input_at_vblank(&movie);
    }
    // the NES ports' controllers by their sources (input.h: by default the first and second found), the others'
    // looked up for input_poll_ports. With none on NES port 1 (the Controls page's ports moved), the hotkeys and the
    // exit buttons are the first controller's and NES port 1's pad stays released (input_ports_start)
    maple_device_t* my_controller = input_first_device();
    input_ports_start();
    int exitLoop = 0;
    uint32_t frames = 0;
    uint32_t skipped = 0;
    uint64_t wait_us = 0;
    // frame pacing (video.h): video_pacing can change at any time (the frontend's menu); PAL always paces to the sound
    int pace_vblank = 0;
    uint32_t frame_vblank = video_vblank_count, late_frames = 0;
    uint64_t fps_start_us = timer_us_gettime64();
#if !defined(FRNES_FPS_LOG) && !defined(FRNES_AUTOSAVE_TEST) && !defined(FRNES_HANG_TEST)
    printf("PLAYER running (the console is quiet until exit; FPS_LOG=1 builds print the FPS lines)\n");
#endif
    CONSOLE_QUIET();
    do {
        // run the CPU up to the next PPU / APU event, then bring the PPU and APU up to date
        video_crumb(CRUMB_CPU);
        cycles = cpu_run(cpu_batch_budget());
        if (cycles == CPU_HALTED) {
            CONSOLE_ON();
            printf("UNIMPLEMENTED OPCODE [$%02x] AT [$%04x]\n", read_byte(cpu_registers -> PC), cpu_registers -> PC);
            break;
        }
        video_crumb(CRUMB_DEVICES);
        cpu_run_devices();

        // poll the controller once per emulated frame
        if (ppu_take_frame()) {
            // present the finished frame if the PVR can take it, otherwise skip it and keep emulating (never wait
            // for vsync, the texture ring already prevents tearing)
#if FRNES_FRAME_HUD
            uint64_t hud_emu_end_us = timer_us_gettime64();
            hud_perf_end();
#endif
            video_crumb(CRUMB_SYNTH);
            apu_synth_frame(cpu_cycle_total);
            if (__builtin_expect(state_action != 0, 0)) {
                state_action = state_hotkey_run(state_action);
                // (the game menu's Exit)
                if (state_action == STATE_HOTKEY_EXIT) {
                    exitLoop = 1;
                    state_action = 0;
                }
            }
#if FRNES_FRAME_HUD
            uint64_t hud_synth_end_us = timer_us_gettime64();
#endif
            video_crumb(CRUMB_FRAME_END);
            // (late: the vblank this frame was due for has passed before it is handed to the PVR)
            uint32_t submit_vblank = video_vblank_count;
            int presented = video_frame_end(0);
            if (!presented) {
                skipped++;
            }
            // the store queues are free between frames: feed the AICA stream, and while more than 2 frames of
            // sound are buffered, keep feeding it rather than running ahead (paces emulation to 60 fps)
#if defined(FRNES_PLAYBACK_PATH) && defined(FRNES_MOVIE_HASH) && defined(FRNES_PERF_EVENT)
            perf_cntr_stop(PRFC1);
#endif
            uint64_t wait_start_us = timer_us_gettime64();
            video_crumb(CRUMB_SOUND_WAIT);
            autosave_main_waiting = 1;
#if FRNES_FRAME_HUD
            frame_hud_wait_begin();
#endif
            int pace_now = (video_pacing == PACE_VBLANK) && (video_refresh_hz() > 55.0f);
            if (pace_now != pace_vblank) {
                pace_vblank = pace_now;
                frame_vblank = video_vblank_count;
                if (!pace_vblank) {
                    apu_synth_pace(0.0f);
                }
            }
            if (pace_vblank) {
                // one frame per refresh: wait for the vblank after the one this frame started on, feeding the sound
                // meanwhile (one already past: the frame was late, its predecessor stays up another refresh); the
                // sound's rate follows the display's
                if (submit_vblank != frame_vblank) {
                    late_frames++;
#ifdef FRNES_PACE_LOG
                    printf("LATE frame %lu vblank %lu (started on %lu)\n", (unsigned long)frames, (unsigned long)video_vblank_count, (unsigned long)frame_vblank);
#endif
                }
                // (the sound is fed at least once a frame, also when the frame was late and there's no wait)
                apu_synth_poll();
                frame_vblank = video_wait_vblank(frame_vblank, pace_idle);
                apu_synth_pace(video_refresh_hz());
            } else {
                while (apu_synth_poll() > (APU_SYNTH_SAMPLE_RATE / 30)) {
                    thd_pass();
                    if (timer_us_gettime64() - wait_start_us > PACE_SOUND_WAIT_MAX_US) {
                        pace_sound_wait_give_up();
                        break;
                    }
                }
            }
#if FRNES_FRAME_HUD
            frame_hud_wait_end();
#endif
            autosave_main_waiting = 0;
            video_crumb(CRUMB_AFTER_WAIT);
            uint64_t wait_end_us = timer_us_gettime64();
            wait_us += wait_end_us - wait_start_us;
#if defined(FRNES_PLAYBACK_PATH) && defined(FRNES_MOVIE_HASH) && defined(FRNES_PERF_EVENT)
            perf_cntr_resume(PRFC1);
#endif
#if FRNES_FRAME_HUD
            {
                // (the work: from the end of the last frame's wait for sound, so the rest takes in what runs between
                // frames too)
                hud_times_t times;
                uint32_t work = (uint32_t)(wait_start_us - hud_frame_start_us);
                uint32_t synth = (uint32_t)(hud_synth_end_us - hud_emu_end_us);
                times.emu_us = (uint32_t)(hud_emu_end_us - hud_emu_start_us);
                times.expansion_us = apu_synth_expansion_us();
                if (times.expansion_us > synth) {
                    times.expansion_us = synth;
                }
                times.synth_us = synth - times.expansion_us;
                times.other_us = (work > times.emu_us + synth) ? work - times.emu_us - synth : 0;
                times.wait_us = (uint32_t)(wait_end_us - wait_start_us);
                frame_hud_record(&times,
                    (presented ? 0 : HUD_NOT_PRESENTED) | ((apu_synth_buffered() < APU_SYNTH_SAMPLE_RATE / 60) ? HUD_SOUND_LOW : 0),
                    wait_end_us - hud_frame_start_us);
#ifdef FRNES_AUTOSAVE_TEST
                // (the frames around the forced save, and any long one while it's written)
                if (((frames + 5 >= FRNES_AUTOSAVE_TEST) && (frames <= FRNES_AUTOSAVE_TEST + 5)) || (autosave_busy && (work > 16000))) {
                    printf("AST frame %lu work %lu emu %lu synth %lu other %lu wait %lu busy %d\n", (unsigned long)frames,
                        (unsigned long)work, (unsigned long)times.emu_us, (unsigned long)synth, (unsigned long)times.other_us,
                        (unsigned long)times.wait_us, autosave_busy);
                }
#endif
            }
            hud_frame_start_us = wait_end_us;
#endif
#if !defined(FRNES_PLAYBACK_PATH)
            if (!movie_from_power_on && battery_backed()) {
                autosave_check(frames);
            }
#ifdef FRNES_AUTOSAVE_TEST
            // test builds: an autosave at this frame whatever the game did, to measure its cost
            if (battery_backed() && (frames == FRNES_AUTOSAVE_TEST)) {
                battery_saved_crc = ~battery_crc();
                autosave_settled();
            }
#endif
#endif
            // a movie's desync check (its .frc checkpoints: movie.c), at this frame's end
            if (__builtin_expect(movie.checkpoint_count != 0, 0)) {
                movie_checkpoint(&movie, frames + 1);
            }
#if defined(FRNES_PLAYBACK_PATH) && defined(FRNES_MOVIE_HASH)
            // (frame 'frames' + 1's, the replays' numbering)
            if (movie_hashes && (movie_hash_count < movie_hash_capacity)) {
#ifdef FRNES_PERF_EVENT
                if (movie_perf) {
                    uint64_t now = perf_cntr_count(PRFC1);
                    movie_perf[movie_hash_count] = (uint32_t)(now - movie_perf_last);
                    movie_perf_last = now;
                }
#endif
                movie_hashes[movie_hash_count++] = movie_frame_hash();
            }
#endif
            if ((++frames % FPS_INTERVAL) == 0) {
                uint64_t now_us = timer_us_gettime64();
#ifndef FRNES_NO_FPS_PRINT
                printf("FPS %.2f (waiting for %s %.1f%% of the time, %u of %u frames not presented, %u late, %u samples buffered)\n",
                    FPS_INTERVAL * 1000000.0 / (double)(now_us - fps_start_us), pace_vblank ? "vblank" : "sound",
                    100.0 * (double)wait_us / (double)(now_us - fps_start_us),
                    (unsigned)skipped, (unsigned)FPS_INTERVAL, (unsigned)late_frames, (unsigned)apu_synth_buffered());
#endif
                fps_start_us = now_us;
                wait_us = 0;
                skipped = 0;
                late_frames = 0;
            }
            video_crumb_frame = frames;
            video_crumb(CRUMB_FRAME_BEGIN);
#ifdef FRNES_HANG_TEST
            // (a hang with interrupts masked at that frame: the SH4 watchdog has to reset the console)
            if (frames == FRNES_HANG_TEST) {
                printf("HANG TEST: interrupts masked, spinning at frame %lu\n", (unsigned long)frames);
                irq_disable();
                for (;;) {
                    __asm__ volatile("nop");
                }
            }
#endif
            video_frame_begin();
#if FRNES_FRAME_HUD
            hud_emu_start_us = timer_us_gettime64();
            frame_hud_emu_begin(frames);
#endif

            // (the pads take the controllers' state at vblank, as Mesen 2 does, or for recordings made before that at
            // the frame's end: see input_at_vblank)
            input_due = !input_at_vblank;
            vblank_input_done = 0;
        }
        if (input_at_vblank && !vblank_input_done && (ppu_current_scanline() >= 241) && (ppu_current_scanline() < 261)) {
            input_due = 1;
            vblank_input_done = 1;
        }
        if (input_due) {
            input_due = 0;
            // (the frame these pads are for: at vblank, the next one; at the frame's end, 'frames' has already moved on)
            uint32_t input_frame = frames + (input_at_vblank ? 1 : 0);
            (void)input_frame;
            video_crumb(CRUMB_INPUT);
            cont_state_t* my_state = my_controller ? (cont_state_t*)maple_dev_status(my_controller) : NULL;
            video_crumb(CRUMB_FRAME_DONE);
            // a movie plays without a controller (one only for exiting early)
            if (__builtin_expect(movie_playing, 0)) {
                if (movie_frame_input(&movie, input_frame, my_state)) {
                    exitLoop = 1;
                }
#if defined(FRNES_RECORD) || defined(FRNES_PLAYBACK_PATH)
                record_watchdog_frame = frames;
#endif
                continue;
            }
#if defined(FRNES_AUTOSAVE_TEST)
            // (nor does the autosave test: no input)
            cont_state_t no_buttons;
            if (my_state == NULL) {
                memset(&no_buttons, 0, sizeof(no_buttons));
                my_state = &no_buttons;
            }
#endif
            if (my_state == NULL) {
                exitLoop = 1;
            } else {
                // FDS games: the right trigger held changes the disk side, tapped the frame display's mode
                uint16_t event = 0;
                uint32_t gesture = trigger_gesture(&right_trigger, my_state -> rtrig, my_state -> ltrig);
                if (cartridge_mapper_number == 20) {
                    // (the flip disk hotkey: the right trigger held, or the frontend's choice, state_hotkeys.c)
                    if (state_flip_requested(my_state -> buttons, my_state -> ltrig, my_state -> rtrig, gesture)) {
                        event = fds_hand_change();
#if FRNES_FRAME_HUD
                    } else if (gesture == TRIGGER_TAP) {
                        frame_hud_cycle_mode();
#endif
                    }
                }
                recording_event(event);
#if FRNES_FRAME_HUD
                // other games: the right trigger alone (the save gesture is both), held for a second: next frame
                // display mode
                if ((cartridge_mapper_number != 20) && (my_state -> rtrig > 200) && (my_state -> ltrig < RIGHT_TRIGGER_IDLE)) {
                    if (++hud_toggle_frames == HUD_TOGGLE_FRAMES) {
                        frame_hud_cycle_mode();
                    }
                } else {
                    hud_toggle_frames = 0;
                }
#endif
                // exit on the Dreamcast soft reset combination A+B+X+Y+Start
                if ((my_state -> buttons & EXIT_BUTTONS) == EXIT_BUTTONS) {
                    exitLoop = 1;
                }
                // (every hotkey has a trigger pressed past half way: only then, or to see one let go, is it looked at)
                if (__builtin_expect(((my_state -> ltrig | my_state -> rtrig) & 0x80) | state_hotkeys_held, 0)) {
                    uint8_t action = state_hotkeys(my_state -> buttons, my_state -> ltrig, my_state -> rtrig, &state_hotkeys_held);
                    if (action) {
                        state_action = action;
                    }
                }
#ifdef FRNES_MAX_FRAMES
                // test builds: stop after a set number of frames
                if (frames >= FRNES_MAX_FRAMES) {
                    exitLoop = 1;
                }
#endif
                // the pads through their mappings (frnes_settings): by default A = A, X (or B) = B, Y or the left
                // trigger = Select, Start = Start
                input_poll_ports(my_state, frnes_settings.pads, frames);
                // the Vs. System: coin 1 (VS_COIN_PRESSED), and player 1's pad on $4017 for the games that read it there
                if (__builtin_expect(cpu_registers -> VS_Flags, 0)) {
                    event |= vs_frame_input(VS_COIN_PRESSED(my_state));
                }
                if (__builtin_expect(movie.mode == MOVIE_RECORDING, 0)) {
                    movie_record_input(&movie, frames, event, my_state);
                }
#if defined(FRNES_RECORD) || defined(FRNES_PLAYBACK_PATH)
                record_watchdog_frame = frames;
#endif
            }
        }
    } while (!exitLoop);
    CONSOLE_ON();
#if defined(FRNES_PLAYBACK_PATH) && defined(FRNES_MOVIE_HASH)
    if (movie_hashes) {
        movie_hash_print(movie_hashes, movie_hash_count);
        free(movie_hashes);
    }
#ifdef FRNES_PERF_EVENT
    if (movie_perf) {
        for (uint32_t first = 0; first < movie_hash_count; first += 16) {
            char line[220];
            int length = snprintf(line, sizeof(line), "MOVIE PERF %lu", (unsigned long)(first + 1));
            for (uint32_t i = first; (i < first + 16) && (i < movie_hash_count); i++) {
                length += snprintf(line + length, sizeof(line) - length, " %lu", (unsigned long)movie_perf[i]);
            }
            printf("%s\n", line);
        }
        free(movie_perf);
    }
#endif
#endif
#if defined(FRNES_RECORD) || defined(FRNES_PLAYBACK_PATH)
    // (the exit's saves take a while and the sound's shutdown spins on TMU1)
#ifdef FRNES_HW_WATCHDOG
    wdt_disable();
#endif
    timer_stop(TMU1);
    timer_disable_ints(TMU1);
#endif

#if !defined(FRNES_PLAYBACK_PATH)
    // the saving indicator on the last frame while the exit save runs
    if (autosave_busy || (!movie_from_power_on && battery_backed() && (frames >= SAVE_MIN_FRAMES) && (battery_crc() != battery_saved_crc))) {
        save_indicator_active = 1;
    }
#endif
    video_frame_end(1);
    // (the sound stops first: the saves take a while and the AICA would keep looping its last buffer)
    apu_synth_shutdown();
#if !defined(FRNES_PLAYBACK_PATH)
    // (a save state still going to the VMU as well)
    autosave_wait();
    if (!movie_from_power_on && battery_backed()) {
        battery_save(frames);
    }
    save_indicator_active = 0;
#endif
    video_shutdown();
    movie_close(&movie);
    gamegenie_free(&gamegenie_undo);
    shutdown_frnes();
    return 0;
}

// a game with the pads (the player builds' PLAYBACK=movie: that movie)
#if defined(FRNES_PLAYBACK_PATH)
#define FRNES_DEFAULT_MOVIE FRNES_PLAYBACK_PATH
#else
#define FRNES_DEFAULT_MOVIE NULL
#endif
// (record builds: the pads recorded into /pc)
#if defined(FRNES_RECORD)
#define FRNES_DEFAULT_RECORDING RECORDING_DIRECTORY
#else
#define FRNES_DEFAULT_RECORDING NULL
#endif
// (not FRNES_COLD_TEXT: GCC would take the frame loop it calls, and what that calls, for code that runs once and move
// them to .text.unlikely, out of the hot slots)
int frnes_run(const char* rom_path) {
    return frnes_run_movie(rom_path, FRNES_DEFAULT_MOVIE, FRNES_DEFAULT_RECORDING);
}
