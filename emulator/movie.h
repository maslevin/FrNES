#ifndef __MOVIE_H
#define __MOVIE_H

#include "input_recording.h"

/*
 * Movies: a game played from power on with its controller input taken from a recording instead of the pads
 * (input_recording.h's .frm; FCEUX .fm2 and BizHawk .bk2 imported into the same runs). frnes_run_movie plays one; the
 * state lives on frnes_run's stack (no new .bss: perf pins the heap's start) and every function here is cold, the
 * frame loop's only cost being its per-frame test of the mode.
 */

typedef enum {
    MOVIE_OFF = 0,
    MOVIE_PLAYING,
    MOVIE_ENDED,
    MOVIE_RECORDING,
    MOVIE_RECORDED,
} movie_mode_t;

typedef struct {
    movie_mode_t mode;
    input_recording_t recording;
    // frames played, and when the movie started (for the end's frames per second)
    uint32_t frames;
    uint64_t start_us;
    // playing: the movie; recording: its file (picked at the first save, in 'directory': <ROM CRC32>_<n>.frm)
    char path[160];
    char directory[96];
    // recording: the game's name and path, written with it (INPUT_RECORDING_FLAG_ROM_INFO)
    input_recording_rom_info_t rom;
    // recording: the runs on file (handed to the save thread): later saves only add the runs after them
    uint32_t saved_runs;
    // recording: frames both triggers have been held (held for MOVIE_STOP_FRAMES: the recording stops)
    uint32_t trigger_frames;
    // playing with checkpoints (the movie's .frc, tools/frc_make.py: a known-good run's RAM hash every so many frames):
    // { frame, hash } pairs, the next one due, the RAM addresses left out, and the first frame that differed (0: none)
    uint32_t* checkpoints;
    uint32_t checkpoint_count, checkpoint_next;
    uint8_t checkpoint_ignore[256];
    uint32_t desync_frame;
} movie_t;

// a recording's stop gesture (both triggers held), and how often it's written while it runs (a hang or a crash
// leaves it usable)
#define MOVIE_STOP_FRAMES 60
#define MOVIE_SAVE_FRAMES 300

// a .frm movie for the ROM with this CRC32 (0: any), from power on; 0 when it can be played
int movie_open(movie_t* movie, const char* path, uint32_t rom_crc32);

// the desync check at the end of frame 'frame' (numbered as the replays' hashes: 1 = the first frame's end), when the
// movie has checkpoints; frnes_run_movie calls it only then (movie -> checkpoint_count)
void movie_checkpoint(movie_t* movie, uint32_t frame);

// frame 'frame''s pads and event; 0 once the movie has run out (the pads then released, the mode MOVIE_ENDED)
int movie_input(movie_t* movie, uint32_t frame, uint8_t* pad1, uint8_t* pad2, uint16_t* event);

// whether the movie's pads are latched at vblank (version 3, INPUT_RECORDING_FLAG_VBLANK) or at the frame's end
int movie_input_at_vblank(const movie_t* movie);

// recording from power on, into a new .frm in 'directory' (made if missing), which names the game played (rom_path);
// frame 0 runs with nothing pressed
int movie_record_begin(movie_t* movie, uint32_t rom_crc32, const char* directory, const char* rom_path);

// the next frame's pads and event as the game takes them, and the triggers (both held: the stop gesture); writes the
// recording every MOVIE_SAVE_FRAMES frames; 1 when this frame stopped it
int movie_record_frame(movie_t* movie, uint32_t frame, uint8_t pad1, uint8_t pad2, uint16_t event, uint8_t ltrig,
    uint8_t rtrig);

// the recording written now: the runs since the last save and the header, or the whole file; 0 when it was
int movie_record_save(movie_t* movie);
// the recording written by the save thread while the game goes on (the periodic saves), unless it's busy
void movie_record_save_background(movie_t* movie);
// (main.c) a file written by the save thread: 'image' is input_recording_image's (taken: freed once written);
// -1 (image not taken) while the thread is busy
int autosave_file_write(uint8_t* image, uint32_t size);
void autosave_wait();
// (main.c) set when the save thread couldn't write a file: the next save writes the whole file again
extern volatile int autosave_file_failed;
// (main.c) called after the thread writes a file on /sd: the frontend sets it to sd_card_sync (its file system's cache
// put on the card); NULL leaves the file in the cache until the next sync
extern void (*autosave_file_sync)(void);

// the movie's end: a recording written, a played movie freed
void movie_close(movie_t* movie);

/*
 * The frontend's side (the emulator only ever opens .frm):
 *   - a .frm: frnes_run_movie(rom, movie, NULL), the ROM the one whose CRC32 the .frm's header has (rom_info.c shows it)
 *   - a BizHawk .bk2 (or TASVideos' .zip around one): movie_bk2_to_frm(bk2, 0, NULL, sha1) for the SHA1 of the ROM it
 *     was made with (of the file without its iNES header), which the frontend matches against its ROMs; then
 *     movie_bk2_to_frm(bk2, that ROM's CRC32, a .frm path, sha1) and frnes_run_movie(rom, that .frm, NULL). The .frm
 *     can go to /ram (KOS's RAM disk)
 *   - recording: frnes_run_movie(rom, NULL, directory), a new <CRC32>_<n>.frm in it (the SD card's FrNES/movies)
 * frnes_run_movie returns when the game is left (A+B+X+Y+Start, the game menu's Exit) or the movie has run out.
 */
// movie_bk2.c (linked in the frontend builds only: it brings zlib's inflate): a .bk2 into a .frm for the ROM with this
// CRC32, and the ROM the movie was made with (Header.txt's SHA1) into 'sha1'; with frm_path NULL only the SHA1 (to find
// the ROM first); 0 when the movie can be played
int movie_bk2_to_frm(const char* bk2_path, uint32_t rom_crc32, const char* frm_path, char* sha1, uint32_t sha1_size);

#endif
