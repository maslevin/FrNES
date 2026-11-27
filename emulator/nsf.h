#ifndef __NSF_H
#define __NSF_H

#include <stdint.h>

/*
 * NSF music files (nsf.c): load_nes_file passes every file through nsf_cartridge, which turns an .nsf into a
 * cartridge of the NSF board (NSF_MAPPER, NES 2.0's last mapper number) and returns other files as they are (NULL on
 * error, the buffer freed). The board runs a small 6502 driver at $5000 that calls the tune's INIT, then its PLAY at
 * the tune's rate; left / right on the first pad change the song.
 */
#define NSF_MAPPER 4095

uint8_t* nsf_cartridge(uint8_t* data, uint32_t* size);
void nsf_init();
void nsf_write(uint16_t address, uint8_t data);
void nsf_write_low(uint16_t address, uint8_t data);
uint8_t nsf_read_low(uint16_t address, uint8_t open_bus);

#endif
