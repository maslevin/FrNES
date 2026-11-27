/*
 * state_core.c: the CPU's, the 2A03's and the memory's save state chunks, built without link time optimization (as
 * mapper_boards.c): the serializers' code in the link time optimized units moved
 * the hot paths' inlining and scheduling (ppu_rendering_dot, write_byte_6502_c). Their variables are globals for this
 * (cpu.c's four statics, apu.c's); nothing of cpu_exact.c's is written from here (cpu_exact_polled_set). The PPU's
 * chunk stays in ppu.c, compact (its statics written from outside it changed its hot code), and the synthesizer's in
 * apu_synth.c (its channel structs; it alone moved nothing). state.c and state_hotkeys.c stay link time optimized:
 * built without it, their references made globals external and moved the release's write_byte_6502_c
 */
#include <stdint.h>
#include <stdio.h>
#include "state.h"
#include "memory.h"
#include "mapper.h"

// cpu.c's chunk: the CPU's time and interrupt state
extern uint32_t cpu_cycle_total, cpu_cycle_now, cpu_batch_end, cpu_instruction_end, cpu_devices_synced_cycle,
    cpu_irq_raise_cycle, cpu_nmi_raise_cycle, cpu_nmi_raise_dots, cpu_batch_device_event;
extern uint8_t cpu_batch_cut, cpu_poll_early, cpu_irq_raise_recent, cpu_batch_mapper_only, cpu_batch_resync,
    cpu_exact_polled, mapper_irq_flag;
// (POLLED: cpu_exact_polled when saving; a copy when loading, set through cpu_exact_polled_set: nothing of cpu_exact.c's
// is written from here)
#define CPU_STATE_LIST(X, A, POLLED) X(cpu_cycle_total) X(cpu_cycle_now) X(cpu_batch_cut) X(cpu_batch_end) X(cpu_poll_early) \
    X(cpu_instruction_end) X(cpu_devices_synced_cycle) X(cpu_irq_raise_cycle) X(cpu_irq_raise_recent) \
    X(cpu_nmi_raise_cycle) X(cpu_nmi_raise_dots) X(cpu_batch_mapper_only) X(cpu_batch_device_event) \
    X(cpu_batch_resync) X(POLLED) X(mapper_irq_flag)
#define CPU_STATE(X, A) CPU_STATE_LIST(X, A, cpu_exact_polled)
#define CPU_STATE_LOADED(X, A) CPU_STATE_LIST(X, A, polled)

FRNES_COLD_TEXT void cpu_state_save(state_writer_t* writer) {
    STATE_SAVE(CPU_STATE)
}

FRNES_COLD_TEXT int cpu_state_load(state_reader_t* reader, uint16_t version) {
    if ((version != CPU_STATE_VERSION) || (reader -> size != STATE_SIZE(CPU_STATE))) {
        return -1;
    }
    uint8_t polled;
    STATE_LOAD(CPU_STATE_LOADED)
    cpu_exact_polled_set(polled);
    return 0;
}

// apu.c's chunk
#include "apu.h"
extern int32_t apu_dmc_rate_deferred;
extern int32_t apu_frame_counter_cycles;
extern uint8_t apu_frame_irq_flag;
extern uint8_t apu_frame_irq_visible;
extern int32_t apu_frame_write_cycles;
extern uint8_t apu_frame_write_value;
extern uint32_t apu_frame_block_time;
extern uint8_t apu_frame_blocking;
extern uint8_t apu_frame_counter_mode;
extern uint8_t apu_frame_step;
extern uint8_t apu_length[4];
extern uint8_t apu_length_halt[4];
extern uint8_t apu_length_pending;
extern uint32_t apu_length_pending_time;
extern uint8_t apu_length_reload[4];
extern uint8_t apu_length_previous[4];
extern uint8_t apu_length_new_halt[4];
extern uint8_t apu_channel_enabled;
extern uint16_t apu_dmc_rate;
extern uint8_t apu_dmc_irq_enable;
extern uint8_t apu_dmc_loop;
extern uint8_t apu_dmc_irq_flag;
extern uint16_t apu_dmc_sample_address;
extern uint16_t apu_dmc_sample_length;
extern uint16_t apu_dmc_address;
extern uint16_t apu_dmc_bytes_remaining;
extern uint8_t apu_dmc_buffer_full;
extern int32_t apu_dmc_timer;
extern uint8_t apu_delays_pending;
extern uint8_t apu_dmc_bits_remaining;
extern int32_t apu_dmc_start_delay;
extern int32_t apu_dmc_disable_delay;
extern int32_t apu_cycles_to_event;
extern uint32_t apu_dmc_request_time;
extern uint8_t apu_dmc_request_due;
extern uint32_t apu_frame_irq_read_until;
extern uint8_t apu_frame_irq_read_pending;
// save states (state.c): the 2A03's frame counter, length counters and DMC (the sound itself is apu_synth.c's chunk)
// (not apu_events_cycles_after or apu_frame_irq_set_cycle: nothing reads them, and reading them here would keep them,
// and their stores in apu_run_events, alive)
#ifdef APU_DMC_RATE_MESEN
#define APU_STATE_MESEN(X, A) X(apu_dmc_rate_deferred)
#else
#define APU_STATE_MESEN(X, A)
#endif
#define APU_STATE(X, A) X(apu_frame_counter_cycles) X(apu_frame_irq_flag) X(apu_frame_irq_visible) \
    X(apu_frame_write_cycles) X(apu_frame_write_value) X(apu_frame_block_time) X(apu_frame_blocking) \
    X(apu_frame_counter_mode) X(apu_frame_step) A(apu_length) A(apu_length_halt) X(apu_length_pending) \
    X(apu_length_pending_time) A(apu_length_reload) A(apu_length_previous) A(apu_length_new_halt) \
    X(apu_channel_enabled) X(apu_dmc_rate) X(apu_dmc_irq_enable) X(apu_dmc_loop) X(apu_dmc_irq_flag) \
    X(apu_dmc_sample_address) X(apu_dmc_sample_length) X(apu_dmc_address) X(apu_dmc_bytes_remaining) \
    X(apu_dmc_buffer_full) X(apu_dmc_timer) X(apu_delays_pending) X(apu_dmc_bits_remaining) X(apu_dmc_start_delay) \
    X(apu_dmc_disable_delay) X(apu_cycles_to_event) X(apu_dmc_request_time) X(apu_dmc_request_due) \
    X(apu_frame_irq_read_until) X(apu_frame_irq_read_pending) \
    APU_STATE_MESEN(X, A)

FRNES_COLD_TEXT void apu_state_save(state_writer_t* writer) {
    STATE_SAVE(APU_STATE)
}

FRNES_COLD_TEXT int apu_state_load(state_reader_t* reader, uint16_t version) {
    if ((version != APU_STATE_VERSION) || (reader -> size != STATE_SIZE(APU_STATE))) {
        return -1;
    }
    STATE_LOAD(APU_STATE)
    return 0;
}

// memory.c's chunk (its buffers are globals: memory.h, cartridge.h)
#include "cartridge.h"
// save states (state.c): the 2 kB RAM, the register page (the CPU's and PPU's registers), nametable VRAM, OAM and the
// palette, PRG RAM and CHR RAM, each through its buffer (the pages mapped onto them are the board's, BORD)

FRNES_COLD_TEXT void memory_state_save(state_writer_t* writer) {
    uint32_t wram_size = WRAM ? WRAM_size : 0;
    uint32_t chr_ram_size = cartridge_VRAM ? num_VRAM_bytes_allocated : 0;
    STATE_PUT_VAR(writer, wram_size);
    STATE_PUT_VAR(writer, chr_ram_size);
    state_put(writer, RAM, 0x800);
    state_put(writer, register_buffer, 0x400);
    state_put(writer, VRAM, 0x3000);
    state_put(writer, ppu_memory_buffer, 0x400);
    state_put(writer, WRAM, wram_size);
    state_put(writer, cartridge_VRAM, chr_ram_size);
}

FRNES_COLD_TEXT int memory_state_load(state_reader_t* reader, uint16_t version) {
    uint32_t wram_size, chr_ram_size;
    if ((version != MEMORY_STATE_VERSION) || (STATE_GET_VAR(reader, wram_size) != 0) ||
        (STATE_GET_VAR(reader, chr_ram_size) != 0) || (wram_size != (WRAM ? WRAM_size : 0)) ||
        (chr_ram_size != (cartridge_VRAM ? num_VRAM_bytes_allocated : 0)) ||
        (reader -> size - reader -> at != 0x800 + 0x400 + 0x3000 + 0x400 + wram_size + chr_ram_size)) {
        return -1;
    }
    // (the DIP switches are this run's setting; the Vs. System's set up is the game's: vs_state_loaded)
    uint8_t vs_dips = cpu_registers -> VS_Dips;
    state_get(reader, RAM, 0x800);
    state_get(reader, register_buffer, 0x400);
    state_get(reader, VRAM, 0x3000);
    state_get(reader, ppu_memory_buffer, 0x400);
    state_get(reader, WRAM, wram_size);
    state_get(reader, cartridge_VRAM, chr_ram_size);
    vs_state_loaded(vs_dips);
    // (the battery save is written again: PRG RAM is what the state had)
    prg_ram_written = 1;
    ppu_nametable_generation++;
    return 0;
}

// the test builds' replays: a save / load state event on the recording's run starting at this frame (the reader's
// current run, input_recording_frame having moved it), done by test_main.c at the frame's end
#include "input_recording.h"
FRNES_COLD_TEXT uint8_t recording_state_action(const input_recording_t* recording, uint32_t frame) {
    if ((recording -> run_index >= recording -> header.num_runs) || (recording -> run_start_frame != frame)) {
        return 0;
    }
    uint16_t event = recording -> runs[recording -> run_index].event;
    return (event & INPUT_RECORDING_EVENT_STATE_SAVE) ? STATE_HOTKEY_SAVE : ((event & INPUT_RECORDING_EVENT_STATE_LOAD) ? STATE_HOTKEY_LOAD : 0);
}
