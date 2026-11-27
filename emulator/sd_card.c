#include <kos.h>
#include <dc/sd.h>
#include <fat/fs_fat.h>

#include "sd_card.h"

/*
 * The SD card adapter as /sd, for the menus' browser and for the emulator when the game it's handed is on the card,
 * read-write: movies, save states and the movies' notes go on it (sd_card_sync after them).
 * Built without LTO (it's not emulator code).
 */
static kos_blockdev_t sd_device;
static int sd_mounted = 0;

int sd_card_mount() {
    if (sd_mounted) {
        return 1;
    }
    uint8 partition_type;
    if (sd_init()) {
        printf("SD: no card (or no adapter)\n");
        return 0;
    }
    if (sd_blockdev_for_partition(0, &sd_device, &partition_type)) {
        printf("SD: no first partition\n");
        sd_shutdown();
        return 0;
    }
    if (fs_fat_init() || fs_fat_mount("/sd", &sd_device, FS_FAT_MOUNT_READWRITE)) {
        printf("SD: unable to mount the first partition as FAT (type %02x)\n", partition_type);
        sd_shutdown();
        return 0;
    }
    printf("SD: mounted at /sd (partition type %02x)\n", partition_type);
    sd_mounted = 1;
    return 1;
}

// what was written to the card (libkosfat keeps its blocks and the FAT in a cache) put on it
void sd_card_sync() {
    if (sd_mounted) {
        fs_fat_sync("/sd");
    }
}

// (at the menus' exit, frontend.c, or before handing over to another binary: the card is left unmounted, its next
// user starts it again)
void sd_card_unmount() {
    if (!sd_mounted) {
        return;
    }
    fs_fat_sync("/sd");
    fs_fat_unmount("/sd");
    fs_fat_shutdown();
    sd_shutdown();
    sd_mounted = 0;
}
