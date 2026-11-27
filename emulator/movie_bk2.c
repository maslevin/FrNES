/*
 * BizHawk .bk2 movies (NesHawk core) read into input_recording_t runs: tools/bk2_import.py's rules in C, so the player
 * plays them directly (movie.c). A .bk2 is a zip: Header.txt (Core, SHA1 of the ROM, emuVersion), SyncSettings.json
 * (the NES core's settings) and Input Log.txt (a LogKey line naming the columns, then one |Power Reset|P1 UDLRSsBA|...|
 * line per frame, lag frames included). TASVideos serves the .bk2 inside another .zip, which is opened too.
 *
 * NesHawk's frames run from one vblank to the next, its first from power on: .frm version 3's timing (the pads latched
 * at the vblank before the frame), so movie frame N is .frm frame N. Its power on: RAM in FCEUX's pattern
 * (INPUT_RECORDING_FLAG_RAM_FCEUX) and its CPU / PPU alignment, which changed in BizHawk 2.6.3
 * (INPUT_RECORDING_FLAG_NESHAWK_ALIGN, _OLD_ALIGN before it). The Four Score (FourScore on both ports) and the Zapper
 * (on either) make a version 4 movie (input_recording.h: pads 3 and 4, the Zappers' aim and trigger); the columns are
 * taken by name. Reset pressed after frame 0 is the console's reset (INPUT_RECORDING_EVENT_RESET). Refused: other
 * cores, PAL, a RAM pattern setting, other controllers, Power pressed after frame 0. All cold.
 */
#include "movie.h"
#include "memory.h"
#include "input.h"
#include <string.h>
#include <stdlib.h>
#include <zlib.h>

typedef struct {
    const uint8_t* data;
    uint32_t size;
} bk2_blob_t;

static FRNES_COLD_TEXT uint32_t le16(const uint8_t* p) {
    return p[0] | (p[1] << 8);
}

static FRNES_COLD_TEXT uint32_t le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

// a zip member by name (exact, or with 'suffix' when name is NULL), inflated into a new buffer (NUL terminated)
static FRNES_COLD_TEXT uint8_t* zip_member(const bk2_blob_t* zip, const char* name, const char* suffix, uint32_t* out_size) {
    if (zip -> size < 22) {
        return NULL;
    }
    // the end of central directory record, in the last 64 kB + 22 bytes
    int32_t end = -1;
    uint32_t lowest = (zip -> size > 65557) ? zip -> size - 65557 : 0;
    for (int32_t i = (int32_t)zip -> size - 22; i >= (int32_t)lowest; i--) {
        if (le32(zip -> data + i) == 0x06054b50) {
            end = i;
            break;
        }
    }
    if (end < 0) {
        return NULL;
    }
    uint32_t entries = le16(zip -> data + end + 10);
    uint32_t offset = le32(zip -> data + end + 16);
    for (uint32_t e = 0; e < entries; e++) {
        if ((offset + 46 > zip -> size) || (le32(zip -> data + offset) != 0x02014b50)) {
            return NULL;
        }
        const uint8_t* entry = zip -> data + offset;
        uint32_t method = le16(entry + 10), packed = le32(entry + 20), size = le32(entry + 24);
        uint32_t name_length = le16(entry + 28), extra = le16(entry + 30), comment = le16(entry + 32);
        uint32_t local = le32(entry + 42);
        const char* entry_name = (const char*)entry + 46;
        offset += 46 + name_length + extra + comment;
        int match;
        if (name) {
            match = (name_length == strlen(name)) && (memcmp(entry_name, name, name_length) == 0);
        } else {
            uint32_t n = strlen(suffix);
            match = (name_length >= n) && (strncasecmp(entry_name + name_length - n, suffix, n) == 0);
        }
        if (!match) {
            continue;
        }
        if ((local + 30 > zip -> size) || (le32(zip -> data + local) != 0x04034b50)) {
            return NULL;
        }
        uint32_t start = local + 30 + le16(zip -> data + local + 26) + le16(zip -> data + local + 28);
        if (start + packed > zip -> size) {
            return NULL;
        }
        uint8_t* out = malloc(size + 1);
        if (out == NULL) {
            printf("MOVIE .bk2: out of memory for %lu bytes\n", (unsigned long)size);
            return NULL;
        }
        if (method == 0) {
            memcpy(out, zip -> data + start, size);
        } else if (method == 8) {
            z_stream z;
            memset(&z, 0, sizeof(z));
            z.next_in = (Bytef*)(zip -> data + start);
            z.avail_in = packed;
            z.next_out = out;
            z.avail_out = size;
            // (raw deflate: zip members have no zlib header)
            if ((inflateInit2(&z, -MAX_WBITS) != Z_OK) || (inflate(&z, Z_FINISH) != Z_STREAM_END) || (z.total_out != size)) {
                inflateEnd(&z);
                free(out);
                return NULL;
            }
            inflateEnd(&z);
        } else {
            free(out);
            return NULL;
        }
        out[size] = 0;
        *out_size = size;
        return out;
    }
    return NULL;
}

// Header.txt's value for 'key' ("key value" lines) into 'value'; 0 when found
static FRNES_COLD_TEXT int header_value(const char* header, const char* key, char* value, uint32_t value_size) {
    uint32_t n = strlen(key);
    for (const char* line = header; line && *line; line = strchr(line, '\n') ? strchr(line, '\n') + 1 : NULL) {
        if ((strncmp(line, key, n) == 0) && (line[n] == ' ')) {
            const char* v = line + n + 1;
            uint32_t length = 0;
            while (v[length] && (v[length] != '\n') && (v[length] != '\r') && (length + 1 < value_size)) {
                length++;
            }
            memcpy(value, v, length);
            value[length] = 0;
            return 0;
        }
    }
    value[0] = 0;
    return -1;
}

// SyncSettings.json's "key": value as text (up to , or }), "" when missing
static FRNES_COLD_TEXT void json_value(const char* json, const char* key, char* value, uint32_t value_size) {
    char quoted[64];
    snprintf(quoted, sizeof(quoted), "\"%s\"", key);
    const char* at = json ? strstr(json, quoted) : NULL;
    value[0] = 0;
    if (at == NULL) {
        return;
    }
    at = strchr(at + strlen(quoted), ':');
    if (at == NULL) {
        return;
    }
    at++;
    while ((*at == ' ') || (*at == '"')) {
        at++;
    }
    uint32_t length = 0;
    while (at[length] && (at[length] != ',') && (at[length] != '}') && (at[length] != '"') && (length + 1 < value_size)) {
        length++;
    }
    memcpy(value, at, length);
    value[length] = 0;
}

// the Input Log's NES pad columns (U D L R S s B A) to the .frm pad bits (the $4016 order: A B Select Start U D L R)

static FRNES_COLD_TEXT int movie_bk2_load(input_recording_t* recording, const char* path, uint32_t rom_crc32, char* sha1,
    uint32_t sha1_size, char* game_name, uint32_t game_name_size) {
    memset(recording, 0, sizeof(*recording));
    sha1[0] = 0;
    game_name[0] = 0;
    file_t file = fs_open(path, O_RDONLY);
    if (file == -1) {
        printf("MOVIE .bk2: unable to open [%s]\n", path);
        return -1;
    }
    uint32_t size = fs_total(file);
    uint8_t* data = malloc(size ? size : 1);
    int ok = data && (fs_read(file, data, size) == (ssize_t)size);
    fs_close(file);
    if (!ok) {
        printf("MOVIE .bk2: unable to read [%s]\n", path);
        free(data);
        return -1;
    }
    bk2_blob_t zip = { data, size };
    uint32_t length;
    char* header = (char*)zip_member(&zip, "Header.txt", NULL, &length);
    uint8_t* inner = NULL;
    if (header == NULL) {
        // TASVideos' .zip around the .bk2
        uint32_t inner_size;
        inner = zip_member(&zip, NULL, ".bk2", &inner_size);
        if (inner) {
            free(data);
            data = inner;
            zip.data = data;
            zip.size = inner_size;
            header = (char*)zip_member(&zip, "Header.txt", NULL, &length);
        }
    }
    char* sync = (char*)zip_member(&zip, "SyncSettings.json", NULL, &length);
    char* log = (char*)zip_member(&zip, "Input Log.txt", NULL, &length);
    free(data);
    int result = -1;
    char value[96];
    if ((header == NULL) || (log == NULL)) {
        printf("MOVIE .bk2: [%s] has no Header.txt / Input Log.txt\n", path);
        goto done;
    }
    header_value(header, "Core", value, sizeof(value));
    if (strcmp(value, "NesHawk") != 0) {
        printf("MOVIE .bk2: core %s (only NesHawk movies play)\n", value);
        goto done;
    }
    if ((header_value(header, "Platform", value, sizeof(value)) == 0) && (strcmp(value, "NES") != 0)) {
        printf("MOVIE .bk2: platform %s\n", value);
        goto done;
    }
    header_value(header, "SHA1", sha1, sha1_size);
    header_value(header, "GameName", game_name, game_name_size);
    json_value(sync, "RegionOverride", value, sizeof(value));
    if (value[0] && (strcmp(value, "0") != 0) && (strcmp(value, "null") != 0)) {
        printf("MOVIE .bk2: RegionOverride %s (only NTSC)\n", value);
        goto done;
    }
    json_value(sync, "InitialWRamStatePattern", value, sizeof(value));
    // (none: missing, null or an empty list, as bk2_import.py takes it; BizHawk 2.9 writes "[]")
    if (value[0] && (strcmp(value, "null") != 0) && (strcmp(value, "[]") != 0)) {
        printf("MOVIE .bk2: InitialWRamStatePattern %s isn't supported yet\n", value);
        goto done;
    }
    // the ports: a pad, nothing (the right one), the Four Score (both: players 1 and 3 on the left, 2 and 4 on the
    // right) or the Zapper (version 4 movies, input_recording.h)
    int left_zapper = 0, right_zapper = 0, four_score = 0;
    json_value(sync, "NesLeftPort", value, sizeof(value));
    if (strcmp(value, "Zapper") == 0) {
        left_zapper = 1;
    } else if (strcmp(value, "FourScore") == 0) {
        four_score = 1;
    } else if (value[0] && (strcmp(value, "ControllerNES") != 0)) {
        printf("MOVIE .bk2: NesLeftPort %s isn't supported\n", value);
        goto done;
    }
    json_value(sync, "NesRightPort", value, sizeof(value));
    if (strcmp(value, "Zapper") == 0) {
        right_zapper = 1;
    } else if (strcmp(value, "FourScore") == 0) {
        four_score |= 2;
    } else if (value[0] && (strcmp(value, "ControllerNES") != 0) && (strcmp(value, "UnpluggedNES") != 0)) {
        printf("MOVIE .bk2: NesRightPort %s isn't supported\n", value);
        goto done;
    }
    if ((four_score != 0) && (four_score != 3)) {
        printf("MOVIE .bk2: the Four Score on one port only isn't supported\n");
        goto done;
    }
    json_value(sync, "Famicom", value, sizeof(value));
    if (strcmp(value, "true") == 0) {
        printf("MOVIE .bk2: Famicom controls aren't supported\n");
        goto done;
    }

    // the LogKey: groups of columns, each starting with '#' ("#Power|Reset|#P1 Up|P1 Down|...|"); per column, the pad
    // (1-4: ours, the Four Score's players reordered) and bit it sets by the button's name, 0x80 for Power / Reset, or a
    // Zapper's (BK2_ZAPPER | its NES port, with its Fire, X or Y), an axis's value written as 5 characters and a ','
#define BK2_ZAPPER 0x40
#define BK2_ZAPPER_FIRE 0
#define BK2_ZAPPER_X 1
#define BK2_ZAPPER_Y 2
    uint8_t column_pad[64], column_bit[64];
    uint32_t columns = 0;
    const char* key = strstr(log, "LogKey:");
    if (key == NULL) {
        printf("MOVIE .bk2: no LogKey\n");
        goto done;
    }
    key += 7;
    // (BizHawk numbers the players across the ports' halves: with the Four Score its P2 is the left port's second,
    // player 3, its P3 the right port's first, player 2; a Zapper's columns are its port's player's)
    static const char* const bk2_buttons[8] = { "A", "B", "Select", "Start", "Up", "Down", "Left", "Right" };
    static const uint8_t four_score_pads[5] = { 0, 1, 3, 2, 4 };
    uint32_t zapper_player[2] = { left_zapper ? 1 : 0, right_zapper ? (left_zapper ? 2 : 2) : 0 };
    while (*key && (*key != '\n') && (*key != '\r') && (columns < 64)) {
        const char* bar = key;
        while (*bar && (*bar != '|') && (*bar != '\n') && (*bar != '\r')) {
            bar++;
        }
        if (bar > key) {
            const char* name = (*key == '#') ? key + 1 : key;
            uint32_t length = bar - name;
            column_pad[columns] = 0;
            column_bit[columns] = 0;
            if ((strncmp(name, "Power", 5) == 0) || (strncmp(name, "Reset", 5) == 0)) {
                // (0x80 Power, 0x81 Reset)
                column_pad[columns] = (name[0] == 'R') ? 0x81 : 0x80;
            } else if ((name[0] == 'P') && (name[1] >= '1') && (name[1] <= '4') && (name[2] == ' ')) {
                uint32_t player = name[1] - '0';
                const char* button = name + 3;
                uint32_t button_length = length - 3;
                int zapper_port = (player == zapper_player[0]) ? 0 : ((player == zapper_player[1]) ? 1 : -1);
                if (zapper_port >= 0) {
                    column_pad[columns] = BK2_ZAPPER | zapper_port;
                    column_bit[columns] = ((button_length == 8) && (strncmp(button, "Zapper X", 8) == 0)) ? BK2_ZAPPER_X :
                        (((button_length == 8) && (strncmp(button, "Zapper Y", 8) == 0)) ? BK2_ZAPPER_Y : BK2_ZAPPER_FIRE);
                } else {
                    for (uint32_t b = 0; b < 8; b++) {
                        if ((strlen(bk2_buttons[b]) == button_length) && (strncmp(button, bk2_buttons[b], button_length) == 0)) {
                            column_pad[columns] = four_score ? four_score_pads[player] : player;
                            column_bit[columns] = b;
                        }
                    }
                }
            }
            columns++;
        }
        key = (*bar == '|') ? bar + 1 : bar;
    }

    // (a version 4 movie with the Four Score or a Zapper: pads 3 and 4, the Zappers' aim and trigger in each frame's
    // extension, the Zappers on)
    int extended = four_score || left_zapper || right_zapper;
    if (extended) {
        input_recording_ext_t ext;
        memset(&ext, 0, sizeof(ext));
        ext.port_device[0] = left_zapper ? INPUT_DEVICE_ZAPPER : INPUT_DEVICE_PAD;
        ext.port_device[1] = right_zapper ? INPUT_DEVICE_ZAPPER : INPUT_DEVICE_PAD;
        ext.four_score = four_score ? INPUT_FOUR_SCORE_NES : INPUT_FOUR_SCORE_NONE;
        input_recording_begin_ext(recording, rom_crc32, &ext);
    } else {
        input_recording_begin(recording, rom_crc32);
    }
    // (a Reset pressed on the frame before: NesHawk's reset at this frame's start)
    int reset_next = 0;
    for (const char* line = log; line && *line; line = strchr(line, '\n') ? strchr(line, '\n') + 1 : NULL) {
        if (*line != '|') {
            continue;
        }
        uint8_t pads[5] = { 0, 0, 0, 0, 0 };
        input_recording_ext_run_t zap;
        memset(&zap, 0, sizeof(zap));
        zap.y0 = zap.y1 = 0xff;
        zap.flags = (left_zapper ? INPUT_EXT_ZAPPER_ON(0) : 0) | (right_zapper ? INPUT_EXT_ZAPPER_ON(1) : 0);
        uint32_t column = 0;
        int console_pressed = 0, reset_pressed = 0;
        for (const char* c = line + 1; *c && (*c != '\n') && (*c != '\r') && (column < columns); c++) {
            if (*c == '|') {
                continue;
            }
            uint8_t kind = column_pad[column];
            if (((kind & 0xf0) == BK2_ZAPPER) && (column_bit[column] != BK2_ZAPPER_FIRE)) {
                // an axis: its value then ','
                long v = strtol(c, (char**)&c, 10);
                while (*c && (*c != ',') && (*c != '|') && (*c != '\n')) {
                    c++;
                }
                uint32_t port = kind & 1;
                uint16_t value16 = (uint16_t)((v < 0) ? 0 : ((v > 255) ? 255 : v));
                // (each member stored by name: the struct is packed, and a pointer to its 16 bit members, taken by
                // address, would be stored through as aligned, an address error on the SH4 when it isn't)
                if (column_bit[column] == BK2_ZAPPER_X) {
                    if (port) {
                        zap.x1 = value16;
                    } else {
                        zap.x0 = value16;
                    }
                } else {
                    uint16_t y = (value16 > 239) ? 0xff : value16;
                    if (port) {
                        zap.y1 = y;
                    } else {
                        zap.y0 = y;
                    }
                }
                column++;
                continue;
            }
            if ((*c != '.') && (*c != ' ')) {
                if (kind == 0x80) {
                    console_pressed = 1;
                } else if (kind == 0x81) {
                    reset_pressed = 1;
                } else if ((kind & 0xf0) == BK2_ZAPPER) {
                    zap.flags |= INPUT_EXT_TRIGGER(kind & 1);
                } else if (kind) {
                    pads[kind] |= 1 << column_bit[column];
                }
            }
            column++;
        }
        if (console_pressed && !recording -> header.num_frames) {
            // (Power on frame 0: the power on itself)
            console_pressed = 0;
        }
        if (console_pressed) {
            printf("MOVIE .bk2: Power pressed at frame %lu (not supported)\n", (unsigned long)recording -> header.num_frames);
            input_recording_free(recording);
            goto done;
        }
        // (Reset: NesHawk takes it with the frame's input and resets at the start of the next frame (NES.Core.cs's
        // FrameAdvance, resetSignal): the event on the next frame, the console's reset before it runs, frnes_soft_reset)
        uint16_t event = reset_next ? INPUT_RECORDING_EVENT_RESET : 0;
        reset_next = reset_pressed;
        if (extended) {
            zap.pad3 = pads[3];
            zap.pad4 = pads[4];
            input_recording_add_frame_ext(recording, pads[1], pads[2], event, &zap, 0);
        } else {
            input_recording_add_frame_event(recording, pads[1], pads[2], event);
        }
    }
    // NesHawk's power on (input_recording.h), by BizHawk version: "Version 2.9.1", "GIT ..." (new)
    header_value(header, "emuVersion", value, sizeof(value));
    uint32_t align = INPUT_RECORDING_FLAG_NESHAWK_ALIGN;
    for (const char* v = value; *v; v++) {
        if ((*v >= '0') && (*v <= '9')) {
            unsigned major = 0, minor = 0, patch = 0;
            if (sscanf(v, "%u.%u.%u", &major, &minor, &patch) >= 2) {
                if ((major < 2) || ((major == 2) && ((minor < 6) || ((minor == 6) && (patch < 3))))) {
                    align = INPUT_RECORDING_FLAG_NESHAWK_OLD_ALIGN;
                }
            }
            break;
        }
    }
    recording -> header.flags |= INPUT_RECORDING_FLAG_RAM_FCEUX | align;
    printf("MOVIE .bk2: %lu frames (%lu runs), ROM SHA1 %s, %s\n", (unsigned long)recording -> header.num_frames,
        (unsigned long)recording -> header.num_runs, sha1, value);
    result = 0;
done:
    free(header);
    free(sync);
    free(log);
    return result;
}

FRNES_COLD_TEXT int movie_bk2_to_frm(const char* bk2_path, uint32_t rom_crc32, const char* frm_path, char* sha1, uint32_t sha1_size) {
    input_recording_t recording;
    char game_name[64];
    if (movie_bk2_load(&recording, bk2_path, rom_crc32, sha1, sha1_size, game_name, sizeof(game_name)) != 0) {
        return -1;
    }
    // (no .frm path: only the movie's ROM SHA1 wanted, to find the ROM. The .frm names the game as Header.txt does)
    input_recording_rom_info_t rom;
    input_recording_rom_info_set(&rom, game_name, NULL);
    int result = frm_path ? input_recording_save_rom(&recording, game_name[0] ? &rom : NULL, frm_path) : 0;
    input_recording_free(&recording);
    return result;
}
