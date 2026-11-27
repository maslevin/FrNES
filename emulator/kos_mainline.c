/*
 * FrNES on mainline KallistiOS: the two things the MMU_modifications branch of KOS changed that FrNES relies on, done
 * here instead: frnes_kos_map_static, which memory.c calls, and KOS's mmu_set_sq_addr through the link's --wrap
 * (Makefile). Built without LTO (Makefile, as game_menu.c): cold code, called at a game's start and by KOS's sq_lock, so
 * it can't change the hot paths' inlining or LTO's partitions.
 */
#include <kos.h>
#include <arch/mmu.h>
#include <stdio.h>
#include "memory.h"

#define SH4_PTEH  (*(volatile uint32_t*)0xff000000)
#define SH4_PTEL  (*(volatile uint32_t*)0xff000004)
#define SH4_MMUCR (*(volatile uint32_t*)0xff000010)
#define MMUCR_URB_SHIFT 18
#define MMUCR_URC_MASK (0x3f << 10)
#define UTLB_ENTRIES 64

/*
 * mmu_page_map_static, returning the UTLB entry it loaded (memory.c's frnes_map_static keeps a shadow of each entry for
 * utlb_remap). Mainline KOS returns 0 (or -1 for a misaligned page): it loads entry 0x3f - (static mappings so far) and
 * leaves MMUCR.URB one below it, so the entry is read back from URB. A KOS whose mmu_page_map_static returns the entry
 * (the MMU_modifications branch) must agree. A plain call, not a --wrap: with KOS built as LTO objects, LTO inlines
 * mmu_page_map_static into its callers before the link's --wrap could redirect them, and the inlined copy returned
 * mainline's 0 (the nametable remaps then reloaded entry 0, and the console hung). This file isn't LTO, so its call
 * stays a call.
 */
FRNES_COLD_TEXT unsigned int frnes_kos_map_static(uintptr_t virt, uintptr_t phys, page_size_t page_size, page_prot_t page_prot,
    bool cached) {
    unsigned int result = (unsigned int)mmu_page_map_static(virt, phys, page_size, page_prot, cached);
    unsigned int entry = ((SH4_MMUCR >> MMUCR_URB_SHIFT) & 0x3f) + 1;
    if ((result == 0xffffffff) || (entry >= UTLB_ENTRIES) || ((result != 0) && (result != entry))) {
        printf("frnes_kos_map_static: mapping 0x%08lx -> 0x%08lx failed (KOS returned 0x%x, MMUCR.URB + 1 is 0x%x)\n",
            (unsigned long)virt, (unsigned long)phys, result, entry);
        arch_abort();
    }
    return entry;
}

// UTLB entry 'entry' loaded the architected way, as mmu_page_map_static does: MMUCR.URC selects it, LDTLB writes PTEH /
// PTEL (the memory mapped UTLB arrays are never touched: memory.c's utlb_remap)
static FRNES_COLD_TEXT void utlb_load(unsigned int entry, uint32_t pteh, uint32_t ptel) {
    int irq_state = irq_disable();
    uint32_t mmucr = SH4_MMUCR;
    SH4_MMUCR = (mmucr & ~MMUCR_URC_MASK) | (entry << 10);
    SH4_PTEH = pteh;
    SH4_PTEL = ptel;
    __asm__ volatile("ldtlb\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop" ::: "memory");
    SH4_MMUCR = mmucr;
    irq_restore(irq_state);
}

/*
 * KOS's mmu_set_sq_addr, through the link's --wrap=mmu_set_sq_addr (Makefile): sq_lock calls it with the MMU on
 * (pvr_list_begin for the TA every frame, pvr_txr_load, the SPU loads) to point the store queues' UTLB entries 0x3e /
 * 0x3f (1 MB each, mmu_init_basic's) at 'addr'. Mainline KOS writes the UTLB data array directly from cached code, the
 * pattern that left stray TLB values in RAM; these are loaded with LDTLB, with the PTEL memory.c's frnes_sq_map gives
 * them (valid, 1 MB, read / write, cached, dirty).
 */
FRNES_COLD_TEXT void __wrap_mmu_set_sq_addr(void* addr) {
    uint32_t ppn = (uint32_t)addr & 0x1ff00000;
    utlb_load(0x3e, 0xe0000000, ppn | 0x1fc);
    utlb_load(0x3f, 0xe0100000, (ppn + 0x00100000) | 0x1fc);
}
