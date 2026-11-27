#ifndef FDS_H
#define FDS_H

#include <stdint.h>

// Famicom Disk System (fds.c): load_nes_file passes every file through fds_cartridge, which turns an .fds image
// into a mapper 20 cartridge (the BIOS, then the disk image) and returns other files as they are (NULL on error,
// the buffer freed)
uint8_t* fds_cartridge(uint8_t* data, uint32_t* size);
void fds_init();
void fds_write(uint16_t address, uint8_t data);
void fds_write_low(uint16_t address, uint8_t data);
uint8_t fds_read_low(uint16_t address, uint8_t open_bus);
void fds_cpu_run(uint32_t cycles);
uint32_t fds_cycles_to_event();
// 0: no automatic side changes (set before mapper_init)
extern uint8_t fds_auto_insert;

// the side in the drive (or going in), -1 for none
int fds_current_side();
// a side change by hand (the player; input recording events)
void fds_insert_side(uint32_t side);
// the side after the current one, wrapping (-1 for a single side)
int fds_next_side();
// the drive is mid-transfer (no side changes by hand)
int fds_drive_busy();
// once a frame, before it runs: a side changed by hand goes in on time
void fds_frame();
// the test harness: a recording event before a frame, as the player applies it
void fds_recording_event(uint16_t event);
// the written sides as a save (see fds.c), and after one was loaded into it
uint8_t* fds_battery_data(uint32_t* size);
void fds_battery_prepare();
void fds_battery_reset();
void fds_battery_loaded();

// save states (fds.c): the drive's and the sound registers' state, mapper_boards.c's part of the board's chunk
struct state_writer;
struct state_reader;
uint32_t fds_state_size();
void fds_state_save(struct state_writer* writer);
void fds_state_load(struct state_reader* reader);
// the disks as the game wrote them, the state's optional "FDSK" chunk (state.c): 1 when there are disks to keep (an FDS
// image loaded), its version, its writer and its reader (0, or -1 refusing it with the disks as they were)
// (state.c's build and apply: the chunk written when there are disks; a state's chunk, if it has one, loaded)
#define FDS_DISK_STATE_VERSION 2
void fds_disk_state_chunk(struct state_writer* writer);
int fds_disk_state_apply(const uint8_t* data, uint32_t size);

#endif
