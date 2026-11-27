#ifndef __APU_H
#define __APU_H

#include <kos.h>

#include "memory.h"
#include "6502_registers.h"

/**
 * CPU side of the APU: frame counter sequencer (quarter / half frame clocks, IRQ), length counters ($4015
 * status bits 0-3), and the DMC's timer, sample fetches and IRQ. Sound generation is done separately from the
 * logged register writes (apu_synth.c).
 *
 * In 4-step mode the frame counter raises its IRQ flag 29829 CPU cycles into each 29830 cycle sequence
 * unless the IRQ is inhibited ($4017 bit 6). 5-step mode never raises it. The flag drives the CPU IRQ
 * line until it is acknowledged by reading $4015 or inhibited by a $4017 write.
 *
 * Each DMC sample fetch is a DMA request to the CPU (cpu_dma_dmc_start); cpu_exact.c makes the read and hands the
 * byte back with apu_dmc_dma_done(). apu_dmc_request_time is when the next request comes up (if apu_dmc_request_due).
 */
extern int32_t apu_frame_counter_cycles;
extern uint8_t apu_frame_irq_flag;
extern uint32_t apu_dmc_request_time;
extern uint8_t apu_dmc_request_due;
uint16_t apu_dmc_dma_address();
void apu_dmc_dma_done(uint8_t value);
// (cpu_exact.c's DMC fast path) the next DMA's byte needs nothing cycle by cycle, and the APU's next other event
int apu_dmc_dma_plain();
// (cpu_exact.c's DMC fast path) whether the DMC requests again between a DMA ending at 'done' and 'end'
int apu_dmc_rerequest_before(uint32_t done, uint32_t end);
int32_t apu_cycles_to_event_besides_dmc();
// cycles until the next frame counter step or DMC fetch, for scheduling CPU batches
extern int32_t apu_cycles_to_event;

int initialize_apu();
void shutdown_apu();

void apu_write_frame_counter(uint8_t value);
// $4000-$4013 and $4015 writes ($4017 goes to apu_write_frame_counter)
void apu_write_register(uint16_t address, uint8_t value);
uint8_t apu_read_status(uint8_t open_bus);
void apu_run_events(uint32 cpu_cycles);
// the DMC output unit's cycles to its next bit clock, and whether a $4015 / $4017 write's delay is running
extern int32_t apu_dmc_timer;
extern uint8_t apu_delays_pending;

/**
 * Advances the APU by a number of CPU cycles. Most catch-ups have nothing happen in them (no frame counter step, DMC
 * bit clock or write taking effect): those only count down here, the rest go through apu_run_events.
 */
static inline void apu_run(uint32 cpu_cycles) {
    if (!apu_delays_pending && ((int32_t)cpu_cycles < apu_frame_counter_cycles) && ((int32_t)cpu_cycles < apu_dmc_timer)) {
        apu_frame_counter_cycles -= cpu_cycles;
        apu_dmc_timer -= cpu_cycles;
        apu_cycles_to_event = (apu_cycles_to_event > (int32_t)cpu_cycles + 1) ? apu_cycles_to_event - (int32_t)cpu_cycles : 1;
        return;
    }
    apu_run_events(cpu_cycles);
}

#endif
