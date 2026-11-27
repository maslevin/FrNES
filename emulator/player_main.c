#include <kos.h>
#include <malloc.h>

#include "frnes.h"

/*
 * main() for player builds (the frontend has its own, frontend/frontend.c), built with LTO as integration's main()
 * was: the release, the build that ships, keeps integration's hot code
 */
#ifndef FRNES_ROM_PATH
#define FRNES_ROM_PATH "/rd/Super Mario Bros (Rev 0) (JU).nes"
#endif

#ifdef FRNES_SOAK
/*
 * Soak builds (make release SOAK=rounds MAX_FRAMES=n NO_SAVE=1): frnes_run on one ROM after another, the way the
 * frontend will, printing the heap and video RAM in use after each so that leaks and stale state show
 */
static const char* const soak_roms[] = {
    FRNES_ROM_PATH, "/pc/roms/m1a.nes", "/pc/roms/mm3.nes", "/pc/roms/m5a.nes", "/pc/roms/punchout.nes",
    "/pc/roms/m24.nes", "/pc/roms/m85.nes", "/pc/roms/m19a.nes", "/pc/roms/m69a.nes", "/pc/roms/m7.nes",
    "/pc/roms/missing.nes"
};

int main() {
    for (uint32_t round = 0; round < FRNES_SOAK; round++) {
        for (uint32_t i = 0; i < sizeof(soak_roms) / sizeof(soak_roms[0]); i++) {
            int result = frnes_run(soak_roms[i]);
            struct mallinfo heap = mallinfo();
            // (video RAM: the PVR is stopped between games, so it is started to ask)
            pvr_init_defaults();
            size_t vram = pvr_mem_available();
            pvr_shutdown();
            printf("SOAK round %lu game %lu [%s] result %d heap in use %d free %d, VRAM free %lu\n", (unsigned long)round,
                (unsigned long)i, soak_roms[i], result, heap.uordblks, heap.fordblks, (unsigned long)vram);
        }
    }
    printf("SOAK done\n");
    return 0;
}
#endif

// (player builds take no palette: frnes_settings.palette_set stays 0)
const uint16_t* frnes_palette(void) {
    return NULL;
}

const char (*frnes_cheats(void))[9] {
    return NULL;
}

// player builds: the ROM picked at build time, then back to dc-load-ip
#ifndef FRNES_SOAK
int main() {
    frnes_run(FRNES_ROM_PATH);
    return 0;
}
#endif