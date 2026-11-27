#include "movie.h"
#include "input.h"
#include "memory.h"
#include "mapper.h"
#include <string.h>
#include <stdlib.h>

// the movie's checkpoints: <movie without its extension>.frc, if there is one (tools/frc_make.py)
static FRNES_COLD_TEXT void movie_checkpoints_load(movie_t* movie, const char* path) {
    char frc[sizeof(movie -> path) + 8];
    strncpy(frc, path, sizeof(movie -> path));
    frc[sizeof(movie -> path)] = 0;
    char* dot = strrchr(frc, '.');
    strcpy(dot ? dot : frc + strlen(frc), ".frc");
    file_t file = fs_open(frc, O_RDONLY);
    if (file == -1) {
        return;
    }
    struct __attribute__((packed)) {
        char magic[8];
        uint32_t version, count, every;
    } header;
    uint32_t size = 0;
    if ((fs_read(file, &header, sizeof(header)) == sizeof(header)) && (memcmp(header.magic, "FRNESCHK", 8) == 0) &&
        (header.version == 1) && (fs_read(file, movie -> checkpoint_ignore, 256) == 256)) {
        size = header.count * 8;
        movie -> checkpoints = malloc(size ? size : 8);
        if (movie -> checkpoints && (fs_read(file, movie -> checkpoints, size) == (ssize_t)size)) {
            movie -> checkpoint_count = header.count;
            printf("MOVIE checkpoints: %lu, every %lu frames, from [%s]\n", (unsigned long)header.count,
                (unsigned long)header.every, frc);
        } else {
            free(movie -> checkpoints);
            movie -> checkpoints = NULL;
        }
    }
    fs_close(file);
    if (!movie -> checkpoint_count) {
        printf("MOVIE checkpoints: [%s] unreadable, not checking\n", frc);
    }
}

FRNES_COLD_TEXT void movie_checkpoint(movie_t* movie, uint32_t frame) {
    while ((movie -> checkpoint_next < movie -> checkpoint_count) && (movie -> checkpoints[movie -> checkpoint_next * 2] < frame)) {
        movie -> checkpoint_next++;
    }
    if ((movie -> checkpoint_next >= movie -> checkpoint_count) || (movie -> checkpoints[movie -> checkpoint_next * 2] != frame)) {
        return;
    }
    const uint8_t* ram = (const uint8_t*)RAM_ADDR;
    uint32_t hash = 2166136261u;
    for (uint32_t a = 0; a < 0x800; a++) {
        if (!((movie -> checkpoint_ignore[a >> 3] >> (a & 7)) & 1)) {
            hash = (hash ^ ram[a]) * 16777619u;
        }
    }
    uint32_t expected = movie -> checkpoints[movie -> checkpoint_next * 2 + 1];
    movie -> checkpoint_next++;
    if ((hash != expected) && !movie -> desync_frame) {
        movie -> desync_frame = frame;
        printf("MOVIE DESYNC by frame %lu: RAM hash %08lX, the checkpoint's %08lX\n", (unsigned long)frame,
            (unsigned long)hash, (unsigned long)expected);
    }
}

FRNES_COLD_TEXT int movie_open(movie_t* movie, const char* path, uint32_t rom_crc32) {
    memset(movie, 0, sizeof(*movie));
    int loaded = input_recording_load(&movie -> recording, path, rom_crc32);
    if (loaded != 0) {
        printf("MOVIE unable to load [%s]\n", path);
        return -1;
    }
    strncpy(movie -> path, path, sizeof(movie -> path) - 1);
    // the power on state the movie's emulator had (an imported TAS movie: its RAM pattern, NesHawk's PPU alignment)
    input_recording_power_on(&movie -> recording, (uint8_t*)RAM_ADDR);
    movie -> mode = MOVIE_PLAYING;
    movie_checkpoints_load(movie, path);
    movie -> start_us = timer_us_gettime64();
    printf("MOVIE playing [%s], %lu frames, pads latched at %s\n", path, (unsigned long)movie -> recording.header.num_frames,
        movie_input_at_vblank(movie) ? "vblank" : "the end of the frame");
    return 0;
}

FRNES_COLD_TEXT int movie_input(movie_t* movie, uint32_t frame, uint8_t* pad1, uint8_t* pad2, uint16_t* event) {
    if (!input_recording_frame(&movie -> recording, frame, pad1, pad2)) {
        if (movie -> mode == MOVIE_PLAYING) {
            movie -> mode = MOVIE_ENDED;
            uint64_t elapsed_us = timer_us_gettime64() - movie -> start_us;
            // (tools: perf_runs/player_ab.py and tools/console_run.py wait for this line)
            printf("PLAYBACK done, %lu frames in %.2f s (%.2f fps)\n", (unsigned long)movie -> frames,
                (double)elapsed_us / 1000000.0, (double)movie -> frames * 1000000.0 / (double)elapsed_us);
        }
        *event = 0;
        return 0;
    }
    *event = input_recording_event(&movie -> recording, frame);
    // (the console's reset, before the frame: its one path; the event replayed isn't recorded again)
    if (__builtin_expect(*event & INPUT_RECORDING_EVENT_RESET, 0)) {
        frnes_soft_reset_now();
        frnes_reset_event();
    }
    // (players 3 and 4, the Zappers: the frame's from the run's extension, in place of the controllers)
    if (movie -> recording.header.flags & INPUT_RECORDING_FLAG_EXT) {
        input_recording_ext_run_t ext;
        input_recording_ext_frame(&movie -> recording, &ext);
        input_ext_apply(&ext);
    }
    // (a Vs. System game's coin, and its first frame's set up: main.c's recording_event is the FDS's)
    if (__builtin_expect(cpu_registers -> VS_Flags, 0)) {
        vs_recording_event(*event);
    }
    movie -> frames = frame;
    return 1;
}

FRNES_COLD_TEXT int movie_input_at_vblank(const movie_t* movie) {
    return (movie -> recording.header.flags & INPUT_RECORDING_FLAG_VBLANK) != 0;
}

static void movie_record_path(movie_t* movie);

FRNES_COLD_TEXT int movie_record_begin(movie_t* movie, uint32_t rom_crc32, const char* directory, const char* rom_path) {
    memset(movie, 0, sizeof(*movie));
    strncpy(movie -> directory, directory, sizeof(movie -> directory) - 1);
    input_recording_rom_info_set(&movie -> rom, NULL, rom_path);
    // (dc-load-ip's /pc can't make directories: there it has to exist already)
    fs_mkdir(directory);
    // (version 4 for a game with the Zapper or four players: their inputs in each run's extension)
    input_recording_ext_t ext;
    if (input_ext_block(&ext)) {
        input_recording_ext_run_t none;
        memset(&none, 0, sizeof(none));
        none.y0 = none.y1 = 0xff;
        input_recording_begin_ext(&movie -> recording, rom_crc32, &ext);
        input_recording_add_frame_ext(&movie -> recording, 0, 0, 0, &none, 0);
    } else {
        input_recording_begin(&movie -> recording, rom_crc32);
        input_recording_add_frame(&movie -> recording, 0, 0);
    }
    // (the file's name now, at the start, not at the first save between frames: finding a free one opens files)
    movie_record_path(movie);
    movie -> mode = MOVIE_RECORDING;
    movie -> start_us = timer_us_gettime64();
    printf("RECORDING from power on, ROM CRC32 %08lX, to %s\n", (unsigned long)rom_crc32, directory);
    return 0;
}

// the recording's file, picked when it starts: the first <CRC>_NNN.frm not taken
static FRNES_COLD_TEXT void movie_record_path(movie_t* movie) {
    const input_recording_t* recording = &movie -> recording;
    if (!movie -> path[0]) {
        for (uint32_t index = 0; index < 1000; index++) {
            snprintf(movie -> path, sizeof(movie -> path), "%s/%08lX_%03lu.frm", movie -> directory,
                (unsigned long)recording -> header.rom_crc32, (unsigned long)index);
            file_t existing = fs_open(movie -> path, O_RDONLY);
            if (existing == -1) {
                break;
            }
            fs_close(existing);
        }
    }
}

// the recording written now (at its end): the runs since the last save and the header (input_recording_image), or the
// whole file when nothing is on file yet
FRNES_COLD_TEXT int movie_record_save(movie_t* movie) {
    // (after a periodic save the thread is still writing)
    autosave_wait();
    movie_record_path(movie);
    if (autosave_file_failed) {
        autosave_file_failed = 0;
        movie -> saved_runs = 0;
    }
    uint32_t size;
    uint8_t* image = input_recording_image(&movie -> recording, &movie -> rom, movie -> path, movie -> saved_runs, &size);
    int result = image ? input_recording_write_image(image, size, NULL) : -1;
    free(image);
    if (result == 0) {
        movie -> saved_runs = movie -> recording.header.num_runs;
        if ((strncmp(movie -> path, "/sd/", 4) == 0) && autosave_file_sync) {
            autosave_file_sync();
        }
    }
    printf("RECORDING %s %lu frames to [%s]\n", (result == 0) ? "saved" : "couldn't save", (unsigned long)movie -> recording.header.num_frames,
        movie -> path);
    return result;
}

FRNES_COLD_TEXT int movie_record_frame(movie_t* movie, uint32_t frame, uint8_t pad1, uint8_t pad2, uint16_t event,
    uint8_t ltrig, uint8_t rtrig) {
    // the input for the next frame (at its vblank, or at the end of this one; a new run after the runs on file, which
    // the saves don't write again), with a reset made since the last (frnes_reset_event)
    event |= frnes_reset_event();
    int split = movie -> saved_runs && (movie -> recording.header.num_runs == movie -> saved_runs);
    if (movie -> recording.header.flags & INPUT_RECORDING_FLAG_EXT) {
        input_recording_ext_run_t ext;
        input_ext_capture(&ext);
        input_recording_add_frame_ext(&movie -> recording, pad1, pad2, event, &ext, split);
    } else if (split) {
        input_recording_add_frame_split(&movie -> recording, pad1, pad2, event);
    } else {
        input_recording_add_frame_event(&movie -> recording, pad1, pad2, event);
    }
    movie -> frames = frame;
    movie -> trigger_frames = ((ltrig > 200) && (rtrig > 200)) ? movie -> trigger_frames + 1 : 0;
    if (movie -> trigger_frames == MOVIE_STOP_FRAMES) {
        movie -> mode = MOVIE_RECORDED;
        printf("RECORDING stopped at frame %lu\n", (unsigned long)frame);
        movie_record_save(movie);
        return 1;
    }
    if ((frame % MOVIE_SAVE_FRAMES) == 0) {
        movie_record_save_background(movie);
    }
    return 0;
}

// the periodic save while recording: the runs since the last save and the header (the whole file the first time),
// made here and written by the save thread (main.c's autosave_file_write) while the game goes on, so its cost doesn't
// grow with the movie's length; skipped while a save is still being written (the next one takes those runs too)
FRNES_COLD_TEXT void movie_record_save_background(movie_t* movie) {
    movie_record_path(movie);
    if (autosave_file_failed) {
        autosave_file_failed = 0;
        movie -> saved_runs = 0;
    }
    uint32_t size;
    uint8_t* image = input_recording_image(&movie -> recording, &movie -> rom, movie -> path, movie -> saved_runs, &size);
    if (image && (autosave_file_write(image, size) != 0)) {
        free(image);
        return;
    }
    if (image) {
        movie -> saved_runs = movie -> recording.header.num_runs;
    }
}

FRNES_COLD_TEXT void movie_close(movie_t* movie) {
    if (movie -> mode == MOVIE_RECORDING) {
        printf("RECORDING stopped at exit, frame %lu\n", (unsigned long)movie -> frames);
        movie_record_save(movie);
    }
    if (movie -> checkpoint_count) {
        printf("MOVIE checkpoints: %s\n", movie -> desync_frame ? "DESYNCED" : "all matched");
    }
    free(movie -> checkpoints);
    movie -> checkpoints = NULL;
    movie -> checkpoint_count = 0;
    if (movie -> mode != MOVIE_OFF) {
        input_recording_free(&movie -> recording);
        movie -> mode = MOVIE_OFF;
    }
}
