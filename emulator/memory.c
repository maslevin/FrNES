#include "memory.h"
#include "cartridge.h"
#include "mapper.h"

/**
 * These are arrays of MMU Page Handles which are used to manage remapping of
 * RAM / ROM to various emulated buffers in the system
 */
unsigned int RAM_MMU_HANDLES[8];
unsigned int WRAM_MMU_HANDLES[2];
unsigned int ROMBANK_MMU_HANDLES[8];
unsigned int VROM_MMU_HANDLES[8];

uint8_t* RAM;
uint8_t* WRAM;
uint32_t WRAM_size;

unsigned int REGISTER_BUFFER_MMU_HANDLE;
uint8_t* register_buffer;
_6502_registers_t* cpu_registers;
_PPU_registers_t* ppu_registers;

unsigned int PPU_NAMETABLES_MMU_HANDLES[8];
unsigned int PPU_MEMORY_BUFFER_MMU_HANDLE;
uint8_t* VRAM;
uint8_t* ppu_memory_buffer;
uint8_t* SPRRAM;
uint8_t* PALETTE_RAM;

/**
* Static UTLB entry remapping
 *
 * Changes the physical page of a static UTLB entry (a handle from mmu_page_map_static) in place, keeping its other
 * attributes, optionally setting write-through. The entry is reloaded the architected way, the same as KOS's
 * mmu_page_map_static: MMUCR.URC selects the entry, PTEH / PTEL hold the new contents and LDTLB writes them.
 *
 * The memory mapped UTLB arrays are never touched: writing them directly (an older version, and KOS's sq_lock)
 * and also reading them from cached code (the version before this one read PTEH / PTEL back from the arrays) was
 * followed by stray 8 byte writes of TLB values (e.g. 0x0200b300, the address array entry of a ROM bank page)
 * into whatever line was in one operand cache set. The entries' contents are kept in a shadow table instead,
 * filled in when they are created (frnes_map_static) with the same values KOS loads.
 */
#define UTLB_ENTRIES 64
#define UTLB_DATA_PPN_MASK 0x1ffffc00
#define UTLB_DATA_WRITE_THROUGH 0x00000001
#define SH4_PTEH  (*(volatile uint32_t*)0xff000000)
#define SH4_PTEL  (*(volatile uint32_t*)0xff000004)
#define SH4_MMUCR (*(volatile uint32_t*)0xff000010)
#define MMUCR_URC_MASK (0x3f << 10)

static uint32_t utlb_shadow_pteh[UTLB_ENTRIES];
static uint32_t utlb_shadow_ptel[UTLB_ENTRIES];

// PTEL as KOS's mmu_page_map_static builds it: valid, the page size, protection and cacheability, dirty set
static uint32_t utlb_build_ptel(uintptr_t phys, page_size_t page_size, page_prot_t page_prot, bool cached) {
    return (phys & UTLB_DATA_PPN_MASK) | (1 << 8) | ((page_size & 2) << 6) | ((page_size & 1) << 4) | (page_prot << 5) |
        ((cached ? 1 : 0) << 3) | (1 << 2);
}

// mmu_page_map_static (through frnes_kos_map_static, kos_mainline.c: the entry it loaded), recording the entry's
// contents for utlb_remap
static unsigned int frnes_map_static(uintptr_t virt, uintptr_t phys, page_size_t page_size, page_prot_t page_prot, bool cached) {
    unsigned int entry = frnes_kos_map_static(virt, phys, page_size, page_prot, cached);
    if (entry < UTLB_ENTRIES) {
        utlb_shadow_pteh[entry] = virt & 0xfffffc00;
        utlb_shadow_ptel[entry] = utlb_build_ptel(phys, page_size, page_prot, cached);
    }
    return entry;
}

static void utlb_shadow_init() {
    // mmu_init_basic's store queue entries: 0x3f maps 0xe0100000, 0x3e maps 0xe0000000 (1 MB, as sq_lock loads them)
    utlb_shadow_pteh[0x3e] = 0xe0000000;
    utlb_shadow_pteh[0x3f] = 0xe0100000;
    utlb_shadow_ptel[0x3e] = utlb_shadow_ptel[0x3f] = 0x1fc;
}

static void utlb_remap(unsigned int entry, const void* physical, uint32_t write_through) {
    int irq_state = irq_disable();
    uint32_t ptel = (utlb_shadow_ptel[entry] & ~(UTLB_DATA_PPN_MASK | UTLB_DATA_WRITE_THROUGH)) |
        ((uintptr_t)physical & UTLB_DATA_PPN_MASK) | (write_through ? UTLB_DATA_WRITE_THROUGH : 0);
    utlb_shadow_ptel[entry] = ptel;
    uint32_t mmucr = SH4_MMUCR;
    SH4_MMUCR = (mmucr & ~MMUCR_URC_MASK) | (entry << 10);
    SH4_PTEH = utlb_shadow_pteh[entry];
    SH4_PTEL = ptel;
    __asm__ volatile("ldtlb\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop" ::: "memory");
    SH4_MMUCR = mmucr;
    irq_restore(irq_state);
}

/**
 * Points the store queues at 'dest' (a physical / P1 / P2 address) and returns the store queue address to write
 * to, like KOS's sq_lock without its mutex. KOS's sq_lock rewrites UTLB entries 0x3e / 0x3f (the two 1 MB store
 * queue mappings) by writing the UTLB data array directly from cached code, the same pattern that caused stray
 * writes in utlb_remap, so the frame output maps them here with LDTLB instead. Nothing else may use the store
 * queues meanwhile (the sound stream uses SPU DMA).
 */
uint32_t* frnes_sq_map(void* dest) {
    uint32_t ppn = (uint32_t)dest & 0x1ff00000;
    utlb_remap(0x3e, (const void*)ppn, 0);
    utlb_remap(0x3f, (const void*)(ppn + 0x00100000), 0);
    return (uint32_t*)(MEM_AREA_SQ_BASE | ((uintptr_t)dest & 0x000fffe0));
}

/**
 * Nametables: 4 slots ($2000-$2FFF) plus their $3000-$3FFF mirrors, each a 1 kB MMU page pointing at one of the
 * two 1 kB halves of VRAM according to the mirroring. The pages are write-through, and a nametable write
 * invalidates the cache line in every other virtual alias of the same VRAM page, so reads through any alias
 * (the renderer reads base + (v & 0xFFF) directly) always see the current data.
 */
static uint8_t nametable_slot_page[4];
// changes whenever nametable contents or mirroring change (the renderer caches per tile row data from them)
uint32_t ppu_nametable_generation = 0;

// nametable_slot_page is what the 8 slots show and their cache lines are current: a mapping of the same 4 pages again
// changes nothing (cleared when the slots are mapped from scratch, initialize_ppu_memory)
static uint8_t nametable_pages_valid = 0;

static void map_nametables(uint8_t page0, uint8_t page1, uint8_t page2, uint8_t page3) {
    // (the same pages: no remap, no stale line to drop and no new contents for the renderer's row cache. Tengen's
    // 800037 (Alien Syndrome) maps its nametables after every register write, ~36 a frame)
    if (nametable_pages_valid && (page0 == nametable_slot_page[0]) && (page1 == nametable_slot_page[1]) &&
        (page2 == nametable_slot_page[2]) && (page3 == nametable_slot_page[3])) {
        return;
    }
    nametable_pages_valid = 1;
    ppu_nametable_generation++;
    nametable_slot_page[0] = page0;
    nametable_slot_page[1] = page1;
    nametable_slot_page[2] = page2;
    nametable_slot_page[3] = page3;
    for (uint32_t slot = 0; slot < 8; slot++) {
        utlb_remap(PPU_NAMETABLES_MMU_HANDLES[slot], &VRAM[nametable_slot_page[slot & 3] << 10], 1);
    }
    // the pages are write-through so nothing is dirty; drop any line that might hold stale data for the new
    // physical page of a slot (it was not invalidated by writes made while the slot pointed elsewhere)
    for (uint32_t offset = 0; offset < 0x2000; offset += 32) {
        __asm__ volatile("ocbi @%0" :: "r"(NAMETABLE0_ADDR + offset) : "memory");
    }
}

// the VRAM page a nametable slot (0-3) maps
FRNES_COLD_TEXT uint8_t ppu_nametable_slot_page(uint32_t slot) {
    return nametable_slot_page[slot & 3];
}

void set_ppu_nametable_pages(uint8_t page0, uint8_t page1, uint8_t page2, uint8_t page3) {
    map_nametables(page0, page1, page2, page3);
}

/**
 * Writes a byte of a VRAM page directly (not through a nametable slot), e.g. MMC5's ExRAM from the CPU side: the
 * line goes to memory and any slot showing the page drops its cached copy
 */
void ppu_vram_page_write(uint32_t page, uint32_t offset, uint8_t data) {
    uint8_t* p = &VRAM[(page << 10) | (offset & 0x3ff)];
    *p = data;
    __asm__ volatile("ocbwb @%0" :: "r"(p) : "memory");
    for (uint32_t slot = 0; slot < 8; slot++) {
        if (nametable_slot_page[slot & 3] == page) {
            __asm__ volatile("ocbi @%0" :: "r"(NAMETABLE0_ADDR + (slot << 10) + (offset & 0x3ff)) : "memory");
        }
    }
    ppu_nametable_generation++;
}

uint8_t ppu_vram_page_read(uint32_t page, uint32_t offset) {
    return VRAM[(page << 10) | (offset & 0x3ff)];
}

// fills a whole VRAM page (MMC5 fill mode): 960 tile bytes and 64 attribute bytes
void ppu_vram_page_fill(uint32_t page, uint8_t tile, uint8_t attribute) {
    uint8_t* p = &VRAM[page << 10];
    memset(p, tile, 0x3c0);
    memset(p + 0x3c0, attribute, 0x40);
    dcache_wback_range((uintptr_t)p, 0x400);
    for (uint32_t slot = 0; slot < 8; slot++) {
        if (nametable_slot_page[slot & 3] == page) {
            for (uint32_t offset = 0; offset < 0x400; offset += 32) {
                __asm__ volatile("ocbi @%0" :: "r"(NAMETABLE0_ADDR + (slot << 10) + offset) : "memory");
            }
        }
    }
    ppu_nametable_generation++;
}

/**
 * Nametable slots from CHR ROM (Sunsoft-4): slots whose bit is set in rom_slots show the 1 kB page at pages[slot]
 * instead of VRAM, read only; the other slots keep VRAM pages 0 / 1 as given in vram_pages
 */
void set_ppu_nametable_rom(uint8_t* const* pages, uint32_t rom_slots, const uint8_t* vram_pages) {
    ppu_nametable_generation++;
    for (uint32_t slot = 0; slot < 4; slot++) {
        nametable_slot_page[slot] = (rom_slots & (1 << slot)) ? 0xff : vram_pages[slot];
    }
    for (uint32_t slot = 0; slot < 8; slot++) {
        uint32_t s = slot & 3;
        utlb_remap(PPU_NAMETABLES_MMU_HANDLES[slot], (rom_slots & (1 << s)) ? pages[s] : &VRAM[vram_pages[s] << 10], 1);
    }
    for (uint32_t offset = 0; offset < 0x2000; offset += 32) {
        __asm__ volatile("ocbi @%0" :: "r"(NAMETABLE0_ADDR + offset) : "memory");
    }
}

void ppu_nametable_write(uint16_t address, uint8_t data) {
    uint32_t offset = address & 0x03ff;
    uint32_t page = nametable_slot_page[(address >> 10) & 3];
    if (page == 0xff) {
        // CHR ROM nametable
        return;
    }
    ((uint8_t*)NAMETABLE0_ADDR)[address & 0x0fff] = data;
    ppu_nametable_generation++;
    for (uint32_t slot = 0; slot < 8; slot++) {
        uintptr_t alias = NAMETABLE0_ADDR + (slot << 10) + offset;
        if ((nametable_slot_page[slot & 3] == page) && (alias != (NAMETABLE0_ADDR + (address & 0x0fff)))) {
            __asm__ volatile("ocbi @%0" :: "r"(alias) : "memory");
        }
    }
}

int initialize_registers() {
    printf("registers - allocating\n");
    // allocate register buffer on a 1kB-byte boundary so that we can page in register structs into their own cache lines
    // and also use the MMU to control it in the virtual memory space
    // just 64-bytes for now, we can expand it later for adding APU registers in a struct as well
    register_buffer = aligned_alloc(0x400, 0x400);
    memset(register_buffer, 0, 0x400);
    dcache_wback_range((uintptr_t)register_buffer, 0x400);
    // put the cpu registers in the first 32 bytes of the register buffer
    cpu_registers = (_6502_registers_t*)register_buffer;
    // put the ppu registers in the second 32 bytes of the register buffer
    ppu_registers = (_PPU_registers_t*)&register_buffer[32];
    printf(" ...mapping MMU page\n");
    // map the register buffer into a 1kB buffer that we can MMU map into a place that will make ocindex cache usage easier
    REGISTER_BUFFER_MMU_HANDLE = frnes_map_static(REGISTERS_ADDR, (uintptr_t)register_buffer, PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    printf(" ...done\n");
    return 0;
}

int initialize_ppu_memory() {
    printf("PPU memory - allocating\n");
    // 2 kB of PPU VRAM, plus two more 1 kB pages a mapper can put in the nametable slots (MMC5's ExRAM and fill mode)
    // and two banks of four-screen nametable RAM (pages 4-11, GTROM)
    printf(" ...allocating 12kB PPU VRAM\n");
    VRAM = aligned_alloc(0x400, 0x3000);
    memset(VRAM, 0, 0x3000);
    dcache_wback_range((uintptr_t)VRAM, 0x3000);
    // allocate PPU memory buffer on a 1kB-byte boundary so that we can put SPRRAM and PALETTE RAM in their own cache lines
    // and also use the MMU to control it in the virtual memory space
    ppu_memory_buffer = aligned_alloc(0x400, 0x400);
    memset(ppu_memory_buffer, 0, 0x400);
    dcache_wback_range((uintptr_t)ppu_memory_buffer, 0x400);
    // put SPRRAM in the first 256 bytes of the PPU memory buffer
    printf(" ...allocating 256b SPRRAM\n");    
    SPRRAM = ppu_memory_buffer;
    // put PALETTE RAM in the next 32 bytes of the PPU memory buffer
    printf(" ...allocating 32b PALETTE RAM\n");    
    PALETTE_RAM = &ppu_memory_buffer[256];
    printf(" ...mapping MMU pages\n");
    printf(" ...defaulting to horizontal mirroring\n");
    PPU_NAMETABLES_MMU_HANDLES[0] = frnes_map_static(NAMETABLE0_ADDR, (uintptr_t)VRAM, PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    PPU_NAMETABLES_MMU_HANDLES[1] = frnes_map_static(NAMETABLE1_ADDR, (uintptr_t)VRAM, PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    PPU_NAMETABLES_MMU_HANDLES[2] = frnes_map_static(NAMETABLE2_ADDR, (uintptr_t)&VRAM[0x400], PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    PPU_NAMETABLES_MMU_HANDLES[3] = frnes_map_static(NAMETABLE3_ADDR, (uintptr_t)&VRAM[0x400], PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    PPU_NAMETABLES_MMU_HANDLES[4] = frnes_map_static(NAMETABLE0_ADDR_MIRROR, (uintptr_t)VRAM, PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    PPU_NAMETABLES_MMU_HANDLES[5] = frnes_map_static(NAMETABLE1_ADDR_MIRROR, (uintptr_t)VRAM, PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    PPU_NAMETABLES_MMU_HANDLES[6] = frnes_map_static(NAMETABLE2_ADDR_MIRROR, (uintptr_t)&VRAM[0x400], PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    PPU_NAMETABLES_MMU_HANDLES[7] = frnes_map_static(NAMETABLE3_ADDR_MIRROR, (uintptr_t)&VRAM[0x400], PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    printf(" ...making nametable pages write-through\n");
    nametable_pages_valid = 0;
    map_nametables(0, 0, 1, 1);
    printf(" ...mapping SPRRAM and PALETTE RAM\n");
    // map the ppu memory buffer into a 1kB buffer that we can MMU map into a place that will make ocindex cache usage easier
    PPU_MEMORY_BUFFER_MMU_HANDLE = frnes_map_static(SPRRAM_ADDR, (uintptr_t)ppu_memory_buffer, PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    printf(" ...done\n");    
    return 0;    
}

int initialize_memory() {
    printf("memory - initializing\n");
    printf(" ...enabling MMU\n");
    mmu_init_basic();
    utlb_shadow_init();
    printf(" ...allocating RAM buffer\n");
    RAM = aligned_alloc(0x400, 0x800);
    // power on RAM contents are undefined on the NES, start from zeroes so that runs are reproducible
    memset(RAM, 0, 0x800);
    dcache_wback_range((uintptr_t)RAM, 0x800);
    WRAM = NULL;
    WRAM_size = 0;
    printf(" ...mapping MMU pages\n");
    uintptr_t ram_buffer_page_1 = (uintptr_t)RAM;
    uintptr_t ram_buffer_page_2 = (uintptr_t)&RAM[0x400];
    // Mapping 6502 RAM region using memory space 0x02000000 - 0x02001fff 
    // to virtual addresses, creates 8 individual 1kB page mappings to mirror RAM from 0x000-0x800 four times
    uint32_t virtual_address = RAM_ADDR;
    uint32_t entry_index = 0;
    for (uint32_t index = 0; index < 4; index++) {
        RAM_MMU_HANDLES[entry_index] = frnes_map_static(virtual_address, ram_buffer_page_1, PAGE_SIZE_1K, MMU_ALL_RDWR, true);
        if (RAM_MMU_HANDLES[entry_index] == 0xffffffff) {
            goto handle_err;
        }
        entry_index++;
        virtual_address += 0x400;
        RAM_MMU_HANDLES[entry_index] = frnes_map_static(virtual_address, ram_buffer_page_2, PAGE_SIZE_1K, MMU_ALL_RDWR, true);
        if (RAM_MMU_HANDLES[entry_index] == 0xffffffff) {
            goto handle_err;
        }        
        entry_index++;
        virtual_address += 0x400;
    }
    printf(" ...done\n"); 
    return 0;

handle_err:
    printf(" ...error encountered mapping NES memory - exiting\n");
    return -1;
}

int initialize_rom(uint8_t* rombank0data, uint8_t* rombank1data, uint8_t* rombank2data, uint8_t* rombank3data) {
    printf("rom - initializing\n");
    printf(" ...mapping MMU pages\n");
    printf(" ...mapping ROM bank 0\n");
    uint32_t virtual_address = ROMBANK0_ADDR;
    uintptr_t bank_page_1 = (uintptr_t)rombank0data;
    uintptr_t bank_page_2 = (uintptr_t)&rombank0data[0x1000];
    ROMBANK_MMU_HANDLES[0] = frnes_map_static(virtual_address, bank_page_1, PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    if (ROMBANK_MMU_HANDLES[0] == 0xffffffff) {
        goto handle_err;
    }
    virtual_address += 0x1000;
    ROMBANK_MMU_HANDLES[1] = frnes_map_static(virtual_address, bank_page_2, PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    if (ROMBANK_MMU_HANDLES[1] == 0xffffffff) {
        goto handle_err;
    }
    virtual_address += 0x1000;
    printf(" ...mapping ROM bank 1\n");
    bank_page_1 = (uintptr_t)rombank1data;
    bank_page_2 = (uintptr_t)&rombank1data[0x1000];
    ROMBANK_MMU_HANDLES[2] = frnes_map_static(virtual_address, bank_page_1, PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    if (ROMBANK_MMU_HANDLES[2] == 0xffffffff) {
        goto handle_err;
    }
    virtual_address += 0x1000;
    ROMBANK_MMU_HANDLES[3] = frnes_map_static(virtual_address, bank_page_2, PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    if (ROMBANK_MMU_HANDLES[3] == 0xffffffff) {
        goto handle_err;
    }
    virtual_address += 0x1000;
    printf(" ...mapping ROM bank 2\n");
    bank_page_1 = (uintptr_t)rombank2data;
    bank_page_2 = (uintptr_t)&rombank2data[0x1000];
    ROMBANK_MMU_HANDLES[4] = frnes_map_static(virtual_address, bank_page_1, PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    if (ROMBANK_MMU_HANDLES[4] == 0xffffffff) {
        goto handle_err;
    }
    virtual_address += 0x1000;
    ROMBANK_MMU_HANDLES[5] = frnes_map_static(virtual_address, bank_page_2, PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    if (ROMBANK_MMU_HANDLES[5] == 0xffffffff) {
        goto handle_err;
    }
    virtual_address += 0x1000;
    printf(" ...mapping ROM bank 3\n");
    bank_page_1 = (uintptr_t)rombank3data;
    bank_page_2 = (uintptr_t)&rombank3data[0x1000];
    ROMBANK_MMU_HANDLES[6] = frnes_map_static(virtual_address, bank_page_1, PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    if (ROMBANK_MMU_HANDLES[6] == 0xffffffff) {
        goto handle_err;
    }
    virtual_address += 0x1000;
    ROMBANK_MMU_HANDLES[7] = frnes_map_static(virtual_address, bank_page_2, PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    if (ROMBANK_MMU_HANDLES[7] == 0xffffffff) {
        goto handle_err;
    }
    printf(" ...done\n");     
    return 0;

handle_err:
    printf(" ...error encountered mapping ROM memory - exiting\n");
    return -1;
}

int initialize_ppu_pattern_tables(uint8_t* ppubank0data, uint8_t* ppubank1data, uint8_t* ppubank2data, uint8_t* ppubank3data, uint8_t* ppubank4data, uint8_t* ppubank5data, uint8_t* ppubank6data, uint8_t* ppubank7data) {
    printf("ppu pattern tables - initializing\n");
    printf(" ...mapping MMU pages\n");
    printf(" ...mapping bank 0\n");
    uint32_t virtual_address = PATTERN_TABLES_ADDR;
    VROM_MMU_HANDLES[0] = frnes_map_static(virtual_address, (uintptr_t)ppubank0data, PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    if (VROM_MMU_HANDLES[0] == 0xffffffff) {
        goto handle_err;
    }
    printf(" ...mapping bank 1\n");    
    virtual_address += 0x400;
    VROM_MMU_HANDLES[1] = frnes_map_static(virtual_address, (uintptr_t)ppubank1data, PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    if (VROM_MMU_HANDLES[1] == 0xffffffff) {
        goto handle_err;
    }
    printf(" ...mapping bank 2\n");    
    virtual_address += 0x400;
    VROM_MMU_HANDLES[2] = frnes_map_static(virtual_address, (uintptr_t)ppubank2data, PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    if (VROM_MMU_HANDLES[2] == 0xffffffff) {
        goto handle_err;
    }
    printf(" ...mapping bank 3\n");    
    virtual_address += 0x400;
    VROM_MMU_HANDLES[3] = frnes_map_static(virtual_address, (uintptr_t)ppubank3data, PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    if (VROM_MMU_HANDLES[3] == 0xffffffff) {
        goto handle_err;
    }
    printf(" ...mapping bank 4\n");    
    virtual_address += 0x400;
    VROM_MMU_HANDLES[4] = frnes_map_static(virtual_address, (uintptr_t)ppubank4data, PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    if (VROM_MMU_HANDLES[4] == 0xffffffff) {
        goto handle_err;
    }
    printf(" ...mapping bank 5\n");    
    virtual_address += 0x400;
    VROM_MMU_HANDLES[5] = frnes_map_static(virtual_address, (uintptr_t)ppubank5data, PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    if (VROM_MMU_HANDLES[5] == 0xffffffff) {
        goto handle_err;
    }
    printf(" ...mapping bank 6\n");    
    virtual_address += 0x400;
    VROM_MMU_HANDLES[6] = frnes_map_static(virtual_address, (uintptr_t)ppubank6data, PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    if (VROM_MMU_HANDLES[6] == 0xffffffff) {
        goto handle_err;
    }
    printf(" ...mapping bank 7\n");    
    virtual_address += 0x400;
    VROM_MMU_HANDLES[7] = frnes_map_static(virtual_address, (uintptr_t)ppubank7data, PAGE_SIZE_1K, MMU_ALL_RDWR, true);
    if (VROM_MMU_HANDLES[7] == 0xffffffff) {
        goto handle_err;
    }
    // (CHR RAM slots write-through from the start, see chr_slot_map)
    set_ppu_pattern_table_bank_pointers(ppubank0data, ppubank1data, ppubank2data, ppubank3data, ppubank4data, ppubank5data, ppubank6data, ppubank7data);
    printf(" ...done\n");
    return 0;

handle_err:
    printf(" ...error encountered mapping VROM memory - exiting\n");
    return -1;    
}

static int initialize_wram_mapping();

int initialize_wram(uint32_t size, uint8_t* data) { 
    printf("work ram - initializing\n");

    WRAM_size = size;
    if (WRAM_size >= 0x2000) {
        printf(" ...allocating WRAM buffer\n");
        WRAM = aligned_alloc(0x1000, size);
        // zeroed like RAM so that runs (and input recordings) are reproducible
        memset(WRAM, 0, size);
        dcache_wback_range((uintptr_t)WRAM, size);
    } else {
        goto handle_err;
    }
    return initialize_wram_mapping();

handle_err:
    printf(" ...unsupported work ram configuration encountered - exiting\n");
    return -1;
}

static int initialize_wram_mapping() {
    printf(" ...mapping MMU pages\n");
    printf(" ...mapping WRAM\n");
    uint32_t virtual_address = WRAM_ADDR;
    uintptr_t bank_page_1 = (uintptr_t)WRAM;
    uintptr_t bank_page_2 = (uintptr_t)&WRAM[0x1000];
    WRAM_MMU_HANDLES[0] = frnes_map_static(virtual_address, bank_page_1, PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    if (WRAM_MMU_HANDLES[0] == 0xffffffff) {
        goto handle_err;
    }
    virtual_address += 0x1000;
    WRAM_MMU_HANDLES[1] = frnes_map_static(virtual_address, bank_page_2, PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    if (WRAM_MMU_HANDLES[1] == 0xffffffff) {
        goto handle_err;
    }
    printf(" ...done\n");
    return 0;

handle_err:
    printf(" ...unsupported work ram configuration encountered - exiting\n");
    return -1;
}

// grows WRAM (MMC5 boards have up to 64 kB of PRG RAM), keeping its contents; maps its first 8 kB at $6000
int resize_wram(uint32_t size) {
    if (WRAM && (WRAM_size >= size)) {
        return 0;
    }
    uint8_t* bigger = aligned_alloc(0x1000, size);
    if (!bigger) {
        return -1;
    }
    memset(bigger, 0, size);
    if (WRAM) {
        memcpy(bigger, WRAM, WRAM_size);
    }
    dcache_wback_range((uintptr_t)bigger, size);
    if (!WRAM) {
        WRAM = bigger;
        WRAM_size = size;
        return initialize_wram_mapping();
    }
    uint8_t* old = WRAM;
    WRAM = bigger;
    WRAM_size = size;
    set_wram_bank_pointer(WRAM);
    free(old);
    return 0;
}

// maps 8 kB of WRAM at $6000-$7FFF (mappers with banked PRG RAM)
volatile uint8_t prg_ram_written = 0;

// writes back the $6000 window's cache lines (through the window's addresses, which the operand cache is indexed by)
__attribute__((cold, noinline)) void wram_window_writeback() {
    for (uintptr_t line = WRAM_ADDR; line < WRAM_ADDR + 0x2000; line += 32) {
        __asm__ volatile("ocbwb @%0" :: "r"(line) : "memory");
    }
}

void set_wram_bank_pointer(uint8_t* data) {
    // what the game wrote to the bank being unmapped goes to memory, so WRAM is always up to date there apart from
    // the mapped bank's dirty lines (the battery save writes those back before reading WRAM)
    wram_window_writeback();
    utlb_remap(WRAM_MMU_HANDLES[0], data, 0);
    utlb_remap(WRAM_MMU_HANDLES[1], data + 0x1000, 0);
}

void shutdown_memory() {
    printf("memory - shutting down\n");
    printf(" ...shutting down MMU\n");
    mmu_shutdown_basic();
    printf(" ...freeing RAM buffer\n");
    free(RAM);
    RAM = NULL;
    if (WRAM != NULL) {
        printf(" ...freeing WRAM buffer\n");
        free(WRAM);
        WRAM = NULL;
    }
    printf(" ...freeing VRAM buffer\n");
    free(VRAM);
    VRAM = NULL;
    printf(" ...freeing SPRRAM / palette buffer\n");
    free(ppu_memory_buffer);
    ppu_memory_buffer = NULL;
    SPRRAM = NULL;
    PALETTE_RAM = NULL;
    printf(" ...freeing registers buffer\n");
    free(register_buffer);
    register_buffer = NULL;
    cpu_registers = NULL;
    ppu_registers = NULL;
    printf(" ...done\n");
}

// (in the "c" slot, tools/hot_slots.py: one copy, at a fixed address; LTO's clones of it, one per caller, were numbered
// and placed in an order a change elsewhere could swap, which moved the C core's bank switching)
// (a bank that already maps 'data' is left alone: MMC1 maps its fixed $C000 bank again at every PRG register load,
// Zelda II's overworld ~350 a frame every ~18 frames. Only the emulator changes these entries, so the shadow is
// what they hold; unlike the store queue entries 0x3e / 0x3f, which KOS's sq_lock rewrites behind it)
FRNES_HOT(c_bank) void set_6502_bank_pointer(uint32_t bank, uint8_t* data) {
    unsigned int entry = ROMBANK_MMU_HANDLES[bank << 1];
    if ((utlb_shadow_ptel[entry] & UTLB_DATA_PPN_MASK) == ((uintptr_t)data & UTLB_DATA_PPN_MASK)) {
        return;
    }
    utlb_remap(entry, data, 0);
    utlb_remap(ROMBANK_MMU_HANDLES[(bank << 1) + 1], &data[0x1000], 0);
}

void set_6502_rom_bank_pointers(uint8_t* rombank0data, uint8_t* rombank1data, uint8_t* rombank2data, uint8_t* rombank3data) {
    utlb_remap(ROMBANK_MMU_HANDLES[0], rombank0data, 0);
    utlb_remap(ROMBANK_MMU_HANDLES[1], &rombank0data[0x1000], 0);
    utlb_remap(ROMBANK_MMU_HANDLES[2], rombank1data, 0);
    utlb_remap(ROMBANK_MMU_HANDLES[3], &rombank1data[0x1000], 0);
    utlb_remap(ROMBANK_MMU_HANDLES[4], rombank2data, 0);
    utlb_remap(ROMBANK_MMU_HANDLES[5], &rombank2data[0x1000], 0);
    utlb_remap(ROMBANK_MMU_HANDLES[6], rombank3data, 0);
    utlb_remap(ROMBANK_MMU_HANDLES[7], &rombank3data[0x1000], 0);    
}

/**
 * CHR RAM in the pattern table slots: a page can be in two slots at once (CPROM's bank 0 at $0000 and $1000), and the
 * operand cache is indexed by virtual address, so the two slots keep separate lines for it. As the nametables: the
 * slots of CHR RAM are write-through, a CHR RAM write invalidates the line in every other slot showing the page, and
 * a slot pointed at a page drops the lines it had for it (from an earlier mapping). CHR ROM slots are untouched.
 */
static uint8_t* chr_slot_page[8];
// the slots whose CHR RAM page is in another slot too (write_byte invalidates the others only for these)
uint8_t ppu_chr_alias_slots = 0;

static inline int chr_is_ram() {
    return (num_1k_VROM_pages == 0) || mapper_chr_ram_slots;
}

__attribute__((noinline)) static void chr_slot_map(uint32_t bank, uint8_t* data) {
    chr_slot_page[bank] = data;
    if (!chr_is_ram()) {
        utlb_remap(VROM_MMU_HANDLES[bank], data, 0);
        return;
    }
    utlb_remap(VROM_MMU_HANDLES[bank], data, 1);
    for (uint32_t offset = 0; offset < 0x400; offset += 32) {
        __asm__ volatile("ocbi @%0" :: "r"(PATTERN_TABLES_ADDR + (bank << 10) + offset) : "memory");
    }
    uint8_t aliased = 0;
    for (uint32_t a = 0; a < 8; a++) {
        for (uint32_t b = 0; b < 8; b++) {
            if ((a != b) && (chr_slot_page[a] == chr_slot_page[b])) {
                aliased |= 1 << a;
            }
        }
    }
    ppu_chr_alias_slots = aliased;
}

void set_ppu_pattern_table_bank_pointer(uint32_t bank, uint8_t* data) {
    chr_slot_map(bank, data);
}

// the page a pattern table slot maps (mapper.c: a bank write that maps what's already there changes nothing)
FRNES_COLD_TEXT const uint8_t* ppu_pattern_table_bank_pointer(uint32_t bank) {
    return chr_slot_page[bank];
}

__attribute__((cold, noinline)) void ppu_chr_ram_alias_write(uint16_t address) {
    uint32_t slot = address >> 10;
    for (uint32_t other = 0; other < 8; other++) {
        if ((other != slot) && (chr_slot_page[other] == chr_slot_page[slot])) {
            __asm__ volatile("ocbi @%0" :: "r"(PATTERN_TABLES_ADDR + (other << 10) + (address & 0x3ff)) : "memory");
        }
    }
}

void set_ppu_pattern_table_bank_pointers(uint8_t* ppubank0data, uint8_t* ppubank1data, uint8_t* ppubank2data, uint8_t* ppubank3data, uint8_t* ppubank4data, uint8_t* ppubank5data, uint8_t* ppubank6data, uint8_t* ppubank7data) {
    uint8_t* pages[8] = { ppubank0data, ppubank1data, ppubank2data, ppubank3data, ppubank4data, ppubank5data, ppubank6data, ppubank7data };
    for (uint32_t bank = 0; bank < 8; bank++) {
        chr_slot_map(bank, pages[bank]);
    }
}

void set_ppu_nametable_mirroring(uint32_t mirroring_mode) {
    switch (mirroring_mode) {
        case NAMETABLE_MIRRORING_HORIZONTAL:
            map_nametables(0, 0, 1, 1);
            break;

        case NAMETABLE_MIRRORING_VERTICAL:
            map_nametables(0, 1, 0, 1);
            break;

        case NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW:
            map_nametables(0, 0, 0, 0);
            break;

        case NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH:
            map_nametables(1, 1, 1, 1);
            break;
    }
#ifdef MAPPER_LOG
    {
        extern uint32_t cpu_cycle_total;
        printf("MIRROR mode=%lu cyc=%lu", (unsigned long)mirroring_mode, (unsigned long)cpu_cycle_total);
        for (uint32_t slot = 0; slot < 4; slot++) {
            uint32_t sum_alias = 0, sum_page = 0;
            for (uint32_t i = 0; i < 0x400; i++) {
                sum_alias += ((uint8_t*)NAMETABLE0_ADDR)[(slot << 10) + i] * (i + 1);
                sum_page += VRAM[(nametable_slot_page[slot] << 10) + i] * (i + 1);
            }
            printf(" s%lu=%08lX/%08lX", (unsigned long)slot, (unsigned long)sum_alias, (unsigned long)sum_page);
        }
        printf("\n");
    }
#endif
}

// (save states: state_core.c, out of the link time optimized code, has this file's chunk)
