#ifndef __CPU_H
#define __CPU_H

#include <kos.h>

#include "memory.h"
#include "6502_registers.h"

#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_R 0x20
#define FLAG_V 0x40
#define FLAG_N 0x80

#define BASE_STACK 0x100

#define VECTOR_NMI 0xfffa
#define VECTOR_RESET 0xfffc
#define VECTOR_IRQ 0xfffe

int initialize_cpu();
void shutdown_cpu();
#define CPU_HALTED 0xffffffff

// APU get cycles are the odd CPU cycles (the cycle count is Mesen 2's minus 8), the even ones are put cycles
#define CPU_GET_CYCLE(cycle) ((cycle) & 1)

/**
 * Runs instructions until at least 'budget' cycles have passed, see cpu.c
 */
uint32 cpu_run(uint32 budget);

/**
 * CPU cycles until the next point a batch must stop at (vblank / NMI, end of frame, APU IRQ)
 */
uint32_t cpu_cycles_to_next_event();

/**
 * The next batch's budget for the frame loops: cpu_cycles_to_next_event() counted from cpu_cycle_total. When it comes
 * from a mapper's CPU cycle counter, cpu_run_devices() leaves the PPU and APU behind for the next device event.
 */
uint32_t cpu_batch_budget();

/**
 * Brings the PPU and APU up to the CPU's current time. Call after cpu_run().
 */
void cpu_run_devices();

/**
 * Brings the PPU and APU up to the start of the instruction making a memory mapped I/O access (called from the
 * PPU / APU register paths of read_byte / write_byte)
 */
void cpu_sync_devices();
// cpu_sync_devices for an access to $4000-$5FFF, but none for the FDS's drive and sound registers ($4020 up, no PPU /
// APU state: a disk byte IRQ handler reads $4031 and writes $4024 every 150 cycles while a disk loads); out of the hot
// code, so read_byte / write_byte keep their size
void cpu_sync_devices_io(uint16_t address);

extern uint32_t cpu_cycle_total;
extern uint32_t cpu_cycle_now;
// set by an I/O write that moves a device event earlier (an APU / DMC write): cpu_run() returns after the current
// instruction so the caller brings the devices up to date and sizes the next batch for the new event
extern uint8_t cpu_batch_cut;
// debugging (CPU_TRACE_PC builds): printing the instruction trace
void cpu_trace_dump();
// the CPU cycle the running batch ends at
extern uint32_t cpu_batch_end;
// the batch has to end with the devices caught up (see cpu_run_devices)
extern uint8_t cpu_batch_resync;
// cuts the batch (cpu_batch_cut) when a device event 'cycles' after cpu_cycle_now comes before its end
static inline void cpu_cut_batch_for_event(int32_t cycles) {
    if ((cycles >= 0) && ((uint32_t)cycles < (uint32_t)(cpu_batch_end - cpu_cycle_now))) {
        cpu_batch_cut = 1;
        cpu_batch_resync = 1;
    }
}
// how far the PPU and APU have been brought up to date (cpu_sync_devices)
extern uint32_t cpu_devices_synced_cycle;

/**
 * The IRQ line (IRQ_Request): the APU's frame counter and DMC flags ORed with the cartridge's mapper_irq_flag, one bit
 * per mapper IRQ source. When the line goes up, the cycle it went up during is stamped (cpu_irq_raise_cycle, in
 * cpu_cycle_now's time base): the poll before an instruction's last cycle misses an IRQ raised during its last two
 * cycles, or its last three after a taken branch that stayed on its page, and the CPU takes it after the next
 * instruction instead. Mappers raise and drop their sources with cpu_irq_raise_at / cpu_irq_clear (the cycle from
 * ppu_dot_cpu_cycle for a PPU clocked counter, or the cycle of the CPU cycle counter's step); nothing here runs per
 * instruction beyond the IRQ line test the cores already make.
 */
#define CPU_IRQ_MAPPER 0x01
extern uint32_t cpu_irq_raise_cycle;
extern uint8_t cpu_irq_raise_recent;
void cpu_irq_update_line();
// stamps 'cycle' as the cycle the line went up during, if it isn't up already (call before setting the flag)
static inline void cpu_irq_stamp(uint32_t cycle) {
    if (!cpu_registers -> IRQ_Request) {
        cpu_irq_raise_cycle = cycle;
        cpu_irq_raise_recent = 1;
    }
}
void cpu_irq_raise_at(uint8_t source, uint32_t cycle);
void cpu_irq_clear(uint8_t source);

/**
 * DMA (cpu_exact.c): instructions a DMA can land in run cycle by cycle. The APU requests DMC fetches
 * (cpu_dma_dmc_start at the cycle the request comes up, cpu_dma_dmc_stop when $4015 turns the DMC off), $4014
 * writes start OAM DMA. CPU_EXACT_LEAD: how many cycles before a predicted DMC request the cores hand over.
 */
#define CPU_EXACT_LEAD 8
extern uint8_t cpu_dma_need_halt;
extern uint8_t cpu_dma_active;
extern uint8_t cpu_exact_polled;
#include "apu.h"
void cpu_dma_oam_start(uint8_t page);
void cpu_dma_dmc_start(uint32_t time);
void cpu_dma_dmc_stop();
uint32_t cpu_exact_run(uint32_t cycle, int interrupt);
// the DMC DMA fast path: -1 when the instruction at PC has to run cycle by cycle in cpu_exact_run, otherwise the
// assembly core runs it and it takes this many cycles more (the DMA's stall, made already)
int32_t cpu_dma_fast(uint32_t cycle);
uint32_t cpu_next_event_besides_dmc();
// cpu_exact_run's 'interrupt' for one instruction whose interrupt poll is left to the next boundary (the player's
// fallback for what the assembly core doesn't run, outside DMAs)
#define CPU_EXACT_DEFER 2
// $2000-$5FFF, and $6000-$7FFF without PRG RAM
#define CPU_EXACT_FETCH(pc) (((uint16_t)((pc) - 0x2000) < 0x4000) || (((uint16_t)((pc) - 0x6000) < 0x2000) && !WRAM))
static inline int cpu_exact_due(uint32_t cycle) {
    return cpu_dma_need_halt || cpu_dma_active ||
        (apu_dmc_request_due && ((int32_t)(apu_dmc_request_time - cycle) <= CPU_EXACT_LEAD));
}
// the last value on the CPU data bus from an I/O range read, and its cycle (6502_readbyte.c)
extern uint8_t cpu_io_bus_value;
extern uint32_t cpu_io_bus_cycle;
extern uint8_t cpu_internal_bus_value;

#endif