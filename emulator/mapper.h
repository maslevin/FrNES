#ifndef __MAPPER_H
#define __MAPPER_H

#include <kos.h>

/**
 * Cartridge mappers
 *
 * mapper_init() sets up the initial banking after the cartridge is loaded. Writes to $8000-$FFFF go to
 * mapper_write_fn (after the PPU and APU have been brought up to the CPU's time). Mappers that count scanlines
 * (MMC3) are clocked by the PPU through mapper_ppu_clock() at mapper_clock_dot of each rendering scanline, and
 * raise their IRQ through mapper_irq_flag.
 */

// CPU IRQ line from the cartridge, ORed with the APU's into IRQ_Request
extern uint8_t mapper_irq_flag;

// PPU dot of each rendering scanline at which mapper_ppu_clock() is called, 0 when the mapper doesn't use it
extern uint32_t mapper_clock_dot;

// non zero while the mapper's scanline IRQ is armed, so the CPU batches stop at every clock
extern uint32_t mapper_irq_armed;

// MMC2 / MMC4 (mappers 9 / 10): non zero when PPU pattern fetches of tiles $FD / $FE switch CHR banks, the PPU then
// reports its pattern reads through mapper_latch_read() and saves / restores the latches with its scanline state
extern uint8_t mapper_chr_latch;
// non zero when any of the PPU fetch hooks (latches, extended attributes, split sprite CHR) is in use
extern uint8_t mapper_fetch_hooks;
void mapper_latch_read(uint16_t address);
uint16_t mapper_latch_state();
void mapper_latch_restore(uint16_t state);

// MMC5 extended attribute mode: non zero while each background tile's 4 kB CHR bank and palette come from ExRAM;
// mapper_ext_tile() returns the tile's 16 pattern bytes and its palette, for nametable index (v & 0x3ff) and tile name
extern uint8_t mapper_ext_attributes;
const uint8_t* mapper_ext_tile(uint32_t index, uint32_t name, uint32_t* palette);

// MMC5 with 8x16 sprites: sprite pattern fetches read mapper_sprite_chr(address) (the A set) while the mapped
// pattern tables hold the background's B set; mapper_ppuctrl_written() is told about $2000 writes (before they apply)
extern uint8_t mapper_sprite_chr_split;
const uint8_t* mapper_sprite_chr(uint32_t address);
void mapper_ppuctrl_written(uint8_t old_value, uint8_t new_value);

// mappers with an IRQ counter clocked by the CPU (FME-7, VRC): non zero while it runs; mapper_cpu_run() brings it up
// to CPU cycle 'now' (at the end of each batch), mapper_cycles_to_event() gives the cycles until it can next raise its
// IRQ
extern uint8_t mapper_cpu_timer;
void mapper_cpu_run(uint32_t now);
uint32_t mapper_cycles_to_event();
// mapper clocks (scanlines) until an armed scanline IRQ can be raised
uint32_t mapper_clocks_to_irq();

// $6000-$7FFF writes: 0 PRG RAM, MAPPER_6000_ROM ignored (ROM mapped there), MAPPER_6000_REGISTERS to
// mapper_write_6000() (NINA-001)
#define MAPPER_6000_ROM 1
#define MAPPER_6000_REGISTERS 2
extern uint8_t mapper_6000_mode;
// set when $6000-$7FFF reads go to mapper_read_6000 instead of the window (the CPU cores read below $8000 through
// read_byte)
extern uint8_t mapper_6000_reads;
uint8_t mapper_read_6000(uint16_t address, uint8_t open_bus);
void mapper_write_6000(uint16_t address, uint8_t data);
// cartridges with both CHR ROM and CHR RAM (TQROM): the 1 kB pattern table slots currently mapped to CHR RAM
extern uint8_t mapper_chr_ram_slots;

int mapper_init(uint32_t mapper_number);
// the smallest PRG bank window of the loaded board in bytes (32, 16 or 8 kB), for the frontend's Game Genie
uint32_t mapper_prg_window(void);
// $4020-$5FFF (after the PPU and APU have been brought up to the CPU's time); reads return open_bus when unmapped
uint8_t mapper_read_low(uint16_t address, uint8_t open_bus);
void mapper_write_low(uint16_t address, uint8_t data);
// battery backed RAM kept by the mapper instead of WRAM (MMC6), or NULL; mapper_battery_loaded() after loading it
uint8_t* mapper_battery_data(uint32_t* size);
// brings mapper_battery_data's buffer up to date before it's checked or saved (the FDS makes its save then; asking
// for the buffer is cheap for every mapper)
void mapper_battery_prepare();
// makes mapper_battery_data's buffer again from the mapper's memory (a load into it failed part way)
void mapper_battery_reset();
// set when a board that saves its own memory (board_saves) changed it: the autosave watches it instead of
// prg_ram_written (FDS games write their PRG RAM all the time)
extern volatile uint8_t mapper_save_written;
// an interrupt sequence (NMI, IRQ, BRK) is about to read its vector at 'vector' ($FFFA or $FFFE): every core calls it
// there, for mappers that watch the vector fetch (MMC5)
void mapper_vector_fetch(uint16_t vector);
void mapper_battery_loaded();
// the board's $8000-$FFFF write handler (set by mapper_init; write_byte_6502_c calls it directly)
extern void (*mapper_write_fn)(uint16_t address, uint8_t data);
void mapper_ppu_clock();


// the mappers in mapper_boards.c (built without LTO): mapper.c calls these from its default branches
extern uint8_t board_6000, board_timer, board_low, board_saves;
// board_low_unsynced: the board's $4020-$5FFF registers need no PPU / APU catch-up (the FDS); board_own_irqs: its CPU
// cycle counters raise their IRQs themselves (cpu_irq_raise_at, not CPU_IRQ_MAPPER), so mapper_cpu_run doesn't time them
extern uint8_t board_low_unsynced, board_own_irqs;
uint32_t board_override(uint32_t number);
// 1 when mapper_init loads the board (mapper_boards.c: frnes_rom_support answers from the same list)
int board_supported(uint32_t number);
// the NSF player's board (nsf.c): a number past the 12 bits of a header's mapper. nsf_cartridge makes a tune a NES 2.0
// cartridge saying NSF_IMAGE_MAPPER, which board_override turns into NSF_BOARD only when cartridge_convert saw the
// tune itself (mapper_boards.c keeps an 8 bit number of its own for it). nsf.c's entry points (weak stand-ins in
// mapper_boards.c until it's linked in)
#define NSF_BOARD 0x1000
#define NSF_IMAGE_MAPPER 4095
// the file as load_nes_file read it into a cartridge: an FDS image (fds_cartridge), an NSF tune (nsf_cartridge), or as
// it is; NULL if it can't be (out of the link time optimized code, mapper_boards.c)
uint8_t* cartridge_convert(uint8_t* data, uint32_t* size);

// the Vs. System (mapper_boards.c): $4016 writes (6502_writebyte.c, while VS_Flags is set), the DIP switches (a
// setting: bit n is switch n + 1), the register page set up again after a state's load, the player's frame (coin 1
// while its buttons are held; returns the recording's event), a recording's events, and the game's RGB palette (NULL
// if it isn't a Vs. System game)
#define VS_COIN1_BIT 0x20
#define VS_PAD_SELECT 0x04
#define VS_PAD_START 0x08
void vs_write_4016(uint8_t data);
void vs_start_swap(void);
void board_fresh_wram(void);
// the console's reset button (mapper_boards.c, cold, out of the link time optimized code). frnes_soft_reset asks for
// it (game_menu.c's menu reset); frnes_soft_reset_now, the one path every reset takes, makes it at an input point (the
// asked for one from input_poll_ports, a movie's INPUT_RECORDING_EVENT_RESET from movie_input and fds_recording_event,
// the host's HOST_SOFT_RESET), so a reset made live and its replay land at the same moment: the boards that count
// resets step (mapper 313's outer bank), then the CPU, APU and PPU reset as NesHawk has them (the frame ending there);
// frnes_reset_event gives a recording the event once, for the frame whose input it records next
void frnes_soft_reset(void);
void frnes_soft_reset_now(void);
void frnes_reset_input_point(void);
uint16_t frnes_reset_event(void);
void vs_ppu_write(uint16_t address, uint8_t data);
// the Zapper (mapper_boards.c): the frame's aim (x 0-255, y 0-239; y < 0 or >= 240 off the screen) and trigger
void zapper_aim(int x, int y, int trigger);
void vs_set_dip_switches(uint8_t dips);
// the Zapper's reticle speed, 1-10 (0: 5): the frontend's setting, kept across games like the DIP switches
void zapper_set_sensitivity(uint8_t speed);
// the light gun's calibration (the frontend's, kept across games): screen x = (H - bitmapx) * x_scale / 1000 + x_offset,
// y = (V - bitmapy, doubled when interlaced) * y_scale / 1000 + y_offset; x_scale 0 for the defaults
// (and whether a light gun's reticle is hidden)
void zapper_gun_set_calibration(int16_t x_scale, int16_t x_offset, int16_t y_scale, int16_t y_offset, uint8_t hide_reticle);
// port 2 for NES games (Vs. games ignore it), taken at the game's start: the header's choice, a pad, or the Zapper
#define ZAPPER_PORT_AUTO 0
#define ZAPPER_PORT_CONTROLLER 1
#define ZAPPER_PORT_ZAPPER 2
// (input.h's input_set_port_device for NES port 1)
void zapper_set_port_mode(uint8_t mode);
void vs_state_loaded(uint8_t dips);
uint16_t vs_frame_input(int coin);
void vs_recording_event(uint16_t event);
const uint16_t* vs_palette(void);
uint8_t* nsf_cartridge(uint8_t* data, uint32_t* size);
void nsf_init();
void nsf_write(uint16_t address, uint8_t data);
void nsf_write_low(uint16_t address, uint8_t data);
uint8_t nsf_read_low(uint16_t address, uint8_t open_bus);
int board_init(uint32_t number);
void board_write(uint16_t address, uint8_t data);
void board_write_6000(uint16_t address, uint8_t data);
// $4020-$5FFF while board_low is set (mapper_write_low, mapper_read_low)
void board_write_low(uint16_t address, uint8_t data);
// the board's own save (board_saves: mapper_battery_data / mapper_battery_loaded)
uint8_t* board_battery_data(uint32_t* size);
void board_battery_prepare();
void board_battery_reset();
void board_battery_loaded();
uint8_t board_read_low(uint16_t address, uint8_t open_bus);
// the Vs. System's protection reads at $4020-$5FFF (mapper 206's boards)
int vs_protection_reads(void);
uint8_t board_read_6000(uint16_t address, uint8_t open_bus);
void board_cpu_run(uint32_t cycles);
uint32_t board_cycles_to_fire();
uint32_t board_prg_ram_size(const uint8_t* data, uint32_t size, uint32_t mapper);
uint32_t board_chr_rom_size(const uint8_t* data, uint32_t size, uint32_t chr_size);
// a header that isn't NES 2.0: the mapper (bits 0-11) and submapper (12-15) Mesen 2's game database gives the ROM whose
// PRG and CHR ROM have that CRC32, -1 when it isn't there (mapper_table.h); its mirroring and battery go into NesHeader
// and cartridge_single_screen / cartridge_db_no_battery (here, not in cartridge.c: load time code in the link time
// optimized units moves its partitions, and the hot code's register allocation with them)
int32_t board_db_mapper(uint32_t prg_chr_crc);
// the MMC3 boards with an outer bank register at $6000 (47 NES-QJ, 37 PAL-ZZ): run as mapper 4, the outer bank by
// the page tables (mapper_boards.c); mmc3_reapply (mapper.c) maps the MMC3's banks again after a change
void board_mmc3_outer_init();
void mmc3_reapply();
void mmc3_board_init();
// the nametable mirroring at power on: the header's (flags 6 bit 0: 0 horizontal, 1 vertical), or the game database's
// one screen (cartridge_single_screen)
uint32_t board_mirroring();
// the TC0690's scanline IRQ (mapper.c), for mapper_boards.c
void tc0690_irq_write(uint32_t reg, uint8_t data);
void tc0690_irq_init(uint8_t jetsons);
// the Namco 163's channel phases in its RAM as the chip updates them (one channel every 15 CPU cycles), for reads of
// the RAM: run up to 'now', the $E000 bit 6 halt (mapper_boards.c)
void board_n163_reset();
void board_n163_sync(uint8_t* ram, uint32_t now);
void board_n163_halt(uint8_t halt);
uint8_t board_n163_halted(void);
// starts mapper_cpu_run's count at that CPU cycle (for mapper_boards.c)
void mapper_timer_start(uint32_t cycle);
// save states (state.h): a board's banks, the page each window slot shows (mapper_boards.c). board_window_read takes
// and checks them (0, or -1 with nothing changed), board_window_apply maps them
struct state_writer;
struct state_reader;
typedef struct {
    uint8_t* page[17];
} board_window_t;
// (VRAM and WRAM's size, from mapper.c: only the LTO code refers to them in the release)
typedef struct {
    uint8_t* vram;
    uint32_t wram_size;
    uint32_t flags;
    uint32_t mapper;
} board_memory_t;
// mapper.c's part (the boards' variables there, by value): mode MAPPER_STATE_SAVE / LOAD copies them to / from
// 'buffer', INFO fills 'memory' (VRAM, WRAM's size, the mapper number), AFTER maps again the pages the boards derive
// from their registers; returns the variables' size
#define MAPPER_STATE_SIZE 0
#define MAPPER_STATE_SAVE 1
#define MAPPER_STATE_LOAD 2
#define MAPPER_STATE_INFO 3
#define MAPPER_STATE_AFTER 4
uint32_t mapper_state(uint32_t mode, uint8_t* buffer, board_memory_t* memory);
// the board maps its nametables itself after a load (from CHR ROM: Namco 163, Sunsoft-4)
#define BOARD_OWN_NAMETABLES 1
void board_window_save(struct state_writer* writer, const board_memory_t* memory);
int board_window_read(struct state_reader* reader, board_window_t* window, const board_memory_t* memory);
void board_window_apply(const board_window_t* window, const board_memory_t* memory);
#define BOARD_WINDOW_SIZE (17 * 4)
// the state mapper_boards.c's boards keep (board_init's, and the Namco 163's sound RAM position): its size for the
// loaded board, -1 if it can't be saved; save and load (after the size is checked)
int board_extra_size(uint32_t number);
void board_extra_save(struct state_writer* writer, uint32_t number);
void board_extra_load(struct state_reader* reader, uint32_t number);

#endif
