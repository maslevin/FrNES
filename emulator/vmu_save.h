#ifndef VMU_SAVE_H
#define VMU_SAVE_H

#include <stdint.h>

// battery RAM saves on the VMU, in old FrNES's format (see vmu_save.c)

// prints each attached VMU (port / unit, free blocks) and its files
void vmu_save_list();
// whether any VMU is attached
int vmu_save_available();
// loads the save for the ROM with this CRC into data (up to size bytes); its length, or -1 when there is none
int vmu_save_load(uint32_t crc, uint8_t* data, uint32_t size);
// copies every file on every VMU to directory/<port><unit>_<name>.vms; the number copied
int vmu_save_backup_all(const char* directory);
// writes the save to the first VMU that takes it; 0 on success
int vmu_save_write(uint32_t crc, const uint8_t* data, uint32_t size, const char* rom_path);
// save states (state.h's, already taken) on the first VMU that takes them, compressed with bzip2; 0 on success
int vmu_state_write(uint32_t crc, int slot, const uint8_t* data, uint32_t size, const char* rom_path);
// a save state from the first VMU that has it: *data (malloc'd, the caller frees it) and *size; 0, or -1 if none
int vmu_state_load(uint32_t crc, int slot, uint8_t** data, uint32_t* size);
// the VMUs that take battery saves and save states first (the frontend's choice: 0 the first with room, else 1 +
// port * 2 + unit - 1), kept across games
void vmu_save_choose(uint8_t battery, uint8_t states);
// called between slices of the compression (set by the player for its autosave thread; NULL: none)
extern void (*vmu_save_yield)(void);

#endif
