#include "vmu_save.h"
#include <kos.h>
#include <dc/vmufs.h>
#include <dc/vmu_pkg.h>
#include <dc/maple/vmu.h>
#include <bzlib/bzlib.h>
#define VMU_ICONS_DEFINE
#include "vmu_icons.h"
#include "memory.h"

/**
 * Battery RAM on the VMU, in old FrNES's format so its saves still load: a file named by the ROM's CRC32 as 8
 * lowercase hex digits, a VMU package (short description "FrNES <version> SAVE", long description the ROM's file
 * name without its extension, app id "FrNES <version>", no icon or eyecatch) whose payload is the RAM compressed
 * with bzip2. Loading takes the first VMU that has the file, saving the first that accepts it. The VMU's screen shows
 * FrNES's loading / saving / error / normal pictures (vmu_icons.h, from old FrNES).
 */
#define VMU_SAVE_APP "FrNES"
#define VMU_SAVE_VERSION "1.0"

// the VMUs the frontend picked (vmu_save_choose): 0 the first that takes the file (or has it), else 1 + port * 2 +
// unit - 1 (A1 1, A2 2, B1 3 ...), tried first; the others after it, so a save isn't lost to a full or missing VMU
static uint8_t vmu_battery_choice FRNES_COLD_BSS, vmu_state_choice FRNES_COLD_BSS;

void vmu_save_choose(uint8_t battery, uint8_t states) {
    vmu_battery_choice = battery;
    vmu_state_choice = states;
}

// the VMUs in the order to try them: the chosen one (when it's there), then the rest; NULL past the last
static maple_device_t* vmu_in_order(int i, uint8_t choice) {
    maple_device_t* chosen = choice ? maple_enum_dev((choice - 1) / 2, (choice - 1) % 2 + 1) : NULL;
    if (chosen && !(chosen -> info.functions & MAPLE_FUNC_MEMCARD)) {
        chosen = NULL;
    }
    if (chosen) {
        if (i == 0) {
            return chosen;
        }
        i--;
    }
    for (int n = 0; ; n++) {
        maple_device_t* vmu = maple_enum_type(n, MAPLE_FUNC_MEMCARD);
        if ((vmu == NULL) || ((vmu != chosen) && (i-- == 0))) {
            return vmu;
        }
    }
}

// old FrNES's routine (from libdream's ghettoplay example): the 48x32 picture, '+' for a dark pixel
static void vmu_save_icon(maple_device_t* vmu, const char* icon) {
    uint8_t bitmap[48 * 32 / 8] = { 0 };
    for (uint32_t y = 0; y < 32; y++) {
        for (uint32_t x = 0; x < 48; x++) {
            if (icon[(31 - y) * 48 + (47 - x)] == '+') {
                bitmap[y * 6 + x / 8] |= 0x80 >> (x % 8);
            }
        }
    }
    vmu_draw_lcd(vmu, bitmap);
}

static void vmu_save_filename(char* name, uint32_t crc) {
    snprintf(name, 13, "%08lx", (unsigned long)crc);
}

void vmu_save_list() {
    int found = 0;
    for (int i = 0; ; i++) {
        maple_device_t* vmu = maple_enum_type(i, MAPLE_FUNC_MEMCARD);
        if (vmu == NULL) {
            break;
        }
        found++;
        vmu_dir_t* entries = NULL;
        int count = 0;
        printf("VMU %c%d: %d blocks free\n", 'A' + vmu -> port, vmu -> unit, vmufs_free_blocks(vmu));
        if ((vmufs_readdir(vmu, &entries, &count) == 0) && entries) {
            for (int e = 0; e < count; e++) {
                char name[13];
                memcpy(name, entries[e].filename, 12);
                name[12] = 0;
                printf("VMU %c%d:   %-12s %3u blocks\n", 'A' + vmu -> port, vmu -> unit, name, entries[e].filesize);
            }
            free(entries);
        }
    }
    if (!found) {
        printf("VMU none attached\n");
    }
}

int vmu_save_available() {
    return maple_enum_type(0, MAPLE_FUNC_MEMCARD) != NULL;
}

int vmu_save_backup_all(const char* directory) {
    char path[160];
    int copied = 0;
    fs_mkdir(directory);
    for (int i = 0; ; i++) {
        maple_device_t* vmu = maple_enum_type(i, MAPLE_FUNC_MEMCARD);
        if (vmu == NULL) {
            break;
        }
        vmu_dir_t* entries = NULL;
        int count = 0;
        if ((vmufs_readdir(vmu, &entries, &count) != 0) || !entries) {
            continue;
        }
        for (int e = 0; e < count; e++) {
            char name[13];
            memcpy(name, entries[e].filename, 12);
            name[12] = 0;
            uint8_t* file = NULL;
            int file_size = 0;
            if (vmufs_read(vmu, name, (void**)&file, &file_size) != 0) {
                printf("VMU backup: unable to read [%s]\n", name);
                continue;
            }
            snprintf(path, sizeof(path), "%s/%c%d_%s.vms", directory, 'A' + vmu -> port, vmu -> unit, name);
            file_t f = fs_open(path, O_WRONLY | O_TRUNC | O_CREAT);
            int ok = (f >= 0) && (fs_write(f, file, file_size) == file_size);
            if (f >= 0) {
                fs_close(f);
            }
            printf("VMU backup of [%s] (%d bytes) to [%s]: %s\n", name, file_size, path, ok ? "ok" : "FAILED");
            copied += ok;
            free(file);
        }
        free(entries);
    }
    return copied;
}

int vmu_save_load(uint32_t crc, uint8_t* data, uint32_t size) {
    char name[13];
    vmu_save_filename(name, crc);
    for (int i = 0; ; i++) {
        maple_device_t* vmu = vmu_in_order(i, vmu_battery_choice);
        if (vmu == NULL) {
            break;
        }
        vmu_save_icon(vmu, vmu_screen_loading);
        uint8_t* file = NULL;
        int file_size = 0;
        if (vmufs_read(vmu, name, (void**)&file, &file_size) != 0) {
            vmu_save_icon(vmu, vmu_screen_normal);
            continue;
        }
        vmu_pkg_t package;
        int result = -1;
        if (vmu_pkg_parse(file, file_size, &package) == 0) {
            unsigned int length = size;
            int bz = BZ2_bzBuffToBuffDecompress((char*)data, &length, (char*)package.data, package.data_len, 0, 0);
            if (bz == BZ_OK) {
                printf("VMU loaded %u bytes from %c%d [%s]\n", length, 'A' + vmu -> port, vmu -> unit, name);
                result = (int)length;
            } else {
                printf("VMU [%s] on %c%d: bzip2 error %d\n", name, 'A' + vmu -> port, vmu -> unit, bz);
            }
        } else {
            printf("VMU [%s] on %c%d: package fails its CRC check\n", name, 'A' + vmu -> port, vmu -> unit);
        }
        free(file);
        vmu_save_icon(vmu, (result >= 0) ? vmu_screen_normal : vmu_screen_error);
        if (result >= 0) {
            return result;
        }
    }
    return -1;
}

void (*vmu_save_yield)(void) = NULL;

// bzip2 (block size 500 kB, as BZ2_bzBuffToBuffCompress was called) fed 4 kB at a time, vmu_save_yield between
// pieces, so a save written by a background thread hands the CPU back often: the same stream as in one call
#define VMU_COMPRESS_SLICE 4096
static int vmu_compress(char* out, unsigned int* out_size, const char* in, unsigned int in_size) {
    bz_stream stream;
    memset(&stream, 0, sizeof(stream));
    int bz = BZ2_bzCompressInit(&stream, 5, 0, 30);
    if (bz != BZ_OK) {
        return bz;
    }
    stream.next_out = out;
    stream.avail_out = *out_size;
    unsigned int fed = 0;
    for (;;) {
        unsigned int slice = in_size - fed;
        if (slice > VMU_COMPRESS_SLICE) {
            slice = VMU_COMPRESS_SLICE;
        }
        stream.next_in = (char*)in + fed;
        stream.avail_in = slice;
        fed += slice;
        int action = (fed == in_size) ? BZ_FINISH : BZ_RUN;
        do {
            bz = BZ2_bzCompress(&stream, action);
        } while ((bz == BZ_RUN_OK) && stream.avail_in);
        if ((bz == BZ_STREAM_END) || ((bz != BZ_RUN_OK) && (bz != BZ_FINISH_OK))) {
            break;
        }
        if (vmu_save_yield) {
            vmu_save_yield();
        }
    }
    *out_size = stream.total_out_lo32;
    BZ2_bzCompressEnd(&stream);
    return (bz == BZ_STREAM_END) ? BZ_OK : ((bz == BZ_FINISH_OK) ? BZ_OUTBUFF_FULL : bz);
}

/*
 * A VMU file: a package (short description "FrNES <version> <kind>", long description the ROM's file name without its
 * extension) holding data, written to the first VMU that takes it (raw_size: what data is the compression of, for the
 * log); 0 on success
 */
static int vmu_write_file(const char* name, const char* kind, const char* data, unsigned int length, uint32_t raw_size,
    const char* rom_path) {
    vmu_pkg_t package;
    memset(&package, 0, sizeof(package));
    snprintf(package.desc_short, sizeof(package.desc_short), "%s %s %s", VMU_SAVE_APP, VMU_SAVE_VERSION, kind);
    // the ROM's file name without its directory or extension
    const char* base = strrchr(rom_path, '/');
    base = base ? base + 1 : rom_path;
    snprintf(package.desc_long, sizeof(package.desc_long), "%s", base);
    char* dot = strrchr(package.desc_long, '.');
    if (dot) {
        *dot = 0;
    }
    snprintf(package.app_id, sizeof(package.app_id), "%s %s", VMU_SAVE_APP, VMU_SAVE_VERSION);
    package.eyecatch_type = VMUPKG_EC_NONE;
    package.data_len = length;
    package.data = (const uint8_t*)data;
    uint8_t* file = NULL;
    int file_size = 0;
    int result = -1;
    if (vmu_pkg_build(&package, &file, &file_size) == 0) {
        for (int i = 0; ; i++) {
            maple_device_t* vmu = vmu_in_order(i, (strcmp(kind, "STATE") == 0) ? vmu_state_choice : vmu_battery_choice);
            if (vmu == NULL) {
                break;
            }
            vmu_save_icon(vmu, vmu_screen_saving);
            uint64_t start = timer_us_gettime64();
            if (vmufs_write(vmu, name, file, file_size, VMUFS_OVERWRITE) == 0) {
                printf("VMU saved %lu bytes (%d in the file) to %c%d [%s] in %lu ms\n", (unsigned long)raw_size, file_size,
                    'A' + vmu -> port, vmu -> unit, name, (unsigned long)((timer_us_gettime64() - start) / 1000));
                vmu_save_icon(vmu, vmu_screen_normal);
                result = 0;
                break;
            }
            printf("VMU unable to save [%s] to %c%d (%d blocks free)\n", name, 'A' + vmu -> port, vmu -> unit,
                vmufs_free_blocks(vmu));
            vmu_save_icon(vmu, vmu_screen_error);
        }
        free(file);
    }
    return result;
}

int vmu_save_write(uint32_t crc, const uint8_t* data, uint32_t size, const char* rom_path) {
    if (!vmu_save_available()) {
#ifdef FRNES_AUTOSAVE_TEST
        // (the autosave test on a console without a VMU: the compression's cost all the same)
        unsigned int length = size + size / 100 + 600;
        char* buffer = malloc(length);
        if (buffer) {
            uint64_t start = timer_us_gettime64();
            vmu_compress(buffer, &length, (const char*)data, size);
            printf("VMU none: compressed %lu bytes to %u in %lu ms anyway\n", (unsigned long)size, length,
                (unsigned long)((timer_us_gettime64() - start) / 1000));
            free(buffer);
        }
#endif
        return -1;
    }
    // bzip2 needs a little more than the input for incompressible data
    unsigned int compressed_size = size + size / 100 + 600;
    char* compressed = malloc(compressed_size);
    if (compressed == NULL) {
        return -1;
    }
    uint64_t compress_start = timer_us_gettime64();
    int bz = vmu_compress(compressed, &compressed_size, (const char*)data, size);
    printf("VMU compressed %lu bytes to %u in %lu ms\n", (unsigned long)size, compressed_size,
        (unsigned long)((timer_us_gettime64() - compress_start) / 1000));
    if (bz != BZ_OK) {
        printf("VMU bzip2 compression error %d\n", bz);
        free(compressed);
        return -1;
    }
    char name[13];
    vmu_save_filename(name, crc);
    int result = vmu_write_file(name, "SAVE", compressed, compressed_size, size, rom_path);
    free(compressed);
    return result;
}

/*
 * Save states on the VMU: a file "<crc>.s<slot>" (kind "STATE") whose payload is the state's size (4 bytes, little
 * endian) and then the state compressed with bzip2 (a state is mostly RAM and zeros: an NROM game's 18 kB takes a few
 * blocks)
 */
static void vmu_state_filename(char* name, uint32_t crc, int slot) {
    snprintf(name, 13, "%08lx.s%d", (unsigned long)crc, slot);
}

int vmu_state_write(uint32_t crc, int slot, const uint8_t* data, uint32_t size, const char* rom_path) {
    if (!vmu_save_available()) {
        return -1;
    }
    unsigned int compressed_size = size + size / 100 + 600;
    char* buffer = malloc(4 + compressed_size);
    if (buffer == NULL) {
        return -1;
    }
    buffer[0] = size & 0xff;
    buffer[1] = (size >> 8) & 0xff;
    buffer[2] = (size >> 16) & 0xff;
    buffer[3] = (size >> 24) & 0xff;
    uint64_t compress_start = timer_us_gettime64();
    int bz = vmu_compress(buffer + 4, &compressed_size, (const char*)data, size);
    printf("VMU state compressed %lu bytes to %u in %lu ms\n", (unsigned long)size, compressed_size,
        (unsigned long)((timer_us_gettime64() - compress_start) / 1000));
    int result = -1;
    if (bz == BZ_OK) {
        char name[13];
        vmu_state_filename(name, crc, slot);
        result = vmu_write_file(name, "STATE", buffer, 4 + compressed_size, size, rom_path);
    } else {
        printf("VMU state bzip2 compression error %d\n", bz);
    }
    free(buffer);
    return result;
}

int vmu_state_load(uint32_t crc, int slot, uint8_t** data, uint32_t* size) {
    char name[13];
    vmu_state_filename(name, crc, slot);
    for (int i = 0; ; i++) {
        maple_device_t* vmu = vmu_in_order(i, vmu_state_choice);
        if (vmu == NULL) {
            break;
        }
        uint8_t* file = NULL;
        int file_size = 0;
        if (vmufs_read(vmu, name, (void**)&file, &file_size) != 0) {
            continue;
        }
        vmu_save_icon(vmu, vmu_screen_loading);
        int result = -1;
        vmu_pkg_t package;
        if ((vmu_pkg_parse(file, file_size, &package) == 0) && (package.data_len > 4)) {
            const uint8_t* payload = package.data;
            uint32_t raw = payload[0] | (payload[1] << 8) | (payload[2] << 16) | ((uint32_t)payload[3] << 24);
            uint8_t* out = malloc(raw);
            unsigned int length = raw;
            int bz = out ? BZ2_bzBuffToBuffDecompress((char*)out, &length, (char*)payload + 4, package.data_len - 4, 0, 0) : BZ_MEM_ERROR;
            if ((bz == BZ_OK) && (length == raw)) {
                printf("VMU state loaded %u bytes from %c%d [%s]\n", length, 'A' + vmu -> port, vmu -> unit, name);
                *data = out;
                *size = raw;
                result = 0;
            } else {
                printf("VMU state [%s] on %c%d: bzip2 error %d\n", name, 'A' + vmu -> port, vmu -> unit, bz);
                free(out);
            }
        } else {
            printf("VMU state [%s] on %c%d: not a state package\n", name, 'A' + vmu -> port, vmu -> unit);
        }
        free(file);
        vmu_save_icon(vmu, (result == 0) ? vmu_screen_normal : vmu_screen_error);
        if (result == 0) {
            return 0;
        }
    }
    return -1;
}
