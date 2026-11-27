/**
 * Famicom Disk System: the RAM adaptor (the 2C33's disk and timer registers) and the disk drive, written from the
 * nesdev wiki's "Family Computer Disk System", "FDS disk format" and "FDS BIOS" pages and Brad Taylor's "FDS technical
 * reference" (archive.nesdev.org), where the wiki is newer the wiki's word taken:
 *
 * - load_nes_file hands an .fds image (a 16 byte "FDS\x1a" header with the side count, or bare 65500 byte sides; the
 *   wiki's "FDS file format") to fds_cartridge, which makes it a cartridge for mapper 20 (the iNES number the wiki
 *   reserves for FDS dumps): the 8 kB BIOS (twice, one 16 kB iNES PRG bank) and 32 kB of PRG RAM, then the disk image
 *   itself (so the ROM CRC32 is the disk's).
 * - $6000-$DFFF is PRG RAM (WRAM's 32 kB: the $6000 window and the $8000 / $A000 / $C000 slots, written through the
 *   NES window in fds_write), $E000 the BIOS, 8 kB of CHR RAM.
 * - Each side is kept as the drive sees it ("FDS disk format", Gaps and CRCs): 28300 bits of 0s, then each block as
 *   the gap's closing 1 bit (a byte $80, the data being sent least significant bit first), its bytes and their
 *   CRC-16/KERMIT (the $80 included), then 976 bits of 0s. The bit rate is 96.4 kHz: the 2C33 has a byte every 1792
 *   master clocks, 149 1/3 CPU cycles.
 * - The IRQ timer ($4020-$4022) and the byte clock are CPU cycle counters run by mapper_cpu_run, each raising its own
 *   mapper_irq_flag bit. The batch ends at the next IRQ (fds_cycles_to_event); register accesses catch up first.
 * - Sides change by hand (fds_insert_side: the player's, an input recording event); an optional guess of ours does it
 *   for the test builds (fds_auto_insert, see auto_side_at_scan).
 * - The sound unit's registers ($4040-$408A) go to apu_synth (APU_EXPANSION_FDS) through its write log; reads of
 *   the wave RAM and the gains ($4090 / $4092) come from the copies kept here.
 * - Written sides are saved like battery RAM (fds_battery_data: the sides XORed with the image as loaded, mostly
 *   zeros, which compress to almost nothing on the VMU).
 *
 * This file is built without LTO (see mapper_boards.c), and nothing in it is reached from the PPU's per scanline
 * mapper clock.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fds.h"
#include "mapper.h"
#include "memory.h"
#include "cartridge.h"
#include "ppu.h"
#include "cpu.h"
#include "apu_synth.h"
#include "input_recording.h"

#define FDS_SIDE_SIZE 65500
#define FDS_NO_DISK 0xff
// mapper_irq_flag bits
#define FDS_IRQ_TIMER 0x02
#define FDS_IRQ_DISK 0x04
// the gaps ("FDS disk format": 28300 bits before the first block, 976 between blocks, typical), in bytes
#define LEAD_GAP_BYTES (28300 / 8)
#define BLOCK_GAP_BYTES (976 / 8)
// "-ready" comes at least 14354 bits after the scan starts (Taylor, the drive's signals, step 5), in bytes (rounded up)
#define HEAD_RETURN_BYTES ((14354 + 7) / 8)

// $4025's bits (the wiki's names)
#define CTRL_SCAN 0x01         // 0: transfer reset, 1: scan
#define CTRL_MOTOR_STOP 0x02   // 0: motor on, 1: stop
#define CTRL_READ 0x04         // 1: read, 0: write
#define CTRL_MIRROR 0x08       // 1: horizontal mirroring
#define CTRL_CRC_OUT 0x10      // write: the CRC register goes to the disk instead of $4024
#define CTRL_CRC_ON 0x40       // 0: CRC reset (and no transfers), 1: transfers, the CRC accumulates
#define CTRL_IRQ 0x80          // an IRQ while the byte transfer flag is up

// the automatic side changes' frame counts (ours)
#define AUTO_POLL_FRAMES 30
#define AUTO_OUT_FRAMES 60
// a side changed by hand: frames the disk is out. A game asking for another side may want the disk out a while before
// it takes the new one: Metroid's wait counts 32 frames with no disk and starts over if a disk comes in before that
// (30 here left it waiting at INSERT SIDE B for good)
#define HAND_OUT_FRAMES 60

// (FRNES_FDS_BIOS_PATH: looked at first, e.g. a dev setup serving another folder as /pc; then the SD card's and the
// disc's FrNES folders, where a release's player puts it, the disc's root, and the dev setups' /pc)
static const char* const bios_paths[] = {
#ifdef FRNES_FDS_BIOS_PATH
    FRNES_FDS_BIOS_PATH,
#endif
    "/sd/FrNES/disksys.rom", "/cd/FrNES/disksys.rom", "/cd/disksys.rom", "/rd/disksys.rom", "/pc/roms/disksys.rom",
    "/pc/disksys.rom" };

static uint8_t side_count;
static uint8_t* sides[16];
// the sides as loaded (the save is the difference), and the save's buffer
static uint8_t* original_sides[16];
static uint8_t* save_buffer;
static uint32_t save_size;
static uint32_t side_sizes[16];
// the 256 byte blocks of each side written since the save was last brought up to date (fds_battery_prepare)
#define FDS_BLOCKS ((FDS_SIDE_SIZE + 0x4000) / 256 + 1)
static uint8_t dirty_blocks[16][FDS_BLOCKS], any_dirty;
static uint8_t headers[16][56];

/*
 * The adaptor and the drive. (Named fds and kept within 64 bytes: tools/bss_order places .bss by name and size)
 */
static struct {
    // the timer
    uint16_t timer_reload, timer_counter;
    uint8_t timer_on, timer_repeat;
    // $4023 bit 0 / bit 1, $4025, $4024, $4031, $4026
    uint8_t disk_io, sound_io, control, write_data, read_data, ext_out;
    // the byte transfer flag ($4030.D7), the head at the inner end ($4030.D6), "-ready", a block start mark read
    uint8_t byte_flag, end_of_head, ready, in_block;
    // write: the shift register being written, and whether it holds data from $4024 (the CRC's bytes don't count)
    uint8_t shift, shift_data;
    // the byte clock runs (motor_running, or a write with the CRC enabled), its period in the 3 (149, 149, 150)
    uint8_t clock_on, period;
    // the side in the drive, or FDS_NO_DISK; one going in by hand, at frame manual_frame
    uint8_t side, manual_side;
    // the automatic changes: off, waiting for a scan before the next one, the side taken out
    uint8_t auto_off, auto_armed, auto_side;
    uint8_t reserved;
    uint16_t crc;
    // cycles to the byte clock's next byte, byte periods before "-ready"
    uint16_t next_byte, return_bytes;
    // the head's byte on the side
    uint32_t head;
    uint32_t manual_frame;
    // the automatic changes: the frame $4032 was last read with the motor off, frames in a row of that, frames out
    uint32_t poll_frame;
    uint16_t poll_frames, out_frames;
} fds;
_Static_assert(sizeof(fds) <= 64, "fds: within the 64 bytes tools/bss_order gives it");
// (the save state's tag: a state from before this drive has another size and is refused, see fds_state_load)
static const uint8_t fds_state_tag[4] FRNES_COLD_RODATA = { 'F', 'D', 'R', '2' };

// the sound registers as the CPU reads them back: the wave RAM, the gains last set directly, $4089 bit 7
static uint8_t sound_wave[64], sound_vol_gain, sound_mod_gain, sound_write_enable;
// the CPU cycle the counters have been run up to
static uint32_t fds_cycle;
// automatic side changes (Matt's choice: the player changes sides by hand, the right trigger; disk 1 side A is in at
// power on): off unless set before mapper_init (the host build's HOST_FDS_AUTO, the test build's AC_FDS_AUTO)
uint8_t fds_auto_insert = 0;

static int is_fds_image(const uint8_t* data, uint32_t size) {
    if ((size >= 16) && !memcmp(data, "FDS\x1a", 4)) {
        return 1;
    }
    return (size >= FDS_SIDE_SIZE) && ((size % FDS_SIDE_SIZE) == 0) && (data[0] == 0x01) && !memcmp(&data[1], "*NINTENDO-HVC*", 14);
}

// CRC-16/KERMIT, one byte (the 2C33's: shifting right, the polynomial $8408; Taylor, "CRC calculations")
static uint16_t crc_byte(uint16_t crc, uint8_t value) {
    crc ^= value;
    for (uint32_t n = 0; n < 8; n++) {
        crc = (crc & 1) ? (crc >> 1) ^ 0x8408 : crc >> 1;
    }
    return crc;
}

// a block's length from its first byte ("FDS disk format": 1 the disk info, 2 the file amount, 3 a file header, 4
// the file's data, its size from the last header's bytes $0D-$0E), 0 for anything else
static uint32_t block_length(const uint8_t* block, uint32_t room, uint32_t* file_size) {
    switch (block[0]) {
        case 1: return 56;
        case 2: return 2;
        case 3:
            *file_size = (room > 14) ? (block[13] | (block[14] << 8)) : 0;
            return 16;
        case 4: return 1 + *file_size;
    }
    return 0;
}

// the side as the drive sees it, from its 65500 bytes in the image: its length
static uint32_t side_on_disk(uint8_t* out, const uint8_t* side) {
    uint32_t n = LEAD_GAP_BYTES, j = 0, file_size = 0;
    memset(out, 0, n);
    while (j < FDS_SIDE_SIZE) {
        uint32_t length = block_length(&side[j], FDS_SIDE_SIZE - j, &file_size);
        if (length == 0) {
            // (no block here: the rest of the side as it is)
            out[n++] = 0x80;
            memcpy(&out[n], &side[j], FDS_SIDE_SIZE - j);
            return n + FDS_SIDE_SIZE - j;
        }
        if (j + length >= FDS_SIDE_SIZE) {
            break;
        }
        uint16_t crc = crc_byte(0, 0x80);
        out[n++] = 0x80;
        for (uint32_t i = 0; i < length; i++) {
            crc = crc_byte(crc, side[j + i]);
            out[n++] = side[j + i];
        }
        out[n++] = crc & 0xff;
        out[n++] = crc >> 8;
        memset(&out[n], 0, BLOCK_GAP_BYTES);
        n += BLOCK_GAP_BYTES;
        j += length;
    }
    return n;
}

/*
 * The blocks of a side as the drive would find them (gaps of 0s, a start mark $80, a block, its CRC): 'visit' called
 * with each block's CRC position (the CRC's 2 bytes) and its CRC worked out from the bytes. -1 when a gap holds anything
 * but 0s before a start mark, else how many CRCs were wrong. (A block of an unknown type ends the walk: the rest of the
 * side was copied as it is, side_on_disk)
 */
static int side_walk(uint8_t* side, uint32_t size, void (*visit)(uint8_t* side, uint32_t at, uint16_t crc)) {
    uint32_t j = 0, file_size = 0;
    int wrong = 0;
    for (;;) {
        while ((j < size) && (side[j] == 0)) {
            j++;
        }
        if (j + 1 >= size) {
            return wrong;
        }
        if (side[j] != 0x80) {
            return -1;
        }
        uint32_t length = block_length(&side[j + 1], size - j - 1, &file_size);
        if (length == 0) {
            return wrong;
        }
        if (j + 1 + length + 2 > size) {
            return -1;
        }
        uint16_t crc = 0;
        for (uint32_t i = 0; i <= length; i++) {
            crc = crc_byte(crc, side[j + i]);
        }
        j += 1 + length;
        wrong += (side[j] != (crc & 0xff)) || (side[j + 1] != (crc >> 8));
        if (visit) {
            visit(side, j, crc);
        }
        j += 2;
    }
}

static void crc_put(uint8_t* side, uint32_t at, uint16_t crc) {
    side[at] = crc & 0xff;
    side[at + 1] = crc >> 8;
}

// (FrNES's sides before this drive had 2 fixed bytes, $4D $62, where each block's CRC goes: a save made then is the
// difference from those)
static uint8_t* old_layout_side;
static void old_layout_crc(uint8_t* original, uint32_t at, uint16_t crc) {
    (void)crc;
    old_layout_side[at] ^= original[at] ^ 0x4d;
    old_layout_side[at + 1] ^= original[at + 1] ^ 0x62;
}

static uint8_t* load_bios() {
    for (uint32_t i = 0; i < sizeof(bios_paths) / sizeof(bios_paths[0]); i++) {
        file_t f = fs_open(bios_paths[i], O_RDONLY);
        if (f == -1) {
            continue;
        }
        uint8_t* bios = malloc(0x2000);
        ssize_t got = fs_read(f, bios, 0x2000);
        fs_close(f);
        if (got == 0x2000) {
            printf(" - FDS BIOS from [%s]\n", bios_paths[i]);
            return bios;
        }
        free(bios);
    }
    printf(" ...ERROR: no FDS BIOS (disksys.rom) found\n");
    return NULL;
}

uint8_t* fds_cartridge(uint8_t* data, uint32_t* size) {
    if (!is_fds_image(data, *size)) {
        return data;
    }
    uint32_t offset = 0, count;
    if (!memcmp(data, "FDS\x1a", 4)) {
        offset = 16;
        count = data[4];
    } else {
        count = *size / FDS_SIDE_SIZE;
    }
    if (count > 16) {
        count = 16;
    }
    while (count && (offset + count * FDS_SIDE_SIZE > *size)) {
        count--;
    }
    uint8_t* bios = load_bios();
    if (!bios || !count) {
        free(bios);
        free(data);
        return NULL;
    }
    printf(" - FDS image, %lu side(s)\n", (unsigned long)count);
    for (uint32_t i = 0; i < side_count; i++) {
        free(sides[i]);
        free(original_sides[i]);
    }
    free(save_buffer);
    save_buffer = NULL;
    save_size = 0;
    side_count = count;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t* side = &data[offset + i * FDS_SIDE_SIZE];
        memcpy(headers[i], &side[1], 56);
        // (the gaps, start marks and CRCs add a few kB; the stream is at least a side long; FDS_BLOCKS covers the same)
        sides[i] = calloc(1, FDS_SIDE_SIZE + 0x4000);
        side_sizes[i] = side_on_disk(sides[i], side);
        if (side_sizes[i] < FDS_SIDE_SIZE) {
            side_sizes[i] = FDS_SIDE_SIZE;
        }
        original_sides[i] = malloc(side_sizes[i]);
        memcpy(original_sides[i], sides[i], side_sizes[i]);
        save_size += side_sizes[i];
    }
    save_buffer = calloc(1, save_size);
    memset(dirty_blocks, 0, sizeof(dirty_blocks));
    any_dirty = 0;
    // an iNES cartridge: mapper 20, 16 kB PRG (the BIOS twice), 8 kB CHR RAM, 32 kB PRG RAM, vertical mirroring; then
    // the disk image, which the ROM CRC32 is computed over too
    uint32_t disk_bytes = *size - offset;
    uint8_t* cart = malloc(16 + 0x4000 + disk_bytes);
    static const uint8_t header[16] = { 'N', 'E', 'S', 0x1a, 1, 0, 0x41, 0x10, 4, 0, 0, 0, 0, 0, 0, 0 };
    memcpy(cart, header, 16);
    memcpy(&cart[16], bios, 0x2000);
    memcpy(&cart[16 + 0x2000], bios, 0x2000);
    memcpy(&cart[16 + 0x4000], &data[offset], disk_bytes);
    free(bios);
    free(data);
    *size = 16 + 0x4000 + disk_bytes;
    return cart;
}

// (the disk turning: a side in, the scan on and the motor on: $4025 bits 0-1 = 01, Taylor's step 3)
static int motor_running() {
    return (fds.side != FDS_NO_DISK) && ((fds.control & (CTRL_SCAN | CTRL_MOTOR_STOP)) == CTRL_SCAN);
}

// the byte clock matters while the disk turns, or while a write with the CRC enabled raises the byte transfer flag
// (the wiki: in write mode the motor doesn't matter)
static int clock_wanted() {
    return motor_running() || ((fds.control & (CTRL_READ | CTRL_CRC_ON)) == CTRL_CRC_ON);
}

// the counters run while the timer or the byte clock does (mapper_cpu_timer); a clock starting starts a byte period
static void fds_update_timer() {
    uint8_t clock = clock_wanted();
    if (clock && !fds.clock_on) {
        fds.next_byte = 149;
        fds.period = 0;
    }
    fds.clock_on = clock;
    uint8_t running = fds.timer_on || clock;
    if (running && !mapper_cpu_timer) {
        mapper_timer_start(cpu_cycle_now);
        fds_cycle = cpu_cycle_now;
    }
    mapper_cpu_timer = running;
}

// the IRQ line follows the byte transfer flag while $4025.D7 is set
static void disk_irq_update(uint32_t cycle) {
    if (fds.byte_flag && (fds.control & CTRL_IRQ)) {
        cpu_irq_raise_at(FDS_IRQ_DISK, cycle);
    } else {
        cpu_irq_clear(FDS_IRQ_DISK);
    }
}

static void raise_byte_flag(uint32_t cycle) {
    fds.byte_flag = 1;
    if (fds.control & CTRL_IRQ) {
        cpu_irq_raise_at(FDS_IRQ_DISK, cycle);
    }
}

static void disk_store(uint8_t value) {
    uint8_t* at = &sides[fds.side][fds.head];
    if (*at != value) {
        *at = value;
        mapper_save_written = 1;
        dirty_blocks[fds.side][fds.head >> 8] = 1;
        any_dirty = 1;
    }
}

/*
 * One byte period of the byte clock, ending at 'cycle'. The head: while it goes back to the start, no data ("-ready"
 * comes HEAD_RETURN_BYTES after the scan started); then a byte of the side passes each period; past the side's end the
 * head is at the inner end ($4030.D6, $4032.D1), the data 0s, the transfers going on (the wiki: the BIOS relies on
 * the flag past the end) until the scan starts again. The adaptor ($4025, Taylor and the wiki):
 * - read, CRC enabled: the 0s of a gap are skipped up to the start mark (a non-zero byte, the $80), which goes into the
 *   CRC; then every byte goes to $4031 and the CRC and raises the flag;
 * - write, CRC enabled: the shift register goes to the disk (and its data byte into the CRC), then is loaded with the
 *   next byte, $4024 (raising the flag) or, with $4025.D4, the CRC register's low byte (the CRC sent, the flag not);
 * - CRC disabled: the CRC reset, no flag; a write writes 0s.
 */
static void byte_period(uint32_t cycle) {
    uint8_t on_side = 0, data = 0;
    if (motor_running()) {
        if (fds.return_bytes) {
            if (--fds.return_bytes == 0) {
                fds.ready = 1;
                fds.head = 0;
            }
            return;
        }
        if (fds.head < side_sizes[fds.side]) {
            on_side = 1;
            data = sides[fds.side][fds.head];
        } else {
            fds.end_of_head = 1;
            fds.ready = 0;
        }
    }
    uint8_t crc_on = (fds.control & CTRL_CRC_ON) != 0;
    if (fds.control & CTRL_READ) {
        if (motor_running() && crc_on) {
            if (!fds.in_block) {
                if (data) {
                    fds.in_block = 1;
                    fds.crc = crc_byte(fds.crc, data);
                }
            } else {
                fds.read_data = data;
                fds.crc = crc_byte(fds.crc, data);
                raise_byte_flag(cycle);
            }
        }
    } else if (crc_on) {
        if (on_side) {
            disk_store(fds.shift);
        }
        if (fds.shift_data) {
            fds.crc = crc_byte(fds.crc, fds.shift);
        }
        if (fds.control & CTRL_CRC_OUT) {
            fds.shift = fds.crc & 0xff;
            fds.crc >>= 8;
            fds.shift_data = 0;
        } else {
            fds.shift = fds.write_data;
            fds.shift_data = 1;
            raise_byte_flag(cycle);
        }
    } else if (on_side) {
        disk_store(0);
    }
    if (on_side) {
        fds.head++;
    }
}

static uint32_t period_length(uint8_t period) {
    return (period == 2) ? 150 : 149;
}

void fds_cpu_run(uint32_t cycles) {
    uint32_t cycle = fds_cycle;
    fds_cycle += cycles;
    while (cycles) {
        uint32_t n = cycles;
        if (fds.timer_on && ((uint32_t)fds.timer_counter + 1 < n)) {
            n = (uint32_t)fds.timer_counter + 1;
        }
        if (fds.clock_on && (fds.next_byte < n)) {
            n = fds.next_byte;
        }
        cycle += n;
        cycles -= n;
        // (the timer: the wiki's $4022 - each cycle, at 0 an IRQ and the reload, else one down)
        if (fds.timer_on) {
            if ((uint32_t)fds.timer_counter + 1 == n) {
                cpu_irq_raise_at(FDS_IRQ_TIMER, cycle);
                fds.timer_counter = fds.timer_reload;
                fds.timer_on = fds.timer_repeat;
            } else {
                fds.timer_counter -= n;
            }
        }
        if (fds.clock_on) {
            fds.next_byte -= n;
            if (fds.next_byte == 0) {
                byte_period(cycle);
                fds.period = (fds.period + 1) % 3;
                fds.next_byte = period_length(fds.period);
            }
        }
        if (!fds.timer_on && !fds.clock_on) {
            mapper_cpu_timer = 0;
            break;
        }
    }
}

// cycles from fds_cycle to the end of the n-th byte period from now (n >= 1)
static uint32_t periods_to_cycles(uint32_t n) {
    uint32_t cycles = fds.next_byte;
    uint8_t period = fds.period;
    for (n--; n; n--) {
        period = (period + 1) % 3;
        cycles += period_length(period);
        if (cycles > 0x40000000) {
            break;
        }
    }
    return cycles;
}

/*
 * Cycles until the byte transfer flag next goes up with $4025.D7 set (fds_cycles_to_event): the bytes before it only
 * change what $4030-$4032 read, and those reads (and the register writes) catch the drive up first, so they don't end
 * batches. Reads: the start mark's search over the gap's 0s, then every byte; writes: every byte but the CRC's.
 */
static uint32_t disk_to_irq() {
    if (!fds.clock_on || !(fds.control & CTRL_IRQ) || !(fds.control & CTRL_CRC_ON)) {
        return 0xffffffff;
    }
    if (!(fds.control & CTRL_READ)) {
        return (fds.control & CTRL_CRC_OUT) ? 0xffffffff : periods_to_cycles(1);
    }
    if (!motor_running()) {
        return 0xffffffff;
    }
    uint32_t n = fds.return_bytes, head = fds.return_bytes ? 0 : fds.head;
    if (!fds.in_block) {
        // (the start mark's period, the flag one later; past the end only 0s)
        uint32_t size = side_sizes[fds.side];
        const uint8_t* side = sides[fds.side];
        while ((head < size) && !side[head]) {
            head++;
            n++;
        }
        if (head >= size) {
            return 0xffffffff;
        }
        n++;
    }
    return periods_to_cycles(n + 1);
}

uint32_t fds_cycles_to_event() {
    uint32_t ti = fds.timer_on ? (uint32_t)fds.timer_counter + 1 : 0xffffffff, td = disk_to_irq();
    return (ti < td) ? ti : td;
}

static void catch_up() {
    if (mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_now);
    }
}

void fds_init() {
    memset(&fds, 0, sizeof(fds));
    fds.side = side_count ? 0 : FDS_NO_DISK;
    fds.manual_side = FDS_NO_DISK;
    fds.auto_side = FDS_NO_DISK;
    fds.control = 0x06;
    fds.ext_out = 0x7f;
    fds.end_of_head = 1;
    fds.auto_off = !fds_auto_insert;
    fds.auto_armed = 1;
    resize_wram(0x8000);
    set_wram_bank_pointer(WRAM);
    for (uint32_t i = 0; i < 3; i++) {
        set_6502_bank_pointer(i, WRAM + 0x2000 * (i + 1));
    }
    set_6502_bank_pointer(3, ROM_pages[0]);
    for (uint32_t i = 0; i < 8; i++) {
        set_ppu_pattern_table_bank_pointer(i, VROM_pages[i]);
    }
    set_ppu_nametable_mirroring(NAMETABLE_MIRRORING_VERTICAL);
    mapper_cpu_timer = 0;
    memset(sound_wave, 0, sizeof(sound_wave));
    sound_vol_gain = sound_mod_gain = sound_write_enable = 0;
    apu_synth_set_expansion(APU_EXPANSION_FDS);
}

// $8000-$DFFF: PRG RAM (written through the NES window, the address the CPU reads it at)
void fds_write(uint16_t address, uint8_t data) {
    if (address < 0xe000) {
        NES_WINDOW_BYTE(address) = data;
    }
}

/*
 * The automatic side changes (fds_auto_insert, ours; off in the player). When a scan starts, the BIOS is about to
 * compare the disk's ID with the 10 bytes at the pointer in $00 / $01 (the wiki's FDS BIOS, CheckDiskHeader at $E445;
 * $FF matches anything): if exactly one side matches, it goes in. A game waiting for a disk change polls $4032 with the
 * motor off: after AUTO_POLL_FRAMES frames of that in a row, the disk comes out, and AUTO_OUT_FRAMES later the next side
 * goes in (the scan's match then puts the right one in); once per disk access.
 */
static void auto_side_at_scan() {
    fds.auto_armed = 1;
    fds.poll_frames = 0;
    if (fds.auto_off || (fds.side == FDS_NO_DISK)) {
        return;
    }
    uint16_t pointer = NES_WINDOW_BYTE(0) | (NES_WINDOW_BYTE(1) << 8);
    uint8_t wanted[10];
    for (uint32_t i = 0; i < 10; i++) {
        uint16_t a = pointer + i;
        // (RAM, PRG RAM or the BIOS; anything else can't be an ID)
        if ((a >= 0x0800) && (a < 0x6000)) {
            return;
        }
        wanted[i] = NES_WINDOW_BYTE(a);
    }
    int matches = 0, match = -1;
    for (uint32_t s = 0; s < side_count; s++) {
        uint32_t i = 0;
        while ((i < 10) && ((wanted[i] == 0xff) || (wanted[i] == headers[s][14 + i]))) {
            i++;
        }
        if (i == 10) {
            matches++;
            match = s;
        }
    }
    if ((matches == 1) && (fds.side != match)) {
        printf("FDS: side %d in, its ID asked for (frame %lu)\n", match, (unsigned long)ppu_frame_count);
        fds.side = match;
    }
}

static void auto_side_poll() {
    if (fds.auto_off || !fds.auto_armed || (fds.side == FDS_NO_DISK) || motor_running()) {
        return;
    }
    if (fds.poll_frame == ppu_frame_count) {
        return;
    }
    fds.poll_frames = (ppu_frame_count - fds.poll_frame == 1) ? fds.poll_frames + 1 : 1;
    fds.poll_frame = ppu_frame_count;
    if (fds.poll_frames >= AUTO_POLL_FRAMES) {
        fds.auto_armed = 0;
        fds.auto_side = fds.side;
        fds.side = FDS_NO_DISK;
        fds.out_frames = AUTO_OUT_FRAMES;
        printf("FDS: disk out, a change waited for (frame %lu)\n", (unsigned long)ppu_frame_count);
    }
}

// once a frame: a side changed by hand going in, the automatic change's disk going back in
static void side_frame() {
    if ((fds.manual_side != FDS_NO_DISK) && ((int32_t)(ppu_frame_count - fds.manual_frame) >= 0)) {
        fds.side = fds.manual_side;
        fds.manual_side = FDS_NO_DISK;
        printf("FDS: side %u inserted by hand (frame %lu)\n", fds.side, (unsigned long)ppu_frame_count);
    }
    if (fds.out_frames && (--fds.out_frames == 0) && (fds.side == FDS_NO_DISK) && (fds.manual_side == FDS_NO_DISK)) {
        fds.side = (side_count > 1) ? (fds.auto_side + 1) % side_count : 0;
        printf("FDS: side %u in (frame %lu)\n", fds.side, (unsigned long)ppu_frame_count);
    }
}

static void write_control(uint8_t data) {
    uint8_t was_running = motor_running(), old = fds.control;
    fds.control = data;
    if ((old ^ data) & CTRL_MIRROR) {
        ppu_sync();
    }
    set_ppu_nametable_mirroring((data & CTRL_MIRROR) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
    if (!(data & CTRL_CRC_ON)) {
        // (the CRC disabled: reset, and a read waits for the next start mark)
        fds.crc = 0;
        fds.in_block = 0;
        fds.shift_data = 0;
    } else if (!(data & CTRL_READ) && (!(old & CTRL_CRC_ON) || (old & CTRL_READ))) {
        // (a write's CRC enabled: $4024 into the shift register at once, the flag up; Taylor, $4025 bit 6)
        fds.shift = fds.write_data;
        fds.shift_data = 1;
        fds.byte_flag = 1;
    }
    if (!was_running && motor_running()) {
        // the scan starts: the head goes back to the start, "-ready" after HEAD_RETURN_BYTES
        if (!fds.auto_off) {
            auto_side_at_scan();
        }
        fds.ready = 0;
        fds.end_of_head = 0;
        fds.head = 0;
        fds.return_bytes = HEAD_RETURN_BYTES;
    } else if (was_running && !motor_running()) {
        fds.ready = 0;
        fds.return_bytes = 0;
    }
    disk_irq_update(cpu_cycle_now);
}

void fds_write_low(uint16_t address, uint8_t data) {
    if (address >= 0x4040) {
        // the sound unit
        if (fds.sound_io && (address <= 0x408a)) {
            if (address < 0x4080) {
                if (sound_write_enable) {
                    sound_wave[address & 63] = data & 63;
                }
            } else if ((address == 0x4080) && (data & 0x80)) {
                sound_vol_gain = data & 63;
            } else if ((address == 0x4084) && (data & 0x80)) {
                sound_mod_gain = data & 63;
            } else if (address == 0x4089) {
                sound_write_enable = data >> 7;
            }
            apu_synth_log_write(address, data, cpu_cycle_now);
        }
        return;
    }
    // ($4020-$4022 and $4024-$4026 written only with the disk registers on; the wiki leaves the reload open, we keep it)
    if ((address > 0x4026) || (!fds.disk_io && (address != 0x4023) && (address > 0x4021))) {
        return;
    }
    catch_up();
    switch (address) {
        case 0x4020:
            fds.timer_reload = (fds.timer_reload & 0xff00) | data;
            break;
        case 0x4021:
            fds.timer_reload = (fds.timer_reload & 0x00ff) | (data << 8);
            break;
        case 0x4022:
            fds.timer_repeat = data & 1;
            fds.timer_on = (data >> 1) & 1;
            if (fds.timer_on) {
                fds.timer_counter = fds.timer_reload;
            } else {
                cpu_irq_clear(FDS_IRQ_TIMER);
            }
            break;
        case 0x4023:
            fds.disk_io = data & 1;
            fds.sound_io = (data >> 1) & 1;
            if (!fds.disk_io) {
                // (the wiki's "Disk I/O reset": the timer stopped and acknowledged, $4025 = $06, $4026 = $7F, the
                // byte transfer flag off)
                fds.timer_on = 0;
                fds.byte_flag = 0;
                cpu_irq_clear(FDS_IRQ_TIMER);
                fds.ext_out = 0x7f;
                write_control(0x06);
            }
            break;
        case 0x4024:
            fds.write_data = data;
            fds.byte_flag = 0;
            cpu_irq_clear(FDS_IRQ_DISK);
            // (moves no event: no batch cut, after every byte while a disk is written)
            return;
        case 0x4025:
            write_control(data);
            break;
        case 0x4026:
            fds.ext_out = data;
            break;
    }
    fds_update_timer();
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

uint8_t fds_read_low(uint16_t address, uint8_t open_bus) {
    if (fds.sound_io && (address >= 0x4040) && (address <= 0x4097)) {
        // (bits 7-6 read as 01; the wave position and envelopes run in apu_synth, so this is what was set)
        if (address < 0x4080) {
            return 0x40 | sound_wave[sound_write_enable ? (address & 63) : 0];
        }
        if (address == 0x4090) {
            return 0x40 | sound_vol_gain;
        }
        if (address == 0x4092) {
            return 0x40 | sound_mod_gain;
        }
        return open_bus;
    }
    // (the read registers are always there, the wiki's "Disk I/O reset")
    if ((address < 0x4030) || (address > 0x4033)) {
        return open_bus;
    }
    catch_up();
    uint8_t value;
    switch (address) {
        case 0x4030:
            // the timer's IRQ, the mirroring, a CRC error (the register not 0), the end of the head, the byte transfer
            // flag; bits 2 and 5 the open bus. The timer's IRQ acknowledged, not the flag (the wiki)
            value = (open_bus & 0x24) | ((mapper_irq_flag & FDS_IRQ_TIMER) ? 0x01 : 0) | (fds.control & CTRL_MIRROR) |
                (fds.crc ? 0x10 : 0) | (fds.end_of_head ? 0x40 : 0) | (fds.byte_flag ? 0x80 : 0);
            cpu_irq_clear(FDS_IRQ_TIMER);
            return value;
        case 0x4031:
            fds.byte_flag = 0;
            cpu_irq_clear(FDS_IRQ_DISK);
            return fds.read_data;
        case 0x4032: {
            // no disk, not ready (the head not at the start and moving: 1 also while the motor is off), write protected
            // (the disks here never are) or no disk
            uint8_t in = fds.side != FDS_NO_DISK;
            value = (open_bus & 0xf8) | (in ? 0 : 0x05) | ((in && fds.ready && motor_running()) ? 0 : 0x02);
            auto_side_poll();
            return value;
        }
        case 0x4033:
            // $4026's outputs (open collector: 0 where it writes 0) and the drive's power good, read with $4026.D7 set
            // while the motor runs
            return (fds.ext_out & 0x7f) | (((fds.ext_out & 0x80) && (fds.side != FDS_NO_DISK) && !(fds.control & CTRL_MOTOR_STOP)) ? 0x80 : 0);
    }
    return open_bus;
}

int fds_current_side() {
    return (fds.side == FDS_NO_DISK) ? ((fds.manual_side == FDS_NO_DISK) ? -1 : fds.manual_side) : fds.side;
}

// the drive is reading or writing (the disk turning): a side change now would leave the game waiting for bytes that
// never come, so the player refuses it
int fds_drive_busy() {
    return motor_running();
}

// once a frame before it runs (the player and the host build): a side changed by hand goes in on its frame, whether
// or not the game is looking at the drive
void fds_frame() {
    if ((fds.manual_side != FDS_NO_DISK) || fds.out_frames) {
        catch_up();
        side_frame();
        fds_update_timer();
        cpu_cut_batch_for_event(mapper_cycles_to_event());
    }
}

// the side after the one in (or last put in), after the last the first; -1 for single sided images
int fds_next_side() {
    int side = fds_current_side();
    if (side_count < 2) {
        return -1;
    }
    return (side < 0) ? 0 : (int)((side + 1) % side_count);
}

// the test harness's replays (test_main.c): a recording's event (input_recording.h, 0 for none) before every frame runs,
// as the player applies it (main.c's recording_event); fds_frame() puts a side changed by hand in on its frame. Here,
// out of the link time optimized units, so the harness's hot code compiles as without it
void fds_recording_event(uint16_t event) {
    if (__builtin_expect(event & INPUT_RECORDING_EVENT_RESET, 0)) {
        frnes_soft_reset_now();
        frnes_reset_event();
    }
    if (cartridge_mapper_number != 20) {
        // (and the Vs. System's coin, on its games)
        if (__builtin_expect(cpu_registers -> VS_Flags, 0)) {
            vs_recording_event(event);
        }
        return;
    }
    if ((event & 0xff00) == INPUT_RECORDING_EVENT_FDS_SIDE) {
        fds_insert_side(event & 0xff);
    }
    fds_frame();
}

// by hand: the disk comes out now and the side goes in HAND_OUT_FRAMES later (the game sees the change)
void fds_insert_side(uint32_t side) {
    if (side >= side_count) {
        return;
    }
    catch_up();
    fds.side = FDS_NO_DISK;
    fds.manual_side = side;
    fds.manual_frame = ppu_frame_count + HAND_OUT_FRAMES;
    // (the automatic change starts over)
    fds.out_frames = 0;
    fds.poll_frames = 0;
    fds.auto_armed = 1;
    printf("FDS: disk out, side %lu goes in at frame %lu\n", (unsigned long)side, (unsigned long)fds.manual_frame);
    fds_update_timer();
}

// the save's buffer (fds_battery_prepare fills it): cheap, the player asks for it every frame
uint8_t* fds_battery_data(uint32_t* size) {
    if (!save_buffer) {
        return NULL;
    }
    *size = save_size;
    return save_buffer;
}

// the save, brought up to date when it's about to be checked or written: every side XORed with the side as loaded.
// The buffer starts as all zeros (the sides as loaded) and only the blocks the drive wrote since are worked out again
void fds_battery_prepare() {
    if (!save_buffer || !any_dirty) {
        return;
    }
    any_dirty = 0;
    uint32_t offset = 0;
    for (uint32_t i = 0; i < side_count; i++) {
        for (uint32_t block = 0; (block << 8) < side_sizes[i]; block++) {
            if (!dirty_blocks[i][block]) {
                continue;
            }
            dirty_blocks[i][block] = 0;
            uint32_t end = (block + 1) << 8;
            if (end > side_sizes[i]) {
                end = side_sizes[i];
            }
            for (uint32_t j = block << 8; j < end; j++) {
                save_buffer[offset + j] = sides[i][j] ^ original_sides[i][j];
            }
        }
        offset += side_sizes[i];
    }
}

// the save's buffer made again from the sides as they are (after a load into it that failed part way)
void fds_battery_reset() {
    memset(dirty_blocks, 1, sizeof(dirty_blocks));
    any_dirty = 1;
    fds_battery_prepare();
}

/*
 * A save was loaded into fds_battery_data's buffer. A side whose blocks don't all read back with good CRCs is from a
 * save made before this drive (see old_layout_crc): taken as the difference from the old sides instead, its blocks'
 * CRCs worked out from their bytes, and the save made again from the sides as they are
 */
void fds_battery_loaded() {
    uint32_t offset = 0;
    int converted = 0;
    for (uint32_t i = 0; i < side_count; i++) {
        for (uint32_t j = 0; j < side_sizes[i]; j++) {
            sides[i][j] = original_sides[i][j] ^ save_buffer[offset + j];
        }
        if (side_walk(sides[i], side_sizes[i], NULL) != 0) {
            old_layout_side = sides[i];
            side_walk(original_sides[i], side_sizes[i], old_layout_crc);
            side_walk(sides[i], side_sizes[i], crc_put);
            printf("FDS: side %lu's save from an earlier FrNES, its CRCs worked out again\n", (unsigned long)i);
            converted = 1;
        }
        offset += side_sizes[i];
    }
    if (converted) {
        fds_battery_reset();
    }
}

/*
 * Save states (mapper_boards.c's part of the board's chunk): a tag (FDR2: a state from the drive before this one has
 * another size, so the board refuses it), the adaptor and drive (fds), the sound registers as the CPU reads them back,
 * the counters' cycle. The 32 kB of RAM is WRAM (the RAM chunk's), the sound the synthesis' chunk, the disks' contents the state's
 * own "FDSK" chunk (fds_disk_state_save, below)
 */
#include "state.h"
static uint8_t state_tag[4] FRNES_COLD_BSS;
#define FDS_STATE(X, A) A(state_tag) X(fds) A(sound_wave) X(sound_vol_gain) X(sound_mod_gain) X(sound_write_enable) X(fds_cycle)

FRNES_COLD_TEXT uint32_t fds_state_size() {
    return STATE_SIZE(FDS_STATE);
}

FRNES_COLD_TEXT void fds_state_save(state_writer_t* writer) {
    memcpy(state_tag, fds_state_tag, 4);
    STATE_SAVE(FDS_STATE)
}

// (after the size is checked) the side in the state has to be one of this image's
FRNES_COLD_TEXT void fds_state_load(state_reader_t* reader) {
    STATE_LOAD(FDS_STATE)
    if ((fds.side != FDS_NO_DISK) && (fds.side >= side_count)) {
        fds.side = FDS_NO_DISK;
    }
    if ((fds.manual_side != FDS_NO_DISK) && (fds.manual_side >= side_count)) {
        fds.manual_side = FDS_NO_DISK;
    }
}

/*
 * The disks in a save state, state.c's optional "FDSK" chunk (version FDS_DISK_STATE_VERSION): the 256 byte blocks of
 * each side that differ from the side as the image was loaded from its file (original_sides, the battery save's base,
 * before any save was applied), so a state brings the disks back to where they were when it was saved. Its layout,
 * little endian: the side count (1 byte), 3 bytes 0, the block count (4 bytes); then each block: its side (1 byte),
 * 1 byte 0, its index in the side (2 bytes) and its 256 bytes (a side's last block: the bytes past the side's end 0).
 * A state without it (older ones) leaves the disks as they are
 */
#define DISK_BLOCK 256

static FRNES_COLD_TEXT int fds_disk_state_present() {
    return (cartridge_mapper_number == 20) && (side_count != 0);
}

FRNES_COLD_TEXT static uint32_t disk_block_length(uint32_t side, uint32_t block) {
    uint32_t start = block * DISK_BLOCK;
    return (side_sizes[side] - start < DISK_BLOCK) ? side_sizes[side] - start : DISK_BLOCK;
}

static FRNES_COLD_TEXT void fds_disk_state_save(state_writer_t* writer) {
    uint32_t blocks = 0;
    for (uint32_t i = 0; i < side_count; i++) {
        for (uint32_t block = 0; block * DISK_BLOCK < side_sizes[i]; block++) {
            uint32_t at = block * DISK_BLOCK;
            blocks += (memcmp(sides[i] + at, original_sides[i] + at, disk_block_length(i, block)) != 0);
        }
    }
    uint8_t head[8] = { side_count, 0, 0, 0, (uint8_t)blocks, (uint8_t)(blocks >> 8), (uint8_t)(blocks >> 16), (uint8_t)(blocks >> 24) };
    state_put(writer, head, sizeof(head));
    for (uint32_t i = 0; i < side_count; i++) {
        for (uint32_t block = 0; block * DISK_BLOCK < side_sizes[i]; block++) {
            uint32_t at = block * DISK_BLOCK, length = disk_block_length(i, block);
            if (memcmp(sides[i] + at, original_sides[i] + at, length) == 0) {
                continue;
            }
            uint8_t entry[4 + DISK_BLOCK] = { (uint8_t)i, 0, (uint8_t)block, (uint8_t)(block >> 8) };
            memcpy(entry + 4, sides[i] + at, length);
            state_put(writer, entry, sizeof(entry));
        }
    }
}

static FRNES_COLD_TEXT int fds_disk_state_load(state_reader_t* reader, uint16_t version) {
    uint8_t head[8];
    if ((version != FDS_DISK_STATE_VERSION) || (state_get(reader, head, sizeof(head)) != 0) || (head[0] != side_count)) {
        return -1;
    }
    uint32_t blocks = head[4] | (head[5] << 8) | (head[6] << 16) | ((uint32_t)head[7] << 24);
    if ((reader -> size - reader -> at) != blocks * (4 + DISK_BLOCK)) {
        return -1;
    }
    // (every block checked before the disks change)
    const uint8_t* entries = reader -> data + reader -> at;
    for (uint32_t n = 0; n < blocks; n++) {
        const uint8_t* entry = entries + n * (4 + DISK_BLOCK);
        if ((entry[0] >= side_count) || ((uint32_t)(entry[2] | (entry[3] << 8)) * DISK_BLOCK >= side_sizes[entry[0]])) {
            return -1;
        }
    }
    for (uint32_t i = 0; i < side_count; i++) {
        memcpy(sides[i], original_sides[i], side_sizes[i]);
    }
    for (uint32_t n = 0; n < blocks; n++) {
        const uint8_t* entry = entries + n * (4 + DISK_BLOCK);
        uint32_t side = entry[0], block = entry[2] | (entry[3] << 8);
        memcpy(sides[side] + block * DISK_BLOCK, entry + 4, disk_block_length(side, block));
    }
    reader -> at = reader -> size;
    // (the battery save written again from the disks as they are now)
    memset(dirty_blocks, 1, sizeof(dirty_blocks));
    any_dirty = 1;
    mapper_save_written = 1;
    return 0;
}

// (its id and message in the cold .rodata: string literals here would go among the others in .rodata, moving them)
static const char disk_chunk_id[4] FRNES_COLD_RODATA = { 'F', 'D', 'S', 'K' };
static const char disk_chunk_refused[] FRNES_COLD_RODATA = "STATE: the FDSK chunk (version %u, %lu bytes) doesn't fit these disks\n";

FRNES_COLD_TEXT void fds_disk_state_chunk(state_writer_t* writer) {
    if (!fds_disk_state_present()) {
        return;
    }
    state_chunk_t header;
    memcpy(header.id, disk_chunk_id, 4);
    header.version = FDS_DISK_STATE_VERSION;
    header.reserved = 0;
    header.size = 0;
    uint32_t at = writer -> size;
    state_put(writer, &header, sizeof(header));
    fds_disk_state_save(writer);
    if (!writer -> failed) {
        ((state_chunk_t*)(writer -> data + at)) -> size = writer -> size - at - sizeof(header);
    }
}

FRNES_COLD_TEXT int fds_disk_state_apply(const uint8_t* data, uint32_t size) {
    if (!fds_disk_state_present()) {
        return 0;
    }
    uint32_t at = sizeof(state_header_t);
    while (at + sizeof(state_chunk_t) <= size) {
        const state_chunk_t* chunk = (const state_chunk_t*)(data + at);
        uint32_t end = at + sizeof(state_chunk_t) + chunk -> size;
        if (end > size) {
            break;
        }
        if (memcmp(chunk -> id, disk_chunk_id, 4) == 0) {
            state_reader_t reader = { data + at + sizeof(state_chunk_t), chunk -> size, 0 };
            if (fds_disk_state_load(&reader, chunk -> version) != 0) {
                printf(disk_chunk_refused, chunk -> version, (unsigned long)chunk -> size);
                return -1;
            }
            return 0;
        }
        at = end;
    }
    return 0;
}

#ifdef FRNES_HOST
// (host/host_main.c's STATE lines: the disks as they are, FNV-1a)
uint32_t fds_disk_hash(void) {
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < side_count; i++) {
        for (uint32_t j = 0; j < side_sizes[i]; j++) {
            hash = (hash ^ sides[i][j]) * 16777619u;
        }
    }
    return hash;
}
#endif
