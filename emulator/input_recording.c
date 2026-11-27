#include "input_recording.h"
#include "ppu.h"
#include "memory.h"

#include <stdlib.h>
#include <string.h>

uint32_t input_recording_crc32(const uint8_t* data, uint32_t length) {
    return input_recording_crc32_update(0, data, length);
}

uint32_t input_recording_crc32_update(uint32_t crc, const uint8_t* data, uint32_t length) {
    static uint32_t table[256];
    static int table_built = 0;
    if (!table_built) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (uint32_t k = 0; k < 8; k++) {
                c = (c & 1) ? (0xedb88320 ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        table_built = 1;
    }
    crc ^= 0xffffffff;
    for (uint32_t i = 0; i < length; i++) {
        crc = table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    }
    return crc ^ 0xffffffff;
}

void input_recording_begin(input_recording_t* recording, uint32_t rom_crc32) {
    memset(recording, 0, sizeof(*recording));
    memcpy(recording -> header.magic, INPUT_RECORDING_MAGIC, 8);
    recording -> header.version = INPUT_RECORDING_VERSION;
    recording -> header.header_size = sizeof(input_recording_header_t);
    recording -> header.rom_crc32 = rom_crc32;
    // (the player latches the pads at vblank: see INPUT_RECORDING_FLAG_VBLANK)
    recording -> header.flags = INPUT_RECORDING_FLAG_POWER_ON | INPUT_RECORDING_FLAG_VBLANK;
}

void input_recording_add_frame(input_recording_t* recording, uint8_t pad1, uint8_t pad2) {
    input_recording_add_frame_event(recording, pad1, pad2, 0);
}

// (split: a new run even when the last has the same pads; ext: version 4's extension, NULL for none)
static void add_frame(input_recording_t* recording, uint8_t pad1, uint8_t pad2, uint16_t event, int split,
    const input_recording_ext_run_t* ext) {
    input_recording_header_t* header = &recording -> header;
    static const input_recording_ext_run_t no_ext;
    int extended = (header -> flags & INPUT_RECORDING_FLAG_EXT) != 0;
    if (extended && (ext == NULL)) {
        ext = &no_ext;
    }
    if (header -> num_runs && !event && !split) {
        input_recording_run_t* last = &recording -> runs[header -> num_runs - 1];
        if ((last -> pad1 == pad1) && (last -> pad2 == pad2) &&
            (!extended || (memcmp(&recording -> ext_runs[header -> num_runs - 1], ext, sizeof(*ext)) == 0))) {
            last -> frames++;
            header -> num_frames++;
            return;
        }
    }
    if (header -> num_runs == recording -> capacity) {
        uint32_t capacity = recording -> capacity ? (recording -> capacity * 2) : 4096;
        if (extended) {
            input_recording_ext_run_t* ext_runs = realloc(recording -> ext_runs, capacity * sizeof(input_recording_ext_run_t));
            if (ext_runs == NULL) {
                if (header -> num_runs) {
                    recording -> runs[header -> num_runs - 1].frames++;
                    header -> num_frames++;
                }
                return;
            }
            recording -> ext_runs = ext_runs;
        }
        input_recording_run_t* runs = realloc(recording -> runs, capacity * sizeof(input_recording_run_t));
        if (runs == NULL) {
            // out of memory, extend the last run rather than lose the frame count
            if (header -> num_runs) {
                recording -> runs[header -> num_runs - 1].frames++;
                header -> num_frames++;
            }
            return;
        }
        recording -> runs = runs;
        recording -> capacity = capacity;
    }
    if (extended) {
        recording -> ext_runs[header -> num_runs] = *ext;
    }
    input_recording_run_t* run = &recording -> runs[header -> num_runs++];
    run -> frames = 1;
    run -> pad1 = pad1;
    run -> pad2 = pad2;
    run -> event = event;
    header -> num_frames++;
}

void input_recording_add_frame_event(input_recording_t* recording, uint8_t pad1, uint8_t pad2, uint16_t event) {
    add_frame(recording, pad1, pad2, event, 0, NULL);
}

FRNES_COLD_TEXT void input_recording_add_frame_split(input_recording_t* recording, uint8_t pad1, uint8_t pad2, uint16_t event) {
    add_frame(recording, pad1, pad2, event, 1, NULL);
}

FRNES_COLD_TEXT void input_recording_begin_ext(input_recording_t* recording, uint32_t rom_crc32, const input_recording_ext_t* ext) {
    input_recording_begin(recording, rom_crc32);
    recording -> header.version = INPUT_RECORDING_VERSION_EXT;
    recording -> header.flags |= INPUT_RECORDING_FLAG_EXT;
    recording -> ext = *ext;
    recording -> ext.block_version = INPUT_RECORDING_EXT_BLOCK_VERSION;
    recording -> ext.record_size = sizeof(input_recording_run_t) + sizeof(input_recording_ext_run_t);
}

FRNES_COLD_TEXT void input_recording_add_frame_ext(input_recording_t* recording, uint8_t pad1, uint8_t pad2, uint16_t event,
    const input_recording_ext_run_t* ext, int split) {
    add_frame(recording, pad1, pad2, event, split, ext);
}

FRNES_COLD_TEXT void input_recording_ext_frame(const input_recording_t* recording, input_recording_ext_run_t* ext) {
    if (recording -> ext_runs && (recording -> run_index < recording -> header.num_runs)) {
        *ext = recording -> ext_runs[recording -> run_index];
    } else {
        memset(ext, 0, sizeof(*ext));
    }
}

// a run record's size on file, and the records of runs [from, to) as on file (version 4: each run, then its extension)
static FRNES_COLD_TEXT uint32_t record_size(const input_recording_t* recording) {
    return (recording -> header.flags & INPUT_RECORDING_FLAG_EXT) ?
        sizeof(input_recording_run_t) + sizeof(input_recording_ext_run_t) : sizeof(input_recording_run_t);
}

static FRNES_COLD_TEXT void records_copy(uint8_t* out, const input_recording_t* recording, uint32_t from, uint32_t to) {
    if (!(recording -> header.flags & INPUT_RECORDING_FLAG_EXT)) {
        memcpy(out, &recording -> runs[from], (to - from) * sizeof(input_recording_run_t));
        return;
    }
    for (uint32_t i = from; i < to; i++) {
        memcpy(out, &recording -> runs[i], sizeof(input_recording_run_t));
        memcpy(out + sizeof(input_recording_run_t), &recording -> ext_runs[i], sizeof(input_recording_ext_run_t));
        out += sizeof(input_recording_run_t) + sizeof(input_recording_ext_run_t);
    }
}

// the header as written, with the blocks' flags and its size
static FRNES_COLD_TEXT input_recording_header_t file_header(const input_recording_t* recording, const input_recording_rom_info_t* rom) {
    input_recording_header_t header = recording -> header;
    header.flags &= ~INPUT_RECORDING_FLAG_ROM_INFO;
    header.header_size = sizeof(header);
    if (rom) {
        header.flags |= INPUT_RECORDING_FLAG_ROM_INFO;
        header.header_size += sizeof(*rom);
    }
    if (header.flags & INPUT_RECORDING_FLAG_EXT) {
        header.header_size += sizeof(input_recording_ext_t);
    }
    return header;
}

int input_recording_save(const input_recording_t* recording, const char* path) {
    return input_recording_save_rom(recording, NULL, path);
}

// (the file input_recording_image makes, written as it writes it: the one serialization of the format)
FRNES_COLD_TEXT int input_recording_save_rom(const input_recording_t* recording, const input_recording_rom_info_t* rom, const char* path) {
    uint32_t size;
    uint8_t* image = input_recording_image(recording, rom, path, 0, &size);
    int result = image ? input_recording_write_image(image, size, NULL) : -1;
    free(image);
    if (result == 0) {
        printf("input recording - saved %lu frames (%lu runs) to [%s]\n", (unsigned long)recording -> header.num_frames,
            (unsigned long)recording -> header.num_runs, path);
    } else {
        printf("input recording - error writing [%s]\n", path);
    }
    return result;
}

// (input_recording_image's writes: 'W' and the whole file, or 'P' and pieces, each its file offset and length (32 bits,
// little endian) and its bytes)
#define IMAGE_WHOLE 'W'
#define IMAGE_PIECES 'P'

FRNES_COLD_TEXT uint8_t* input_recording_image(const input_recording_t* recording, const input_recording_rom_info_t* rom,
    const char* prefix, uint32_t from_run, uint32_t* size) {
    uint32_t prefix_size = strlen(prefix) + 1;
    if (from_run > recording -> header.num_runs) {
        from_run = recording -> header.num_runs;
    }
    uint32_t record = record_size(recording);
    uint32_t runs_size = (recording -> header.num_runs - from_run) * record;
    input_recording_header_t header = file_header(recording, rom);
    *size = prefix_size + 1 + (from_run ? (8 + runs_size + 8 + sizeof(header)) : (header.header_size + runs_size));
    uint8_t* image = malloc(*size);
    if (image == NULL) {
        return NULL;
    }
    uint8_t* at = image;
    memcpy(at, prefix, prefix_size);
    at += prefix_size;
    if (from_run == 0) {
        *at++ = IMAGE_WHOLE;
        memcpy(at, &header, sizeof(header));
        uint32_t block = sizeof(header);
        if (rom) {
            memcpy(at + block, rom, sizeof(*rom));
            block += sizeof(*rom);
        }
        if (header.flags & INPUT_RECORDING_FLAG_EXT) {
            memcpy(at + block, &recording -> ext, sizeof(recording -> ext));
        }
        records_copy(at + header.header_size, recording, 0, recording -> header.num_runs);
        return image;
    }
    *at++ = IMAGE_PIECES;
    // the new runs after those on file, then the header that counts them (the blocks after it are as on file)
    uint32_t piece[2] = { header.header_size + from_run * record, runs_size };
    memcpy(at, piece, 8);
    records_copy(at + 8, recording, from_run, recording -> header.num_runs);
    at += 8 + runs_size;
    piece[0] = 0;
    piece[1] = sizeof(header);
    memcpy(at, piece, 8);
    memcpy(at + 8, &header, sizeof(header));
    return image;
}

// (4 kB at a time, a turn for 'yield' after each)
static FRNES_COLD_TEXT int image_write(file_t file, const uint8_t* data, uint32_t size, void (*yield)(void)) {
    for (uint32_t at = 0; at < size; ) {
        uint32_t piece = ((size - at) < 4096) ? size - at : 4096;
        if (fs_write(file, data + at, piece) != (ssize_t)piece) {
            return -1;
        }
        at += piece;
        if (yield) {
            yield();
        }
    }
    return 0;
}

FRNES_COLD_TEXT int input_recording_write_image(const uint8_t* image, uint32_t size, void (*yield)(void)) {
    const char* path = (const char*)image;
    uint32_t at = strlen(path) + 1;
    if (at >= size) {
        return -1;
    }
    int whole = (image[at++] == IMAGE_WHOLE);
    // (O_CREAT: the SD card's file system wants it for a new file; pieces go into the file as it is, opened O_RDWR: KOS's
    // FAT driver sets an O_WRONLY file's size to where each write ends, so the header written last at offset 0 cut the
    // recording to its 40 bytes)
    file_t file = fs_open(path, whole ? (O_WRONLY | O_TRUNC | O_CREAT) : O_RDWR);
    if (file == -1) {
        return -1;
    }
    int result = 0;
    if (whole) {
        result = image_write(file, image + at, size - at, yield);
    }
    while (!whole && (result == 0) && (at + 8 <= size)) {
        uint32_t piece[2];
        memcpy(piece, image + at, 8);
        at += 8;
        if ((piece[1] > size - at) || (fs_seek(file, piece[0], SEEK_SET) != (off_t)piece[0])) {
            result = -1;
            break;
        }
        result = image_write(file, image + at, piece[1], yield);
        at += piece[1];
    }
    fs_close(file);
    return result;
}

FRNES_COLD_TEXT int input_recording_rom_info(const char* path, input_recording_rom_info_t* rom) {
    memset(rom, 0, sizeof(*rom));
    file_t file = fs_open(path, O_RDONLY);
    if (file == -1) {
        return -1;
    }
    input_recording_header_t header;
    int result = -1;
    // (the ROM info block is the first optional one: right after the header)
    if ((fs_read(file, &header, sizeof(header)) == sizeof(header)) && (memcmp(header.magic, INPUT_RECORDING_MAGIC, 8) == 0) &&
        (header.flags & INPUT_RECORDING_FLAG_ROM_INFO) && (header.header_size >= sizeof(header) + sizeof(*rom)) &&
        (fs_read(file, rom, sizeof(*rom)) == sizeof(*rom))) {
        rom -> name[sizeof(rom -> name) - 1] = '\0';
        rom -> path[sizeof(rom -> path) - 1] = '\0';
        result = 0;
    }
    fs_close(file);
    return result;
}

FRNES_COLD_TEXT void input_recording_rom_info_set(input_recording_rom_info_t* rom, const char* name, const char* path) {
    memset(rom, 0, sizeof(*rom));
    if (path) {
        strncpy(rom -> path, path, sizeof(rom -> path) - 1);
    }
    if (name) {
        strncpy(rom -> name, name, sizeof(rom -> name) - 1);
        return;
    }
    // (no name: the path's file name without its extension)
    const char* slash = path ? strrchr(path, '/') : NULL;
    strncpy(rom -> name, slash ? slash + 1 : (path ? path : ""), sizeof(rom -> name) - 1);
    char* dot = strrchr(rom -> name, '.');
    if (dot) {
        *dot = '\0';
    }
}

// version 4's extension block (after the ROM info block, if any): 0 when it's one this reader knows
static FRNES_COLD_TEXT int ext_block(file_t file, const input_recording_header_t* header, input_recording_ext_t* ext) {
    uint32_t at = sizeof(*header) + ((header -> flags & INPUT_RECORDING_FLAG_ROM_INFO) ? sizeof(input_recording_rom_info_t) : 0);
    if ((header -> header_size < at + sizeof(*ext)) || (fs_seek(file, at, SEEK_SET) != (off_t)at) ||
        (fs_read(file, ext, sizeof(*ext)) != sizeof(*ext))) {
        return -1;
    }
    return ((ext -> block_version == INPUT_RECORDING_EXT_BLOCK_VERSION) &&
        (ext -> record_size == sizeof(input_recording_run_t) + sizeof(input_recording_ext_run_t))) ? 0 : -1;
}

// version 4's records, split into the runs and their extensions: the runs' bytes read (sizeof(input_recording_run_t) each)
static FRNES_COLD_TEXT ssize_t ext_records_read(file_t file, input_recording_t* recording) {
    uint8_t buffer[64 * (sizeof(input_recording_run_t) + sizeof(input_recording_ext_run_t))];
    const uint32_t record = sizeof(input_recording_run_t) + sizeof(input_recording_ext_run_t);
    uint32_t done = 0;
    while (done < recording -> header.num_runs) {
        uint32_t count = recording -> header.num_runs - done;
        if (count > 64) {
            count = 64;
        }
        if (fs_read(file, buffer, count * record) != (ssize_t)(count * record)) {
            break;
        }
        for (uint32_t i = 0; i < count; i++) {
            memcpy(&recording -> runs[done + i], buffer + i * record, sizeof(input_recording_run_t));
            memcpy(&recording -> ext_runs[done + i], buffer + i * record + sizeof(input_recording_run_t), sizeof(input_recording_ext_run_t));
        }
        done += count;
    }
    return done * sizeof(input_recording_run_t);
}

FRNES_COLD_TEXT int input_recording_ext_info(const char* path, input_recording_ext_t* ext) {
    memset(ext, 0, sizeof(*ext));
    file_t file = fs_open(path, O_RDONLY);
    if (file == -1) {
        return -1;
    }
    input_recording_header_t header;
    int result = -1;
    if ((fs_read(file, &header, sizeof(header)) == sizeof(header)) && (memcmp(header.magic, INPUT_RECORDING_MAGIC, 8) == 0) &&
        (header.flags & INPUT_RECORDING_FLAG_EXT)) {
        result = ext_block(file, &header, ext);
    }
    fs_close(file);
    return result;
}

int input_recording_load(input_recording_t* recording, const char* path, uint32_t expected_crc32) {
    memset(recording, 0, sizeof(*recording));
    file_t file = fs_open(path, O_RDONLY);
    if (file == -1) {
        printf("input recording - unable to open [%s]\n", path);
        return -1;
    }
    input_recording_header_t* header = &recording -> header;
    int result = -1;
    if (fs_read(file, header, sizeof(*header)) != sizeof(*header)) {
        printf("input recording - [%s] is too short for a header\n", path);
    } else if (memcmp(header -> magic, INPUT_RECORDING_MAGIC, 8) != 0) {
        printf("input recording - [%s] is not a recording\n", path);
    } else if ((header -> version < 1) || (header -> version > INPUT_RECORDING_VERSION_EXT) ||
        ((header -> version == INPUT_RECORDING_VERSION_EXT) != ((header -> flags & INPUT_RECORDING_FLAG_EXT) != 0))) {
        printf("input recording - [%s] is version %lu, expected %d\n", path, (unsigned long)header -> version, INPUT_RECORDING_VERSION_EXT);
    } else if (header -> header_size < sizeof(*header)) {
        printf("input recording - [%s] has a bad header size\n", path);
    } else if (expected_crc32 && (header -> rom_crc32 != expected_crc32)) {
        printf("input recording - [%s] is for ROM CRC %08lX, this ROM is %08lX\n", path, (unsigned long)header -> rom_crc32,
            (unsigned long)expected_crc32);
    } else if (!(header -> flags & INPUT_RECORDING_FLAG_POWER_ON)) {
        printf("input recording - [%s] does not start at power on\n", path);
    } else if ((header -> flags & INPUT_RECORDING_FLAG_EXT) && (ext_block(file, header, &recording -> ext) != 0)) {
        printf("input recording - [%s] has a bad extension block\n", path);
    } else {
        uint32_t extended = (header -> flags & INPUT_RECORDING_FLAG_EXT) != 0;
        uint32_t runs_size = header -> num_runs * sizeof(input_recording_run_t);
        recording -> runs = malloc(runs_size ? runs_size : 1);
        if (extended) {
            recording -> ext_runs = malloc(header -> num_runs ? header -> num_runs * sizeof(input_recording_ext_run_t) : 1);
        }
        fs_seek(file, header -> header_size, SEEK_SET);
        if ((recording -> runs == NULL) || (extended && (recording -> ext_runs == NULL))) {
            printf("input recording - out of memory for %lu runs\n", (unsigned long)header -> num_runs);
        } else if ((extended ? ext_records_read(file, recording) : fs_read(file, recording -> runs, runs_size)) != (ssize_t)runs_size) {
            printf("input recording - [%s] is truncated\n", path);
        } else {
            // the runs have to add up to the frame count
            uint32_t frames = 0;
            uint32_t i;
            for (i = 0; i < header -> num_runs; i++) {
                if (recording -> runs[i].frames == 0) {
                    break;
                }
                frames += recording -> runs[i].frames;
            }
            if ((i != header -> num_runs) || (frames != header -> num_frames)) {
                printf("input recording - [%s] runs do not add up to %lu frames\n", path, (unsigned long)header -> num_frames);
            } else {
                recording -> capacity = header -> num_runs;
                result = 0;
                printf("input recording - loaded %lu frames (%lu runs) for ROM CRC %08lX from [%s]\n", (unsigned long)header -> num_frames,
                    (unsigned long)header -> num_runs, (unsigned long)header -> rom_crc32, path);
            }
        }
    }
    fs_close(file);
    if (result != 0) {
        input_recording_free(recording);
    }
    return result;
}

int input_recording_frame(input_recording_t* recording, uint32_t frame, uint8_t* pad1, uint8_t* pad2) {
    while ((recording -> run_index < recording -> header.num_runs) &&
           (frame >= recording -> run_start_frame + recording -> runs[recording -> run_index].frames)) {
        recording -> run_start_frame += recording -> runs[recording -> run_index].frames;
        recording -> run_index++;
    }
    if (recording -> run_index >= recording -> header.num_runs) {
        *pad1 = *pad2 = 0;
        return 0;
    }
    *pad1 = recording -> runs[recording -> run_index].pad1;
    *pad2 = recording -> runs[recording -> run_index].pad2;
    return 1;
}

uint16_t input_recording_event(const input_recording_t* recording, uint32_t frame) {
    if ((recording -> run_index < recording -> header.num_runs) && (frame == recording -> run_start_frame)) {
        return recording -> runs[recording -> run_index].event;
    }
    return 0;
}

void input_recording_free(input_recording_t* recording) {
    free(recording -> runs);
    free(recording -> ext_runs);
    recording -> runs = NULL;
    recording -> ext_runs = NULL;
    recording -> capacity = 0;
}

FRNES_COLD_TEXT void input_recording_power_on_ram(const input_recording_t* recording, uint8_t* ram) {
    if (recording -> header.flags & INPUT_RECORDING_FLAG_RAM_ONES) {
        for (uint32_t i = 0; i < 0x800; i++) {
            ram[i] = 0xff;
        }
    } else if (recording -> header.flags & INPUT_RECORDING_FLAG_RAM_FCEUX) {
        for (uint32_t i = 0; i < 0x800; i++) {
            ram[i] = (i & 4) ? 0xff : 0x00;
        }
    }
}

FRNES_COLD_TEXT void input_recording_power_on(const input_recording_t* recording, uint8_t* ram) {
    input_recording_power_on_ram(recording, ram);
    if (recording -> header.flags & INPUT_RECORDING_FLAG_NESHAWK_ALIGN) {
        ppu_set_power_on_dot(5, 0);
    } else if (recording -> header.flags & INPUT_RECORDING_FLAG_NESHAWK_OLD_ALIGN) {
        ppu_set_power_on_dot(3, 1);
    }
}
