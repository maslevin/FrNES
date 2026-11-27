#include "6502_registers.h"

#include "cpu.h"
#include "memory.h"
#include "ppu.h"
#include "apu.h"
#include "mapper.h"

/**
 * Cycle by cycle 6502 for the instructions a DMA lands in.
 *
 * The C and assembly cores run whole instructions and bring the PPU / APU up to date only at register accesses.
 * That can't show where a DMA halts the CPU: the DMC's sample fetch (and OAM DMA) stops the CPU on its next read
 * cycle, repeats that read while it waits for a get cycle, and the bus values, dummy reads and register side effects
 * all depend on which cycle of which instruction it hit. So the instructions around a DMA run here instead, one bus
 * access per cycle, with the 2A03's two DMA units as the nesdev wiki describes them ("DMA",
 * https://www.nesdev.org/wiki/DMA, and "APU DMC"):
 *
 *   - CPU cycles alternate between get (DMA may read) and put (DMA may write) cycles (CPU_GET_CYCLE)
 *   - a unit can only halt the CPU on a read cycle; a write cycle makes it try again on the next one
 *   - while the CPU is halted, every cycle a unit doesn't read or write repeats the halted CPU's read
 *   - DMC DMA: halt, dummy, an alignment cycle if needed, then its read on a get cycle; OAM DMA: halt, an alignment
 *     cycle if needed, then 256 get / put pairs to $2004; a DMC get takes precedence over an OAM get, and the
 *     units' no-operation cycles overlap each other's cycles
 *   - DMC playback stopped before the halt cancels its DMA; stopped after it, the DMA ends after one cycle
 *
 * Interrupts are polled at the end of every cycle (DMA cycles included), the interrupt taken after an instruction
 * is decided by the poll at the end of its second-to-last cycle, as on the 2A03.
 *
 * cpu_exact_due() says when the next instruction has to come here: a DMA is pending, or the APU predicts a DMC
 * request within CPU_EXACT_LEAD cycles (apu.c ends batches that far before it).
 */

#ifdef FRNES_HOST
uint64_t cpu_exact_stat_runs, cpu_exact_stat_cycles, cpu_exact_stat_oam_slow;
// (the DMC fast path's outcomes, see cpu_dma_fast; HOST_DMA_FAST=0 turns it off)
uint64_t cpu_dma_fast_stat[12];
#endif

// a DMA unit waits to halt the CPU on its next read cycle (set by $4014 writes, cpu_dma_oam_start, and the APU,
// cpu_dma_dmc_start / cpu_dma_dmc_stop)
uint8_t cpu_dma_need_halt;
// a DMA in progress (only between the cycles of an exact instruction, but tested by cpu_exact_due)
uint8_t cpu_dma_active;
// the DMC unit's next step (nesdev "DMA", DMC DMA): waiting for the halt, its dummy cycle, then its get
#define DMC_UNIT_OFF 0
#define DMC_UNIT_HALT 1
#define DMC_UNIT_DUMMY 2
#define DMC_UNIT_GET 3
static uint8_t dmc_unit;
// playback stopped after the DMC unit's halt: the DMA ends with the cycle under way (an aborted DMA)
static uint8_t dmc_unit_stopped;
// the OAM unit (nesdev "DMA", OAM DMA): on from the $4014 write until its 256th put; its page; how many of its
// 512 get / put steps are done (even: a get comes next, odd: the put of the byte it holds)
static uint8_t oam_unit;
static uint8_t oam_unit_page;
static uint16_t oam_unit_step;

// set after an instruction ran here: the poll for the following instruction boundary has been made already
uint8_t cpu_exact_polled;

// (cpu.c) a taken branch that stayed on its page polls before its second cycle
extern uint8_t cpu_poll_early;

// the cycle the next bus access happens on, and when the devices next need to catch up (an event: vblank, an APU
// step, a mapper IRQ...)
static uint32_t ex_cycle;
static uint32_t ex_next_event;
// interrupt polling, as at the end of each cycle: an NMI seen (edge latched), the IRQ line with I clear, and both
// one cycle earlier
static uint8_t ex_need_nmi, ex_prev_need_nmi, ex_run_irq, ex_prev_run_irq;
// an NMI raised in the last two dots of a cycle is only seen at the end of the next one (the same point as
// CPU_NMI_LATE_DOTS in cpu.c: 5 dots before an instruction ends is 2 before its second to last cycle does)
static uint8_t ex_nmi_late;
#ifndef EX_NMI_LATE_DOTS
#define EX_NMI_LATE_DOTS 2
#endif

// 6502 registers while an instruction runs here
static uint16_t ex_pc;
static uint8_t ex_a, ex_x, ex_y, ex_sp, ex_p;

void cpu_dma_oam_start(uint8_t page) {
    // (a second $4014 write before the halt, an RMW's, only changes the page: the copy is from the last one)
    oam_unit = 1;
    oam_unit_page = page;
    oam_unit_step = 0;
    cpu_dma_need_halt = 1;
}

void cpu_dma_dmc_start(uint32_t time) {
#ifdef FRNES_HOST
    static int log = -1;
    if (log < 0) {
        log = getenv("HOST_DMA") != NULL;
    }
    if (log) {
        printf("DMCSTART t=%u now=%u\n", (unsigned)time, (unsigned)cpu_cycle_now);
    }
#endif
    (void)time;
    dmc_unit = DMC_UNIT_HALT;
    cpu_dma_need_halt = 1;
}

void cpu_dma_dmc_stop() {
    if (dmc_unit == DMC_UNIT_OFF) {
        return;
    }
    if ((dmc_unit == DMC_UNIT_HALT) && !oam_unit) {
        // the CPU isn't halted yet: no DMA at all
        dmc_unit = DMC_UNIT_OFF;
        cpu_dma_need_halt = 0;
    } else {
        // halted already (or sharing OAM DMA's halt): the DMA ends after one cycle
        dmc_unit_stopped = 1;
    }
}


/**
 * The end of a cycle: devices caught up when an event is due, then the interrupt lines polled
 */
static inline void ex_end_cycle() {
    if ((int32_t)(ex_cycle - ex_next_event) >= 0) {
        cpu_cycle_now = ex_cycle;
        cpu_sync_devices();
        // (a mapper's CPU cycle IRQ counter too: its IRQ can come up during the instruction, a DMA stall included)
        if (mapper_cpu_timer) {
            mapper_cpu_run(ex_cycle);
        }
        ex_next_event = ex_cycle + cpu_cycles_to_next_event();
        if (ppu_nmi_raised && (ppu_nmi_dots_after < EX_NMI_LATE_DOTS)) {
            ex_nmi_late = 1;
        }
    }
    ex_prev_need_nmi = ex_need_nmi;
    if (cpu_registers -> NMI_Request) {
        if (ex_nmi_late) {
            ex_nmi_late = 0;
        } else {
            ex_need_nmi = 1;
        }
    }
    ex_prev_run_irq = ex_run_irq;
    // (a line that went up during a later cycle than the one ending, see cpu_irq_raise_at, isn't seen yet)
    ex_run_irq = cpu_registers -> IRQ_Request && !(ex_p & FLAG_I) &&
        !(cpu_irq_raise_recent && ((int32_t)(ex_cycle - cpu_irq_raise_cycle) <= 0));
}

// after a register access the next event can have moved (an APU write, NMI enable...)
static inline void ex_io_done() {
    ex_next_event = cpu_devices_synced_cycle + cpu_cycles_to_next_event();
}

// the bus access itself, on cycle ex_cycle (no DMA check, no end of cycle)
#ifdef FRNES_HOST
// host builds: HOST_BUS=1 prints every bus access made here (compare with tools/mesen/bus_window.lua)
static int ex_bus_log = -1;
#define EX_BUS_LOG(kind, address, value) do { if (ex_bus_log < 0) { ex_bus_log = getenv("HOST_BUS") != NULL; } \
    if (ex_bus_log) { printf("BUS %s %04X=%02X cyc=%u\n", kind, (unsigned)(address), (unsigned)(value), (unsigned)ex_cycle + 8); } } while (0)
#else
#define EX_BUS_LOG(kind, address, value)
#endif

static inline uint8_t ex_bus_read_(uint16_t address);
static inline uint8_t ex_bus_read(uint16_t address) {
    uint8_t value = ex_bus_read_(address);
    EX_BUS_LOG("R", address, value);
    return value;
}
static inline uint8_t ex_bus_read_(uint16_t address) {
    uint8_t value;
    if (address < 0x2000) {
        value = NES_WINDOW_BYTE(address & (RAM_SIZE - 1));
    } else if ((address >= 0x8000) || ((address >= 0x6000) && WRAM)) {
        value = NES_WINDOW_BYTE(address);
    } else {
        cpu_cycle_now = ex_cycle;
        // (no poll loop skip from a cycle by cycle read: ppu_poll_skip sees PC 0)
        cpu_registers -> IO_PC = 0;
        value = read_byte(address);
        ex_io_done();
        // ($4015 only drives the CPU's internal bus: the external data bus keeps its value)
        if (address != 0x4015) {
            cpu_io_bus_value = value;
        }
        cpu_internal_bus_value = value;
        cpu_io_bus_cycle = ex_cycle;
        return value;
    }
    cpu_io_bus_value = cpu_internal_bus_value = value;
    cpu_io_bus_cycle = ex_cycle;
    return value;
}

static inline void ex_bus_write(uint16_t address, uint8_t value) {
    EX_BUS_LOG("W", address, value);
    if (address < 0x2000) {
        NES_WINDOW_BYTE(address & (RAM_SIZE - 1)) = value;
    } else {
        cpu_cycle_now = ex_cycle;
        write_byte(address, value);
        ex_io_done();
    }
    cpu_io_bus_value = cpu_internal_bus_value = value;
    cpu_io_bus_cycle = ex_cycle;
}

/*
 * DMA cycles (nesdev "DMA", https://www.nesdev.org/wiki/DMA)
 */
// a DMA cycle with nothing to transfer: the halted CPU's read made again (its side effects included), then the cycle
// ends; 'repeat' 0 leaves the read out (a joypad register: see dma_read)
static inline void dma_idle_cycle(uint16_t cpu_address, int repeat) {
    if (repeat) {
        ex_bus_read(cpu_address);
    }
    ex_cycle++;
    ex_end_cycle();
}

/*
 * A DMA unit's read (nesdev "DMA", Register conflicts). The 2A03 decodes its own registers from bits 15-5 of the
 * 6502 core's address and bits 4-0 of the bus address, and the halted core keeps the address it was reading: halted
 * on a read in $4000-$401F, a DMA read whose low 5 bits name $4015-$4017 also activates that register. Otherwise the
 * DMA can't read the 2A03's own registers at all: the bus keeps its value. Joypads see one read for a run of reads of
 * the same register ('last' holds the register read on the previous DMA cycle).
 */
static uint8_t dma_read(uint16_t address, uint16_t* last, int cpu_in_2a03) {
    if (!cpu_in_2a03) {
        uint8_t value;
        if ((address >= 0x4015) && (address <= 0x401a)) {
            value = cpu_io_bus_value;
            cpu_io_bus_cycle = ex_cycle;
        } else {
            value = ex_bus_read(address);
        }
        *last = address;
        return value;
    }
    uint16_t decoded = 0x4000 | (address & 0x1f);
    uint8_t value;
    if (decoded == 0x4015) {
        // $4015 answers on the CPU's internal bus only; the external bus sees whatever is at the DMA's own address
        uint8_t external = cpu_io_bus_value;
        value = ex_bus_read(0x4015);
        if (decoded != address) {
            uint8_t internal = cpu_internal_bus_value;
            ex_bus_read(address);
            external = cpu_io_bus_value;
            cpu_internal_bus_value = internal;
        }
        cpu_io_bus_value = external;
    } else if ((decoded == 0x4016) || (decoded == 0x4017)) {
        // the joypad is clocked once for consecutive reads of its register
        value = (*last == decoded) ? cpu_io_bus_value : ex_bus_read(decoded);
        if (decoded != address) {
            // the joypad and the memory at the DMA's address both drive the bus: the joypad's bits (4-0) read as the
            // AND of the two, the open bus bits as the memory's
            uint8_t memory = ex_bus_read(address);
            cpu_io_bus_value = (memory & 0xe0) | (value & 0x1f);
            value = (memory & 0xe0) | (value & memory & 0x1f);
        }
    } else {
        value = ex_bus_read(address);
    }
    *last = decoded;
    return value;
}

// the DMC unit's no-operation steps go by with every DMA cycle, whatever else that cycle does (they overlap OAM DMA's
// cycles), taken as the cycle starts: a stop ends it, the halt is followed by the dummy cycle, then the get
static inline void dmc_unit_step() {
    if (dmc_unit_stopped) {
        dmc_unit_stopped = 0;
        dmc_unit = DMC_UNIT_OFF;
        cpu_dma_need_halt = 0;
    } else if (dmc_unit == DMC_UNIT_HALT) {
        dmc_unit = DMC_UNIT_DUMMY;
        cpu_dma_need_halt = 0;
    } else if (dmc_unit == DMC_UNIT_DUMMY) {
        dmc_unit = DMC_UNIT_GET;
    }
}

// OAM DMA's byte to OAM through $2004 (the attribute byte's unused bits read back as 0)
static inline void oam_unit_put(uint8_t value) {
    uint8_t oam_address = ppu_registers -> OAMADDR;
    ((uint8_t*)SPRRAM_ADDR)[oam_address] = ((oam_address & 3) == 2) ? (value & 0xe3) : value;
    ppu_registers -> OAMADDR = oam_address + 1;
    ppu_oam_generation++;
    // (a $2004 write: it refreshes the PPU I/O latch)
    ppu_open_bus_drive(value, 0xff);
    EX_BUS_LOG("W", 0x2004, value);
}

// the 256 bytes at once, for OAM DMA from memory with no side effects while no DMC DMA can come: the same result as
// the cycle by cycle copy. The halt is done; an alignment cycle if this is a put cycle, then 256 gets and puts
static inline int oam_unit_whole_page(uint16_t cpu_address) {
    uint8_t page = oam_unit_page;
    int cpu_plain = (cpu_address < 0x2000) || (cpu_address >= 0x8000) || ((cpu_address >= 0x6000) && WRAM);
    int page_plain = (page < 0x20) || (page >= 0x80) || ((page >= 0x60) && WRAM);
    if ((dmc_unit != DMC_UNIT_OFF) || !cpu_plain || !page_plain ||
        (apu_dmc_request_due && ((int32_t)(apu_dmc_request_time - ex_cycle) <= 0x210))) {
        return 0;
    }
    const uint8_t* source = (page < 0x20) ? &NES_WINDOW_BYTE((page & 7) << 8) : &NES_WINDOW_BYTE(page << 8);
    uint8_t value = 0;
    uint8_t oam_address = ppu_registers -> OAMADDR;
    for (uint32_t i = 0; i < 256; i++) {
        value = source[i];
        ((uint8_t*)SPRRAM_ADDR)[oam_address] = ((oam_address & 3) == 2) ? (value & 0xe3) : value;
        oam_address++;
    }
    ppu_registers -> OAMADDR = oam_address;
    ppu_oam_generation++;
    ppu_open_bus_drive(value, 0xff);
    // all but the last cycle at once, then the last one on its own so the interrupt polls see both
    ex_cycle += 511 + (CPU_GET_CYCLE(ex_cycle) ? 0 : 1);
    ex_end_cycle();
    cpu_io_bus_value = cpu_internal_bus_value = value;
    cpu_io_bus_cycle = ex_cycle;
    ex_cycle++;
    ex_end_cycle();
    oam_unit = 0;
    return 1;
}

/*
 * The CPU's read of 'cpu_address' halted by a DMA unit: the halt cycle, then cycle by cycle until both units are
 * done. The read itself is made afterwards by the caller (the CPU resuming)
 */
static void ex_process_dma(uint16_t cpu_address);
static FRNES_HOT(exact_process_dma) void ex_process_dma(uint16_t cpu_address) {
    if (!cpu_dma_need_halt) {
        return;
    }
    uint16_t last = cpu_address;
    int cpu_in_2a03 = (cpu_address & 0xffe0) == 0x4000;
    int joypad = (cpu_address == 0x4016) || (cpu_address == 0x4017);

    // the halt cycle: the CPU's read (a joypad sees it, unless the DMC is about to read the same joypad register,
    // which makes one long read, or playback was stopped and the DMA ends here)
    int dmc_same_joypad = cpu_in_2a03 && joypad && (dmc_unit != DMC_UNIT_OFF) &&
        ((apu_dmc_dma_address() & 0x1f) == (cpu_address & 0x1f));
    int repeat = !((dmc_unit_stopped && joypad) || dmc_same_joypad);
    cpu_dma_need_halt = 0;
    if (dmc_unit == DMC_UNIT_HALT) {
        dmc_unit = DMC_UNIT_DUMMY;
    }
    dma_idle_cycle(cpu_address, repeat);
    if (dmc_unit_stopped) {
        // stopped once halted: the DMC's DMA ends with its halt cycle
        dmc_unit_stopped = 0;
        dmc_unit = DMC_UNIT_OFF;
    }
    if (!oam_unit && (dmc_unit == DMC_UNIT_OFF)) {
        return;
    }

    if (oam_unit) {
        // (the PPU sees the OAM writes from here on)
        cpu_cycle_now = ex_cycle;
        cpu_sync_devices();
        ppu_sync();
        ex_io_done();
        if (oam_unit_whole_page(cpu_address)) {
            return;
        }
#ifdef FRNES_HOST
        cpu_exact_stat_oam_slow++;
#endif
    }
    uint8_t value = 0;
    while (oam_unit || (dmc_unit != DMC_UNIT_OFF)) {
        // (the APU can stop the DMC between cycles, or request a DMA)
        if ((dmc_unit != DMC_UNIT_OFF) || (apu_dmc_request_due && ((int32_t)(ex_cycle - apu_dmc_request_time) >= 0))) {
            cpu_cycle_now = ex_cycle;
            cpu_sync_devices();
        }
        int get = CPU_GET_CYCLE(ex_cycle);
        if (get && (dmc_unit == DMC_UNIT_GET)) {
            // the DMC's get (before OAM DMA's)
            dmc_unit_stopped = 0;
            value = dma_read(apu_dmc_dma_address(), &last, cpu_in_2a03);
            ex_cycle++;
            // (the cycle's interrupt poll comes before the byte reaches the DMC and its IRQ)
            ex_end_cycle();
            cpu_cycle_now = ex_cycle;
            cpu_sync_devices();
            dmc_unit = DMC_UNIT_OFF;
            dmc_unit_stopped = 0;
            apu_dmc_dma_done(value);
            ex_io_done();
            // the sample buffer taken at once (the sample starting again): the new DMA halts from here
            if (cpu_dma_need_halt) {
                ex_process_dma(cpu_address);
            }
            continue;
        }
        dmc_unit_step();
        if (oam_unit && (oam_unit_step & 1) == !get) {
            if (get) {
                value = dma_read(((uint16_t)oam_unit_page << 8) | (uint8_t)(oam_unit_step >> 1), &last, cpu_in_2a03);
            } else {
                oam_unit_put(value);
                cpu_io_bus_value = value;
                cpu_io_bus_cycle = ex_cycle;
            }
            ex_cycle++;
            ex_end_cycle();
            if (++oam_unit_step == 512) {
                oam_unit = 0;
            }
        } else {
            // no transfer: a DMC halt or dummy cycle, or an alignment cycle (repeated joypad reads aren't seen)
            dma_idle_cycle(cpu_address, !joypad);
        }
    }
}

/**
 * Bus cycles of the instruction: a read (where a pending DMA halts the CPU) and a write
 */
static inline uint8_t ex_read(uint16_t address) {
    if (apu_dmc_request_due && ((int32_t)(ex_cycle - apu_dmc_request_time) >= 0) && !cpu_dma_need_halt) {
        // the DMC's request comes up during the cycles before this one
        cpu_cycle_now = ex_cycle;
        cpu_sync_devices();
        ex_io_done();
    }
    if (cpu_dma_need_halt) {
        ex_process_dma(address);
    }
    uint8_t value = ex_bus_read(address);
    ex_cycle++;
    ex_end_cycle();
    return value;
}

static inline void ex_write(uint16_t address, uint8_t value) {
    ex_bus_write(address, value);
    ex_cycle++;
    ex_end_cycle();
}

#define EX_FETCH() ex_read(ex_pc++)
#define EX_DUMMY_PC() ex_read(ex_pc)
#define EX_PUSH(v) do { ex_write(BASE_STACK + ex_sp, (v)); ex_sp--; } while (0)
#define EX_POP() (ex_sp++, ex_read(BASE_STACK + ex_sp))
#define EX_NZ(b) (((b) & FLAG_N) | ((b) ? 0 : FLAG_Z))
#define EX_TEST(b) ex_p = (ex_p & ~(FLAG_N | FLAG_Z)) | EX_NZ(b)

static inline uint16_t ex_fetch_word() {
    uint16_t low = EX_FETCH();
    return low | (EX_FETCH() << 8);
}

/*
 * Addressing modes (the cycles before the data access)
 */
static inline uint16_t ex_zp_indexed(uint8_t index) {
    uint8_t base = EX_FETCH();
    ex_read(base);
    return (uint8_t)(base + index);
}

// abs,X / abs,Y: the dummy read with the index added to the low byte only, always for writes / read-modify-writes
static inline uint16_t ex_abs_indexed(uint8_t index, int always_dummy, uint16_t* base_out) {
    uint16_t base = ex_fetch_word();
    uint16_t address = base + index;
    int crossed = (base ^ address) & 0xff00;
    if (crossed || always_dummy) {
        ex_read((base & 0xff00) | (address & 0x00ff));
    }
    if (base_out) {
        *base_out = base;
    }
    return address;
}

static inline uint16_t ex_ind_x() {
    uint8_t pointer = EX_FETCH();
    ex_read(pointer);
    pointer += ex_x;
    uint16_t low = ex_read(pointer);
    return low | (ex_read((uint8_t)(pointer + 1)) << 8);
}

static inline uint16_t ex_ind_y(int always_dummy, uint16_t* base_out) {
    uint8_t pointer = EX_FETCH();
    uint16_t base = ex_read(pointer);
    base |= ex_read((uint8_t)(pointer + 1)) << 8;
    uint16_t address = base + ex_y;
    if (((base ^ address) & 0xff00) || always_dummy) {
        ex_read((base & 0xff00) | (address & 0x00ff));
    }
    if (base_out) {
        *base_out = base;
    }
    return address;
}

/*
 * ALU, the same results as cpu.c
 */
#define EX_UNSTABLE_MAGIC 0xEE

static inline void ex_adc(uint8_t value) {
    uint16_t sum = ex_a + value + (ex_p & FLAG_C);
    ex_p = (ex_p & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) | ((sum >> 8) & FLAG_C) |
        (((ex_a ^ sum) & (value ^ sum) & 0x80) ? FLAG_V : 0) | EX_NZ((uint8_t)sum);
    ex_a = (uint8_t)sum;
}

static inline void ex_compare(uint8_t reg, uint8_t value) {
    ex_p = (ex_p & ~(FLAG_N | FLAG_Z | FLAG_C)) | ((reg >= value) ? FLAG_C : 0) | EX_NZ((uint8_t)(reg - value));
}

static inline uint8_t ex_asl(uint8_t v) { ex_p = (ex_p & ~FLAG_C) | (v >> 7); v <<= 1; EX_TEST(v); return v; }
static inline uint8_t ex_lsr(uint8_t v) { ex_p = (ex_p & ~FLAG_C) | (v & 1); v >>= 1; EX_TEST(v); return v; }
static inline uint8_t ex_rol(uint8_t v) { uint8_t c = ex_p & FLAG_C; ex_p = (ex_p & ~FLAG_C) | (v >> 7); v = (v << 1) | c; EX_TEST(v); return v; }
static inline uint8_t ex_ror(uint8_t v) { uint8_t c = ex_p & FLAG_C; ex_p = (ex_p & ~FLAG_C) | (v & 1); v = (v >> 1) | (c << 7); EX_TEST(v); return v; }

enum { OP_ORA, OP_AND, OP_EOR, OP_ADC, OP_CMP, OP_SBC, OP_LDA, OP_LDX, OP_LDY, OP_LAX, OP_CPX, OP_CPY, OP_BIT, OP_NOP, OP_LAS };

static inline void ex_read_op(int op, uint8_t v) {
    switch (op) {
        case OP_ORA: ex_a |= v; EX_TEST(ex_a); break;
        case OP_AND: ex_a &= v; EX_TEST(ex_a); break;
        case OP_EOR: ex_a ^= v; EX_TEST(ex_a); break;
        case OP_ADC: ex_adc(v); break;
        case OP_SBC: ex_adc((uint8_t)~v); break;
        case OP_CMP: ex_compare(ex_a, v); break;
        case OP_CPX: ex_compare(ex_x, v); break;
        case OP_CPY: ex_compare(ex_y, v); break;
        case OP_LDA: ex_a = v; EX_TEST(v); break;
        case OP_LDX: ex_x = v; EX_TEST(v); break;
        case OP_LDY: ex_y = v; EX_TEST(v); break;
        case OP_LAX: ex_a = ex_x = v; EX_TEST(v); break;
        case OP_BIT: ex_p = (ex_p & ~(FLAG_N | FLAG_V | FLAG_Z)) | (v & (FLAG_N | FLAG_V)) | ((v & ex_a) ? 0 : FLAG_Z); break;
        case OP_LAS: ex_sp &= v; ex_a = ex_x = ex_sp; EX_TEST(ex_a); break;
        default: break;
    }
}

enum { RMW_ASL, RMW_LSR, RMW_ROL, RMW_ROR, RMW_INC, RMW_DEC, RMW_SLO, RMW_RLA, RMW_SRE, RMW_RRA, RMW_DCP, RMW_ISC };

static inline uint8_t ex_rmw_op(int op, uint8_t v) {
    switch (op) {
        case RMW_ASL: return ex_asl(v);
        case RMW_LSR: return ex_lsr(v);
        case RMW_ROL: return ex_rol(v);
        case RMW_ROR: return ex_ror(v);
        case RMW_INC: v++; EX_TEST(v); return v;
        case RMW_DEC: v--; EX_TEST(v); return v;
        case RMW_SLO: v = ex_asl(v); ex_a |= v; EX_TEST(ex_a); return v;
        case RMW_RLA: v = ex_rol(v); ex_a &= v; EX_TEST(ex_a); return v;
        case RMW_SRE: v = ex_lsr(v); ex_a ^= v; EX_TEST(ex_a); return v;
        case RMW_RRA: v = ex_ror(v); ex_adc(v); return v;
        case RMW_DCP: v--; ex_compare(ex_a, v); return v;
        case RMW_ISC: v++; ex_adc((uint8_t)~v); return v;
    }
    return v;
}

// read-modify-write: the read, the unmodified value written back, then the result
static inline void ex_rmw(uint16_t address, int op) {
    uint8_t v = ex_read(address);
    ex_write(address, v);
    if (cpu_registers -> NMI_Request == 2) {
        // (see RMW in cpu.c: an NMI enabled by the first write isn't delayed by the second)
        cpu_registers -> NMI_Request = 1;
    }
    ex_write(address, ex_rmw_op(op, v));
}

/*
 * SHA / SHX / SHY / SHS: the value ANDed with the base address's high byte + 1 (not when a DMA hit the cycle before
 * the write), and on a page cross that value replaces the high byte of the address
 */
static inline void ex_sh_store(uint16_t base, uint8_t index, uint8_t value) {
    uint16_t address = base + index;
    int crossed = (base ^ address) & 0xff00;
    uint32_t before = ex_cycle;
    ex_read((base & 0xff00) | (address & 0x00ff));
    int had_dma = (ex_cycle - before) > 1;
    uint8_t high = address >> 8;
    if (crossed) {
        high &= value;
    }
    ex_write((high << 8) | (address & 0xff), had_dma ? value : (value & ((base >> 8) + 1)));
}

static inline void ex_branch(int taken) {
    int8_t offset = (int8_t)EX_FETCH();
    if (!taken) {
        return;
    }
    // a taken branch polls at the end of its first cycle (not its second), and with a page cross again at the end of its
    // third: an IRQ seen by either poll is taken
    ex_run_irq = ex_prev_run_irq;
    uint16_t target = ex_pc + offset;
    if (!((target ^ ex_pc) & 0xff00)) {
        // (the NMI too: one that came up during its second cycle waits for the next instruction)
        ex_need_nmi = ex_prev_need_nmi;
    }
    ex_read(ex_pc);
    if ((target ^ ex_pc) & 0xff00) {
        ex_run_irq |= ex_prev_run_irq;
        ex_read((ex_pc & 0xff00) | (target & 0x00ff));
    } else {
        // (for a boundary poll after a CPU_EXACT_DEFER instruction, as the C core's BRA)
        cpu_poll_early = 1;
    }
    ex_pc = target;
}

// the 7 cycle interrupt sequence (IRQ, NMI, BRK's last 5 cycles): an NMI seen by its 4th cycle takes the vector over
static void ex_interrupt_tail(uint8_t pushed_p) {
    uint16_t vector;
    if (ex_need_nmi) {
        ex_need_nmi = 0;
        cpu_registers -> NMI_Request = 0;
        vector = VECTOR_NMI;
    } else {
        vector = VECTOR_IRQ;
    }
    EX_PUSH(pushed_p);
    ex_p |= FLAG_I;
    mapper_vector_fetch(vector);
    uint16_t low = ex_read(vector);
    ex_pc = low | (ex_read(vector + 1) << 8);
}

static void ex_interrupt() {
    ex_read(ex_pc);
    ex_read(ex_pc);
    EX_PUSH(ex_pc >> 8);
    EX_PUSH(ex_pc & 0xff);
    ex_interrupt_tail((ex_p & ~FLAG_B) | FLAG_R);
}

#define RD(mode_address, op) ex_read_op(op, ex_read(mode_address))
#define ZP() EX_FETCH()
#define ABS() ex_fetch_word()

static int ex_instruction() {
    uint8_t opcode = EX_FETCH();
    uint16_t address, base;
    switch (opcode) {
        // reads: immediate, zp, zp,X / zp,Y, abs, abs,X / abs,Y, (zp,X), (zp),Y
#define READ_GROUP(imm, zp, zpx, abs, absx, absy, izx, izy, op) \
        case imm: ex_read_op(op, EX_FETCH()); break; \
        case zp: RD(ZP(), op); break; \
        case zpx: RD(ex_zp_indexed(ex_x), op); break; \
        case abs: RD(ABS(), op); break; \
        case absx: RD(ex_abs_indexed(ex_x, 0, NULL), op); break; \
        case absy: RD(ex_abs_indexed(ex_y, 0, NULL), op); break; \
        case izx: RD(ex_ind_x(), op); break; \
        case izy: RD(ex_ind_y(0, NULL), op); break;
        READ_GROUP(0x09, 0x05, 0x15, 0x0D, 0x1D, 0x19, 0x01, 0x11, OP_ORA)
        READ_GROUP(0x29, 0x25, 0x35, 0x2D, 0x3D, 0x39, 0x21, 0x31, OP_AND)
        READ_GROUP(0x49, 0x45, 0x55, 0x4D, 0x5D, 0x59, 0x41, 0x51, OP_EOR)
        READ_GROUP(0x69, 0x65, 0x75, 0x6D, 0x7D, 0x79, 0x61, 0x71, OP_ADC)
        READ_GROUP(0xC9, 0xC5, 0xD5, 0xCD, 0xDD, 0xD9, 0xC1, 0xD1, OP_CMP)
        READ_GROUP(0xE9, 0xE5, 0xF5, 0xED, 0xFD, 0xF9, 0xE1, 0xF1, OP_SBC)
        READ_GROUP(0xA9, 0xA5, 0xB5, 0xAD, 0xBD, 0xB9, 0xA1, 0xB1, OP_LDA)
#undef READ_GROUP
        case 0xEB: ex_read_op(OP_SBC, EX_FETCH()); break;
        case 0xA2: ex_read_op(OP_LDX, EX_FETCH()); break;
        case 0xA6: RD(ZP(), OP_LDX); break;
        case 0xB6: RD(ex_zp_indexed(ex_y), OP_LDX); break;
        case 0xAE: RD(ABS(), OP_LDX); break;
        case 0xBE: RD(ex_abs_indexed(ex_y, 0, NULL), OP_LDX); break;
        case 0xA0: ex_read_op(OP_LDY, EX_FETCH()); break;
        case 0xA4: RD(ZP(), OP_LDY); break;
        case 0xB4: RD(ex_zp_indexed(ex_x), OP_LDY); break;
        case 0xAC: RD(ABS(), OP_LDY); break;
        case 0xBC: RD(ex_abs_indexed(ex_x, 0, NULL), OP_LDY); break;
        case 0xE0: ex_read_op(OP_CPX, EX_FETCH()); break;
        case 0xE4: RD(ZP(), OP_CPX); break;
        case 0xEC: RD(ABS(), OP_CPX); break;
        case 0xC0: ex_read_op(OP_CPY, EX_FETCH()); break;
        case 0xC4: RD(ZP(), OP_CPY); break;
        case 0xCC: RD(ABS(), OP_CPY); break;
        case 0x24: RD(ZP(), OP_BIT); break;
        case 0x2C: RD(ABS(), OP_BIT); break;
        case 0xA7: RD(ZP(), OP_LAX); break;
        case 0xB7: RD(ex_zp_indexed(ex_y), OP_LAX); break;
        case 0xAF: RD(ABS(), OP_LAX); break;
        case 0xBF: RD(ex_abs_indexed(ex_y, 0, NULL), OP_LAX); break;
        case 0xA3: RD(ex_ind_x(), OP_LAX); break;
        case 0xB3: RD(ex_ind_y(0, NULL), OP_LAX); break;
        case 0xBB: RD(ex_abs_indexed(ex_y, 0, NULL), OP_LAS); break;

        // NOPs, the memory forms still read
        case 0xEA: case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA: EX_DUMMY_PC(); break;
        case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2: EX_FETCH(); break;
        case 0x04: case 0x44: case 0x64: ex_read(ZP()); break;
        case 0x14: case 0x34: case 0x54: case 0x74: case 0xD4: case 0xF4: ex_read(ex_zp_indexed(ex_x)); break;
        case 0x0C: ex_read(ABS()); break;
        case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC: ex_read(ex_abs_indexed(ex_x, 0, NULL)); break;

        // stores
        case 0x85: ex_write(ZP(), ex_a); break;
        case 0x95: ex_write(ex_zp_indexed(ex_x), ex_a); break;
        case 0x8D: ex_write(ABS(), ex_a); break;
        case 0x9D: ex_write(ex_abs_indexed(ex_x, 1, NULL), ex_a); break;
        case 0x99: ex_write(ex_abs_indexed(ex_y, 1, NULL), ex_a); break;
        case 0x81: ex_write(ex_ind_x(), ex_a); break;
        case 0x91: ex_write(ex_ind_y(1, NULL), ex_a); break;
        case 0x86: ex_write(ZP(), ex_x); break;
        case 0x96: ex_write(ex_zp_indexed(ex_y), ex_x); break;
        case 0x8E: ex_write(ABS(), ex_x); break;
        case 0x84: ex_write(ZP(), ex_y); break;
        case 0x94: ex_write(ex_zp_indexed(ex_x), ex_y); break;
        case 0x8C: ex_write(ABS(), ex_y); break;
        case 0x87: ex_write(ZP(), ex_a & ex_x); break;
        case 0x97: ex_write(ex_zp_indexed(ex_y), ex_a & ex_x); break;
        case 0x8F: ex_write(ABS(), ex_a & ex_x); break;
        case 0x83: ex_write(ex_ind_x(), ex_a & ex_x); break;

        // read-modify-write
#define RMW_GROUP(zp, zpx, abs, absx, op) \
        case zp: ex_rmw(ZP(), op); break; \
        case zpx: ex_rmw(ex_zp_indexed(ex_x), op); break; \
        case abs: ex_rmw(ABS(), op); break; \
        case absx: ex_rmw(ex_abs_indexed(ex_x, 1, NULL), op); break;
        RMW_GROUP(0x06, 0x16, 0x0E, 0x1E, RMW_ASL)
        RMW_GROUP(0x46, 0x56, 0x4E, 0x5E, RMW_LSR)
        RMW_GROUP(0x26, 0x36, 0x2E, 0x3E, RMW_ROL)
        RMW_GROUP(0x66, 0x76, 0x6E, 0x7E, RMW_ROR)
        RMW_GROUP(0xE6, 0xF6, 0xEE, 0xFE, RMW_INC)
        RMW_GROUP(0xC6, 0xD6, 0xCE, 0xDE, RMW_DEC)
#undef RMW_GROUP
#define URMW_GROUP(zp, zpx, abs, absx, absy, izx, izy, op) \
        case zp: ex_rmw(ZP(), op); break; \
        case zpx: ex_rmw(ex_zp_indexed(ex_x), op); break; \
        case abs: ex_rmw(ABS(), op); break; \
        case absx: ex_rmw(ex_abs_indexed(ex_x, 1, NULL), op); break; \
        case absy: ex_rmw(ex_abs_indexed(ex_y, 1, NULL), op); break; \
        case izx: ex_rmw(ex_ind_x(), op); break; \
        case izy: ex_rmw(ex_ind_y(1, NULL), op); break;
        URMW_GROUP(0x07, 0x17, 0x0F, 0x1F, 0x1B, 0x03, 0x13, RMW_SLO)
        URMW_GROUP(0x27, 0x37, 0x2F, 0x3F, 0x3B, 0x23, 0x33, RMW_RLA)
        URMW_GROUP(0x47, 0x57, 0x4F, 0x5F, 0x5B, 0x43, 0x53, RMW_SRE)
        URMW_GROUP(0x67, 0x77, 0x6F, 0x7F, 0x7B, 0x63, 0x73, RMW_RRA)
        URMW_GROUP(0xC7, 0xD7, 0xCF, 0xDF, 0xDB, 0xC3, 0xD3, RMW_DCP)
        URMW_GROUP(0xE7, 0xF7, 0xEF, 0xFF, 0xFB, 0xE3, 0xF3, RMW_ISC)
#undef URMW_GROUP

        // accumulator / implied
        case 0x0A: EX_DUMMY_PC(); ex_a = ex_asl(ex_a); break;
        case 0x4A: EX_DUMMY_PC(); ex_a = ex_lsr(ex_a); break;
        case 0x2A: EX_DUMMY_PC(); ex_a = ex_rol(ex_a); break;
        case 0x6A: EX_DUMMY_PC(); ex_a = ex_ror(ex_a); break;
        case 0xE8: EX_DUMMY_PC(); ex_x++; EX_TEST(ex_x); break;
        case 0xC8: EX_DUMMY_PC(); ex_y++; EX_TEST(ex_y); break;
        case 0xCA: EX_DUMMY_PC(); ex_x--; EX_TEST(ex_x); break;
        case 0x88: EX_DUMMY_PC(); ex_y--; EX_TEST(ex_y); break;
        case 0xAA: EX_DUMMY_PC(); ex_x = ex_a; EX_TEST(ex_x); break;
        case 0xA8: EX_DUMMY_PC(); ex_y = ex_a; EX_TEST(ex_y); break;
        case 0x8A: EX_DUMMY_PC(); ex_a = ex_x; EX_TEST(ex_a); break;
        case 0x98: EX_DUMMY_PC(); ex_a = ex_y; EX_TEST(ex_a); break;
        case 0xBA: EX_DUMMY_PC(); ex_x = ex_sp; EX_TEST(ex_x); break;
        case 0x9A: EX_DUMMY_PC(); ex_sp = ex_x; break;
        case 0x18: EX_DUMMY_PC(); ex_p &= ~FLAG_C; break;
        case 0x38: EX_DUMMY_PC(); ex_p |= FLAG_C; break;
        case 0x58: EX_DUMMY_PC(); ex_p &= ~FLAG_I; break;
        case 0x78: EX_DUMMY_PC(); ex_p |= FLAG_I; break;
        case 0xB8: EX_DUMMY_PC(); ex_p &= ~FLAG_V; break;
        case 0xD8: EX_DUMMY_PC(); ex_p &= ~FLAG_D; break;
        case 0xF8: EX_DUMMY_PC(); ex_p |= FLAG_D; break;

        // stack
        case 0x48: EX_DUMMY_PC(); EX_PUSH(ex_a); break;
        case 0x08: EX_DUMMY_PC(); EX_PUSH(ex_p | FLAG_B | FLAG_R); break;
        case 0x68: EX_DUMMY_PC(); ex_read(BASE_STACK + ex_sp); ex_a = EX_POP(); EX_TEST(ex_a); break;
        case 0x28: EX_DUMMY_PC(); ex_read(BASE_STACK + ex_sp); ex_p = (EX_POP() & ~FLAG_B) | FLAG_R; break;

        // jumps, subroutines, interrupts
        case 0x4C: ex_pc = ABS(); break;
        case 0x6C: {
            uint16_t pointer = ABS();
            uint16_t low = ex_read(pointer);
            ex_pc = low | (ex_read((pointer & 0xff00) | ((pointer + 1) & 0x00ff)) << 8);
        } break;
        case 0x20: {
            uint16_t low = EX_FETCH();
            ex_read(BASE_STACK + ex_sp);
            EX_PUSH(ex_pc >> 8);
            EX_PUSH(ex_pc & 0xff);
            // (the high byte is read after the pushes)
            ex_pc = low | (ex_read(ex_pc) << 8);
        } break;
        case 0x60: {
            EX_DUMMY_PC();
            ex_read(BASE_STACK + ex_sp);
            uint16_t low = EX_POP();
            uint16_t target = low | (EX_POP() << 8);
            ex_read(target);
            ex_pc = target + 1;
        } break;
        case 0x40: {
            EX_DUMMY_PC();
            ex_read(BASE_STACK + ex_sp);
            ex_p = (EX_POP() & ~FLAG_B) | FLAG_R;
            uint16_t low = EX_POP();
            ex_pc = low | (EX_POP() << 8);
        } break;
        case 0x00: {
            EX_FETCH();
            EX_PUSH(ex_pc >> 8);
            EX_PUSH(ex_pc & 0xff);
            ex_interrupt_tail(ex_p | FLAG_B | FLAG_R);
            // the handler's first instruction runs before an NMI
            ex_prev_need_nmi = 0;
        } break;

        // branches
        case 0x10: ex_branch(!(ex_p & FLAG_N)); break;
        case 0x30: ex_branch(ex_p & FLAG_N); break;
        case 0x50: ex_branch(!(ex_p & FLAG_V)); break;
        case 0x70: ex_branch(ex_p & FLAG_V); break;
        case 0x90: ex_branch(!(ex_p & FLAG_C)); break;
        case 0xB0: ex_branch(ex_p & FLAG_C); break;
        case 0xD0: ex_branch(!(ex_p & FLAG_Z)); break;
        case 0xF0: ex_branch(ex_p & FLAG_Z); break;

        // unofficial immediates
        case 0x0B: case 0x2B: ex_read_op(OP_AND, EX_FETCH()); ex_p = (ex_p & ~FLAG_C) | (ex_a >> 7); break;
        case 0x4B: ex_a &= EX_FETCH(); ex_a = ex_lsr(ex_a); break;
        case 0x6B: {
            ex_a = ((ex_a & EX_FETCH()) >> 1) | ((ex_p & FLAG_C) << 7);
            ex_p = (ex_p & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) | EX_NZ(ex_a) | ((ex_a >> 6) & FLAG_C) |
                ((((ex_a >> 6) ^ (ex_a >> 5)) & 1) ? FLAG_V : 0);
        } break;
        case 0xCB: {
            uint8_t ax = ex_a & ex_x;
            uint8_t value = EX_FETCH();
            ex_compare(ax, value);
            ex_x = ax - value;
        } break;
        case 0x8B: { uint8_t v = (ex_a | EX_UNSTABLE_MAGIC) & ex_x & EX_FETCH(); ex_a = v; EX_TEST(v); } break;
        case 0xAB: { uint8_t v = EX_FETCH(); ex_a = ex_x = v; EX_TEST(v); } break;

        // SHA / SHX / SHY / SHS
        case 0x93: {
            uint8_t pointer = EX_FETCH();
            base = ex_read(pointer);
            base |= ex_read((uint8_t)(pointer + 1)) << 8;
            ex_sh_store(base, ex_y, ex_a & ex_x);
        } break;
        case 0x9F: base = ABS(); ex_sh_store(base, ex_y, ex_a & ex_x); break;
        case 0x9E: base = ABS(); ex_sh_store(base, ex_y, ex_x); break;
        case 0x9C: base = ABS(); ex_sh_store(base, ex_x, ex_y); break;
        case 0x9B: base = ABS(); ex_sp = ex_a & ex_x; ex_sh_store(base, ex_y, ex_sp); break;

        // JAM: the caller reports it
        default:
            ex_pc--;
            return -1;
    }
    (void)address;
    return 0;
}

/**
 * Runs one instruction (and the interrupt its last poll asks for), or with 'interrupt' set the IRQ / NMI sequence
 * (NMI when one is pending), cycle by cycle from cycle 'cycle' on the registers in cpu_registers. Returns the
 * cycles used, or CPU_HALTED for a JAM opcode (PC left on it).
 */
uint32_t cpu_exact_run(uint32_t cycle, int interrupt) {
    // CPU_EXACT_DEFER: one instruction only, ending at its last cycle; an interrupt its poll saw is left to the next
    // boundary's poll (cpu_boundary_poll, as the C core takes it there), so the batch and frame end where the C
    // core's do
    int defer = (interrupt == CPU_EXACT_DEFER);
    if (defer) {
        interrupt = 0;
    }
#ifdef FRNES_HOST
    cpu_exact_stat_runs++;
#endif
    ex_pc = cpu_registers -> PC;
    ex_a = cpu_registers -> A;
    ex_x = cpu_registers -> X;
    ex_y = cpu_registers -> Y;
    ex_sp = cpu_registers -> SP;
    ex_p = cpu_registers -> S;
    ex_cycle = cycle;
    ex_next_event = cpu_devices_synced_cycle + cpu_cycles_to_next_event();
    // the poll at the boundary before this instruction has been made: an NMI still pending was seen before it
    ex_need_nmi = ex_prev_need_nmi = (cpu_registers -> NMI_Request != 0);
    if (ex_need_nmi) {
        cpu_registers -> NMI_Request = 1;
    }
    ex_nmi_late = 0;
    ex_run_irq = ex_prev_run_irq = 0;

    if (interrupt) {
        // the poll before this point asked for an interrupt (cpu_run_c decided): the sequence, polled at its end
        ex_interrupt();
    } else {
        if (ex_instruction() < 0) {
            cpu_registers -> PC = ex_pc;
            return CPU_HALTED;
        }
        if (!defer && (ex_prev_need_nmi || ex_prev_run_irq)) {
            ex_interrupt();
        }
    }
    if (!defer) {
        // an NMI seen during the last cycle is taken after the next instruction
        if (ex_need_nmi) {
            cpu_registers -> NMI_Request = 1;
        }
        cpu_exact_polled = 1;
    }

    cpu_registers -> PC = ex_pc;
    cpu_registers -> A = ex_a;
    cpu_registers -> X = ex_x;
    cpu_registers -> Y = ex_y;
    cpu_registers -> SP = ex_sp;
    cpu_registers -> S = ex_p;
#ifdef FRNES_HOST
    cpu_exact_stat_cycles += ex_cycle - cycle;
#endif
    return ex_cycle - cycle;
}

#ifndef CPU_C_ONLY
/**
 * The DMC DMA fast path (step 3 of tests/perf/consolidation-plan.md). A DMC fetch changes three things about the
 * instruction it lands in: how many cycles it stalls, which read is repeated while halted and what the interrupt polls
 * see. When every bus access of the instruction is to RAM / ROM (the repeated reads have no side effects, no register
 * sees the cycles move) and no device event comes before its end (no interrupt line changes, the polls see what the
 * boundary's did), the DMA is made here and the assembly core runs the instruction with the stall added.
 *
 * From the instruction's bus pattern (ex_plan): the DMA halts on its first read at or after the request, then takes
 * a dummy cycle, an alignment cycle when the next one is a put, and its get cycle, and the CPU makes its read again:
 * 3 or 4 cycles. Everything else (an access to a register or the open bus, OAM DMA, a $4015 write cancelling the DMC,
 * a DMC IRQ or restart, an interrupt pending or a device event in the span) stays cycle by cycle in cpu_exact_run.
 */
// each instruction's addressing (EX_PLAN_*) and data access (EX_PLAN_READ / WRITE / RMW, in bits 5-6)
enum {
    EX_PLAN_NONE, EX_PLAN_IMPLIED, EX_PLAN_ZP, EX_PLAN_ZPX, EX_PLAN_ZPY, EX_PLAN_ABS, EX_PLAN_ABSX, EX_PLAN_ABSY,
    EX_PLAN_INDX, EX_PLAN_INDY, EX_PLAN_BRANCH, EX_PLAN_JMP, EX_PLAN_JMPI, EX_PLAN_JSR, EX_PLAN_RTS, EX_PLAN_RTI,
    EX_PLAN_PUSH, EX_PLAN_PULL
};
#define EX_PLAN_READ 0x20
#define EX_PLAN_WRITE 0x40
#define EX_PLAN_RMW 0x60
#define EX_R(mode) ((mode) | EX_PLAN_READ)
#define EX_W(mode) ((mode) | EX_PLAN_WRITE)
#define EX_M(mode) ((mode) | EX_PLAN_RMW)
#define EX_READ_GROUP(imm, zp, zpx, abs, absx, absy, izx, izy) [imm] = EX_PLAN_IMPLIED, [zp] = EX_R(EX_PLAN_ZP), \
    [zpx] = EX_R(EX_PLAN_ZPX), [abs] = EX_R(EX_PLAN_ABS), [absx] = EX_R(EX_PLAN_ABSX), [absy] = EX_R(EX_PLAN_ABSY), \
    [izx] = EX_R(EX_PLAN_INDX), [izy] = EX_R(EX_PLAN_INDY)
#define EX_RMW_GROUP(acc, zp, zpx, abs, absx) [acc] = EX_PLAN_IMPLIED, [zp] = EX_M(EX_PLAN_ZP), [zpx] = EX_M(EX_PLAN_ZPX), \
    [abs] = EX_M(EX_PLAN_ABS), [absx] = EX_M(EX_PLAN_ABSX)
// (official opcodes only; the immediates read their operand like the implied ones read PC + 1)
static const uint8_t ex_plan_mode[256] = {
    EX_READ_GROUP(0x09, 0x05, 0x15, 0x0D, 0x1D, 0x19, 0x01, 0x11),
    EX_READ_GROUP(0x29, 0x25, 0x35, 0x2D, 0x3D, 0x39, 0x21, 0x31),
    EX_READ_GROUP(0x49, 0x45, 0x55, 0x4D, 0x5D, 0x59, 0x41, 0x51),
    EX_READ_GROUP(0x69, 0x65, 0x75, 0x6D, 0x7D, 0x79, 0x61, 0x71),
    EX_READ_GROUP(0xC9, 0xC5, 0xD5, 0xCD, 0xDD, 0xD9, 0xC1, 0xD1),
    EX_READ_GROUP(0xE9, 0xE5, 0xF5, 0xED, 0xFD, 0xF9, 0xE1, 0xF1),
    EX_READ_GROUP(0xA9, 0xA5, 0xB5, 0xAD, 0xBD, 0xB9, 0xA1, 0xB1),
    [0x85] = EX_W(EX_PLAN_ZP), [0x95] = EX_W(EX_PLAN_ZPX), [0x8D] = EX_W(EX_PLAN_ABS), [0x9D] = EX_W(EX_PLAN_ABSX),
    [0x99] = EX_W(EX_PLAN_ABSY), [0x81] = EX_W(EX_PLAN_INDX), [0x91] = EX_W(EX_PLAN_INDY),
    [0xA2] = EX_PLAN_IMPLIED, [0xA6] = EX_R(EX_PLAN_ZP), [0xB6] = EX_R(EX_PLAN_ZPY), [0xAE] = EX_R(EX_PLAN_ABS),
    [0xBE] = EX_R(EX_PLAN_ABSY), [0x86] = EX_W(EX_PLAN_ZP), [0x96] = EX_W(EX_PLAN_ZPY), [0x8E] = EX_W(EX_PLAN_ABS),
    [0xA0] = EX_PLAN_IMPLIED, [0xA4] = EX_R(EX_PLAN_ZP), [0xB4] = EX_R(EX_PLAN_ZPX), [0xAC] = EX_R(EX_PLAN_ABS),
    [0xBC] = EX_R(EX_PLAN_ABSX), [0x84] = EX_W(EX_PLAN_ZP), [0x94] = EX_W(EX_PLAN_ZPX), [0x8C] = EX_W(EX_PLAN_ABS),
    [0xE0] = EX_PLAN_IMPLIED, [0xE4] = EX_R(EX_PLAN_ZP), [0xEC] = EX_R(EX_PLAN_ABS),
    [0xC0] = EX_PLAN_IMPLIED, [0xC4] = EX_R(EX_PLAN_ZP), [0xCC] = EX_R(EX_PLAN_ABS),
    [0x24] = EX_R(EX_PLAN_ZP), [0x2C] = EX_R(EX_PLAN_ABS),
    EX_RMW_GROUP(0x0A, 0x06, 0x16, 0x0E, 0x1E), EX_RMW_GROUP(0x4A, 0x46, 0x56, 0x4E, 0x5E),
    EX_RMW_GROUP(0x2A, 0x26, 0x36, 0x2E, 0x3E), EX_RMW_GROUP(0x6A, 0x66, 0x76, 0x6E, 0x7E),
    [0xE6] = EX_M(EX_PLAN_ZP), [0xF6] = EX_M(EX_PLAN_ZPX), [0xEE] = EX_M(EX_PLAN_ABS), [0xFE] = EX_M(EX_PLAN_ABSX),
    [0xC6] = EX_M(EX_PLAN_ZP), [0xD6] = EX_M(EX_PLAN_ZPX), [0xCE] = EX_M(EX_PLAN_ABS), [0xDE] = EX_M(EX_PLAN_ABSX),
    [0xE8] = EX_PLAN_IMPLIED, [0xC8] = EX_PLAN_IMPLIED, [0xCA] = EX_PLAN_IMPLIED, [0x88] = EX_PLAN_IMPLIED,
    [0xAA] = EX_PLAN_IMPLIED, [0xA8] = EX_PLAN_IMPLIED, [0x8A] = EX_PLAN_IMPLIED, [0x98] = EX_PLAN_IMPLIED,
    [0xBA] = EX_PLAN_IMPLIED, [0x9A] = EX_PLAN_IMPLIED, [0x18] = EX_PLAN_IMPLIED, [0x38] = EX_PLAN_IMPLIED,
    [0x58] = EX_PLAN_IMPLIED, [0x78] = EX_PLAN_IMPLIED, [0xB8] = EX_PLAN_IMPLIED, [0xD8] = EX_PLAN_IMPLIED,
    [0xF8] = EX_PLAN_IMPLIED, [0xEA] = EX_PLAN_IMPLIED,
    [0x48] = EX_PLAN_PUSH, [0x08] = EX_PLAN_PUSH, [0x68] = EX_PLAN_PULL, [0x28] = EX_PLAN_PULL,
    [0x4C] = EX_PLAN_JMP, [0x6C] = EX_PLAN_JMPI, [0x20] = EX_PLAN_JSR, [0x60] = EX_PLAN_RTS, [0x40] = EX_PLAN_RTI,
    [0x10] = EX_PLAN_BRANCH, [0x30] = EX_PLAN_BRANCH, [0x50] = EX_PLAN_BRANCH, [0x70] = EX_PLAN_BRANCH,
    [0x90] = EX_PLAN_BRANCH, [0xB0] = EX_PLAN_BRANCH, [0xD0] = EX_PLAN_BRANCH, [0xF0] = EX_PLAN_BRANCH,
};
#undef EX_READ_GROUP
#undef EX_RMW_GROUP

// the instruction's bus cycles in order: the address of each, which ones write, and the dummy accesses the assembly
// core doesn't make (an indexed address's read before the carry, RTS's read of the address it pulled, a
// read-modify-write's first write)
typedef struct {
    uint32_t count;
    uint32_t writes;
    uint32_t dummies;
    uint16_t address[8];
} ex_plan_t;

// reads without side effects: RAM, ROM, plain PRG RAM (the ones cpu_exact.c reads directly, with no mapper answering)
static inline int ex_plain_read(uint16_t address) {
    return (address < 0x2000) || (address >= 0x8000) || (((uint16_t)(address - 0x6000) < 0x2000) && WRAM && !mapper_6000_reads);
}

static inline uint8_t ex_peek(uint16_t address) {
    return NES_WINDOW_BYTE((address < 0x2000) ? (address & (RAM_SIZE - 1)) : address);
}

// the bus pattern of the instruction at PC, as ex_instruction runs it (0: not an instruction this knows, or one
// reading a pointer from a register)
static FRNES_HOT(exact_plan) int ex_plan(ex_plan_t* plan) {
    uint16_t pc = cpu_registers -> PC;
    uint8_t opcode = ex_peek(pc);
    uint8_t mode = ex_plan_mode[opcode];
    uint8_t sp = cpu_registers -> SP;
    uint16_t address = 0, base;
    uint32_t n = 0, writes = 0, dummies = 0;
#define EX_PLAN_R(a) (plan -> address[n++] = (a))
#define EX_PLAN_W(a) (writes |= 1 << n, plan -> address[n++] = (a))
#define EX_PLAN_DUMMY(access) (dummies |= 1 << n, access)
    EX_PLAN_R(pc);
    EX_PLAN_R(pc + 1);
    switch (mode & 0x1f) {
        case EX_PLAN_NONE:
            return 0;
        case EX_PLAN_IMPLIED:
            break;
        case EX_PLAN_ZP:
            address = ex_peek(pc + 1);
            break;
        case EX_PLAN_ZPX:
        case EX_PLAN_ZPY:
            base = ex_peek(pc + 1);
            EX_PLAN_R(base);
            address = (uint8_t)(base + (((mode & 0x1f) == EX_PLAN_ZPX) ? cpu_registers -> X : cpu_registers -> Y));
            break;
        case EX_PLAN_ABS:
        case EX_PLAN_ABSX:
        case EX_PLAN_ABSY:
            EX_PLAN_R(pc + 2);
            base = address = ex_peek(pc + 1) | (ex_peek(pc + 2) << 8);
            if ((mode & 0x1f) != EX_PLAN_ABS) {
                address = base + (((mode & 0x1f) == EX_PLAN_ABSX) ? cpu_registers -> X : cpu_registers -> Y);
                if (((base ^ address) & 0xff00) || ((mode & EX_PLAN_RMW) != EX_PLAN_READ)) {
                    EX_PLAN_DUMMY(EX_PLAN_R((base & 0xff00) | (address & 0x00ff)));
                }
            }
            break;
        case EX_PLAN_INDX: {
            uint8_t pointer = ex_peek(pc + 1);
            EX_PLAN_R(pointer);
            pointer += cpu_registers -> X;
            EX_PLAN_R(pointer);
            EX_PLAN_R((uint8_t)(pointer + 1));
            address = ex_peek(pointer) | (ex_peek((uint8_t)(pointer + 1)) << 8);
        } break;
        case EX_PLAN_INDY: {
            uint8_t pointer = ex_peek(pc + 1);
            EX_PLAN_R(pointer);
            EX_PLAN_R((uint8_t)(pointer + 1));
            base = ex_peek(pointer) | (ex_peek((uint8_t)(pointer + 1)) << 8);
            address = base + cpu_registers -> Y;
            if (((base ^ address) & 0xff00) || ((mode & EX_PLAN_RMW) != EX_PLAN_READ)) {
                EX_PLAN_DUMMY(EX_PLAN_R((base & 0xff00) | (address & 0x00ff)));
            }
        } break;
        case EX_PLAN_BRANCH: {
            // N, V, C, Z by the top two bits, taken when the flag equals bit 5
            static const uint8_t flags[4] = { FLAG_N, FLAG_V, FLAG_C, FLAG_Z };
            int taken = ((cpu_registers -> S & flags[opcode >> 6]) != 0) == ((opcode & 0x20) != 0);
            if (taken) {
                uint16_t next = pc + 2;
                uint16_t target = next + (int8_t)ex_peek(pc + 1);
                EX_PLAN_R(next);
                if ((target ^ next) & 0xff00) {
                    EX_PLAN_R((next & 0xff00) | (target & 0x00ff));
                }
            }
        } break;
        case EX_PLAN_JMP:
            EX_PLAN_R(pc + 2);
            break;
        case EX_PLAN_JMPI: {
            EX_PLAN_R(pc + 2);
            uint16_t pointer = ex_peek(pc + 1) | (ex_peek(pc + 2) << 8);
            EX_PLAN_R(pointer);
            EX_PLAN_R((pointer & 0xff00) | ((pointer + 1) & 0x00ff));
        } break;
        case EX_PLAN_JSR:
            EX_PLAN_R(BASE_STACK + sp);
            EX_PLAN_W(BASE_STACK + sp);
            EX_PLAN_W(BASE_STACK + (uint8_t)(sp - 1));
            EX_PLAN_R(pc + 2);
            break;
        case EX_PLAN_RTS:
            EX_PLAN_R(BASE_STACK + sp);
            EX_PLAN_R(BASE_STACK + (uint8_t)(sp + 1));
            EX_PLAN_R(BASE_STACK + (uint8_t)(sp + 2));
            EX_PLAN_DUMMY(EX_PLAN_R(ex_peek(BASE_STACK + (uint8_t)(sp + 1)) | (ex_peek(BASE_STACK + (uint8_t)(sp + 2)) << 8)));
            break;
        case EX_PLAN_RTI:
            EX_PLAN_R(BASE_STACK + sp);
            EX_PLAN_R(BASE_STACK + (uint8_t)(sp + 1));
            EX_PLAN_R(BASE_STACK + (uint8_t)(sp + 2));
            EX_PLAN_R(BASE_STACK + (uint8_t)(sp + 3));
            break;
        case EX_PLAN_PUSH:
            EX_PLAN_W(BASE_STACK + sp);
            break;
        case EX_PLAN_PULL:
            EX_PLAN_R(BASE_STACK + sp);
            EX_PLAN_R(BASE_STACK + (uint8_t)(sp + 1));
            break;
    }
    switch (mode & EX_PLAN_RMW) {
        case EX_PLAN_READ: EX_PLAN_R(address); break;
        case EX_PLAN_WRITE: EX_PLAN_W(address); break;
        case EX_PLAN_RMW: EX_PLAN_R(address); EX_PLAN_DUMMY(EX_PLAN_W(address)); EX_PLAN_W(address); break;
    }
#undef EX_PLAN_R
#undef EX_PLAN_W
#undef EX_PLAN_DUMMY
    plan -> count = n;
    plan -> writes = writes;
    plan -> dummies = dummies;
    return 1;
}

// (cpu.c, host/asm_stub.c) the assembly core runs this opcode, and fetches code from this PC directly (set_region)
int cpu_asm_runs(uint8_t opcode);
static inline int ex_asm_region(uint16_t pc) {
    return (pc < 0x07fe) || ((pc >= 0x8000) && (pc < 0xfffe)) ||
        (((uint16_t)(pc - 0x6000) < 0x2000) && WRAM && !mapper_6000_reads);
}

int32_t cpu_dma_fast(uint32_t cycle) {
#ifdef FRNES_HOST
    static int enabled = -1;
    if (enabled < 0) {
        enabled = getenv("HOST_DMA_FAST") ? atoi(getenv("HOST_DMA_FAST")) : 1;
    }
    if (!enabled) {
        return -1;
    }
#define EX_FAST_STAT(i) (cpu_dma_fast_stat[i]++)
#else
#define EX_FAST_STAT(i)
#endif
    // only the DMC's predicted request, or its DMA started already (during the last instruction: a register access
    // caught the devices up past the request) and not yet halting the CPU, which halts on this instruction's first
    // read; not OAM DMA, with no interrupt to take
    const uint8_t started = (dmc_unit == DMC_UNIT_HALT) && !cpu_dma_active && !oam_unit && !dmc_unit_stopped;
    if ((!started && (cpu_dma_need_halt || cpu_dma_active || (dmc_unit != DMC_UNIT_OFF) || oam_unit ||
        !apu_dmc_request_due)) || (cpu_registers -> IRQ_Request && !(cpu_registers -> S & FLAG_I))) {
        EX_FAST_STAT(0);
        // (host: which: a DMA started or under way, OAM DMA, no request predicted, an IRQ to take)
        EX_FAST_STAT((cpu_dma_need_halt || cpu_dma_active || (dmc_unit != DMC_UNIT_OFF)) ? 8 : (oam_unit ? 9 :
            (!apu_dmc_request_due ? 10 : 11)));
        return -1;
    }
    // (the region first: the opcode is only peeked where the window maps memory, $2000-$5FFF is a TLB miss)
    uint16_t pc = cpu_registers -> PC;
    if (!ex_asm_region(pc)) {
        EX_FAST_STAT(1);
        return -1;
    }
    uint8_t opcode = ex_peek(pc);
    ex_plan_t plan;
    if (!cpu_asm_runs(opcode) || !ex_plan(&plan) ||
        // (CLI / PLP / RTI with an IRQ waiting: its poll timing is cpu_exact.c's)
        (cpu_registers -> IRQ_Request && ((opcode == 0x58) || (opcode == 0x28) || (opcode == 0x40)))) {
        EX_FAST_STAT(1);
        return -1;
    }
    // the read the DMA halts on: the first at or after the request (started: this instruction's first)
    int32_t first = started ? 0 : (int32_t)(apu_dmc_request_time - cycle);
    uint32_t i = (first > 0) ? (uint32_t)first : 0;
    while ((i < plan.count) && (plan.writes & (1 << i))) {
        i++;
    }
    // every access to RAM / ROM, or (no halt in this instruction) the dummy ones the assembly core leaves out
    uint32_t check = (i >= plan.count) ? plan.dummies : 0xff;
    for (uint32_t k = 0; k < plan.count; k++) {
        uint16_t address = plan.address[k];
        if ((check & (1 << k)) && ((plan.writes & (1 << k)) ? (address >= 0x2000) : !ex_plain_read(address))) {
            EX_FAST_STAT(3);
            return -1;
        }
    }
    if (i >= plan.count) {
        // the halt comes in a later instruction
        EX_FAST_STAT(2);
        return 0;
    }
    cpu_cycle_now = cycle;
    if (!apu_dmc_dma_plain() || cpu_irq_raise_recent ||
        ((int32_t)(cpu_next_event_besides_dmc() - (cycle + plan.count + 4)) <= 1)) {
        EX_FAST_STAT(4);
        return -1;
    }
    // the DMC requesting again before the instruction ends (the buffer this DMA fills is taken at once: its next
    // output cycle starts right after): the exact path halts the instruction again there (Super C, frame 6448)
    if (apu_dmc_rerequest_before(cycle + i + 3, cycle + plan.count + 4)) {
        EX_FAST_STAT(7);
        return -1;
    }
    // the request (cpu_dma_dmc_start) comes up when the devices catch up to a read at or after it, as in ex_read
    // (it can be a cycle after the prediction)
    uint32_t halt;
    for (;;) {
        halt = cycle + i;
        cpu_cycle_now = halt;
        cpu_sync_devices();
        if (dmc_unit != DMC_UNIT_OFF) {
            break;
        }
        do {
            i++;
        } while ((i < plan.count) && (plan.writes & (1 << i)));
        if (i >= plan.count) {
            // no DMA in this instruction after all (a $4015 write's start delay with the sample buffer full predicts
            // a request it doesn't make): the instruction runs as it is (the devices caught up ahead of it see none
            // of its accesses)
            EX_FAST_STAT(6);
            return 0;
        }
    }
    // the DMA's read on its get cycle: the devices are up to date to its end, as in ex_process_dma
    uint32_t stall = CPU_GET_CYCLE(halt) ? 3 : 4;
    cpu_cycle_now = halt + stall;
    cpu_sync_devices();
    dmc_unit = DMC_UNIT_OFF;
    dmc_unit_stopped = 0;
    cpu_dma_need_halt = 0;
    uint8_t value = ex_peek(apu_dmc_dma_address());
    cpu_io_bus_value = cpu_internal_bus_value = value;
    cpu_io_bus_cycle = halt + stall - 1;
    apu_dmc_dma_done(value);
    EX_FAST_STAT(5);
    return stall;
#undef EX_FAST_STAT
}
#endif

#if 1
// save states (state.c): taken and loaded only with no DMA under way, so there's none of its state to keep (the exact
// core's registers and timing are set up afresh by each cpu_exact_run). Read only: written from here as well, the
// flags would have to be reloaded around the hot code's calls
#include "state.h"
// (save states: state_core.c's load sets it here, writing nothing of this file's from outside it)
FRNES_COLD_TEXT void cpu_exact_polled_set(uint8_t polled) {
    cpu_exact_polled = polled;
}

FRNES_COLD_TEXT int cpu_exact_dma_idle(void) {
    return !(cpu_dma_need_halt | cpu_dma_active | dmc_unit | dmc_unit_stopped | oam_unit);
}
#endif
