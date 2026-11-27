#ifndef __SD_CARD_H
#define __SD_CARD_H

/**
 * The SD card adapter (serial port) as /sd: the first partition, FAT. Mounting again after a failure tries again (a
 * card put in later); 1 when /sd is there
 */
int sd_card_mount();
void sd_card_unmount();
// what was written to the card put on it (after a game, which may have written movies or save states)
void sd_card_sync();

#endif
