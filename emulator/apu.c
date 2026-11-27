#include "apu.h"
#include "cpu.h"
#include "mapper.h"
#include "memory.h"

/**
 * CPU side of the APU: frame counter (sequencer steps and IRQ), length counters, and the DMC's timer, sample
 * fetch requests and IRQ. Sound itself is synthesized from the logged register writes (apu_synth.c).
 *
 * Event times: an event at time E happens while the devices are brought up to E (cpu_sync_devices with
 * cpu_cycle_now >= E), so CPU accesses on cycle E and later see it. A DMC sample fetch is a request to the CPU
 * (cpu_dma_dmc_start); the DMA itself runs in cpu_exact.c, which hands the byte back with apu_dmc_dma_done().
 */

// cycles to the DMC's first bit clock after power on
#ifndef APU_DMC_POWER_ON_TIMER
#define APU_DMC_POWER_ON_TIMER 420
#endif
// cycles from a $4015 write to the DMC start / stop it causes, on a put cycle; one more on a get cycle
#ifndef APU_DMC_START_DELAY
#define APU_DMC_START_DELAY 3
#endif
#ifndef APU_DMC_DISABLE_DELAY
#define APU_DMC_DISABLE_DELAY 3
#endif

/*
 * Frame counter (Mesen 2's sequencer): six steps per sequence, NTSC times in CPU cycles from its start. 4-step mode
 * raises the IRQ flag on its last three steps (29828-29830, then the sequence restarts), the half frame clock is on
 * the second and fifth. A $4017 write takes effect 3 cycles after a write on a put cycle, 4 on a get cycle (with the
 * 5-step bit set, it also clocks the half frame units then), and a clock blocks the next cycle's.
 */
static const int32_t apu_frame_steps[2][6] = {
    { 7457, 14913, 22371, 29828, 29829, 29830 },
    { 7457, 14913, 22371, 29829, 37281, 37282 }
};
// 1: quarter frame, 2: half frame (the quarter frame units aren't emulated here, only the length counters)
static const uint8_t apu_frame_step_type[2][6] = { { 1, 2, 1, 0, 2, 0 }, { 1, 2, 1, 0, 2, 0 } };
#define APU_FRAME_RESET_DELAY  3
// cycles from our cycle 0 to the first step (Mesen's power on write of $00 to $4017 took effect 6 cycles before)
#ifndef APU_FRAME_POWER_ON
#define APU_FRAME_POWER_ON 7451
#endif
// cycles from a $4017 write's delay running out to the event that applies it
#ifndef APU_FRAME_WRITE_LEAD
#define APU_FRAME_WRITE_LEAD 0
#endif

#define APU_FRAME_FIVE_STEP    0x80
#define APU_FRAME_IRQ_INHIBIT  0x40

#define APU_STATUS_FRAME_IRQ   0x40
#define APU_STATUS_DMC_IRQ     0x80
#define APU_STATUS_DMC         0x10

int32_t apu_frame_counter_cycles;
uint8_t apu_frame_irq_flag;
// $4015's frame IRQ flag (also set briefly while the IRQ is inhibited), the IRQ line is apu_frame_irq_flag
uint8_t apu_frame_irq_visible;
// a $4017 write waiting to take effect: cycles left (0 = none) and the value
int32_t apu_frame_write_cycles;
uint8_t apu_frame_write_value;
// a clock on this event time blocks clocks up to one cycle later
uint32_t apu_frame_block_time;
uint8_t apu_frame_blocking;
uint8_t apu_frame_counter_mode;
// next step of the sequence, 0-5
uint8_t apu_frame_step;

static const uint8_t apu_length_table[32] = {
    10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};
uint8_t apu_length[4];
uint8_t apu_length_halt[4];
/*
 * Length counter writes ($4003 / $4007 / $400B / $400F reloads, the halt bits) take effect on the APU's next cycle,
 * after a half frame clock on that cycle (which still sees the old halt bit); a reload is dropped when that clock
 * changed the counter (Mesen 2's ApuLengthCounter; Blargg's len_halt_timing / len_reload_timing). Pending until
 * event time apu_length_pending_time.
 */
uint8_t apu_length_pending;
uint32_t apu_length_pending_time;
uint8_t apu_length_reload[4], apu_length_previous[4], apu_length_new_halt[4];
uint8_t apu_channel_enabled;

// (in .hotdata, like cpu.c's cycle_table: its operand cache sets shouldn't move with the size of unrelated code)
static const uint16_t apu_dmc_rates[16] FRNES_HOT_RODATA = {
    428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54
};
uint16_t apu_dmc_rate;
uint8_t apu_dmc_irq_enable;
uint8_t apu_dmc_loop;
uint8_t apu_dmc_irq_flag;
uint16_t apu_dmc_sample_address;
uint16_t apu_dmc_sample_length;
uint16_t apu_dmc_address;
uint16_t apu_dmc_bytes_remaining;
uint8_t apu_dmc_buffer_full;
// output unit: cycles to the next bit clock and bits left in the current output cycle
int32_t apu_dmc_timer;
#ifdef APU_DMC_RATE_MESEN
// (APU_DMC_RATE_MESEN builds: Mesen 2.2.1's order, a $4010 write on the cycle the DMC timer reloads takes effect
// after the reload, for AccuracyCoin runs that stay aligned with Mesen's past its DMC Reload Timing test)
int32_t apu_dmc_rate_deferred;
#endif
uint8_t apu_delays_pending;
uint8_t apu_dmc_bits_remaining;
// a $4015 write enabling the DMC requests the first fetch after 2 (put) / 3 (get) cycles, and disabling it stops
// the DMC 2 / 3 cycles later (cancelling a DMA that hasn't started): cycles left, 0 = none
int32_t apu_dmc_start_delay;
int32_t apu_dmc_disable_delay;

int32_t apu_cycles_to_event;
uint32_t apu_dmc_request_time;
uint8_t apu_dmc_request_due;

static inline void apu_update_irq_line() {
    // the IRQ line is shared with the cartridge
    cpu_registers -> IRQ_Request = apu_frame_irq_flag | apu_dmc_irq_flag | mapper_irq_flag;
}

// (cpu.h, for the cartridge's side of the line)
void cpu_irq_update_line() {
    apu_update_irq_line();
}

static void apu_update_next_event();
// cycles of the apu_run_events() window left after the current step
static uint32_t apu_events_cycles_after;
// The frame IRQ flag is set on 3 consecutive cycles from apu_frame_irq_set_cycle. A $4015 read acknowledges it (the
// IRQ line drops) but the flag still reads as set until the next get cycle: apu_frame_irq_read_until.
static uint32_t apu_frame_irq_set_cycle;
uint32_t apu_frame_irq_read_until;
uint8_t apu_frame_irq_read_pending;

int initialize_apu() {
    printf("apu - initializing\n");
    apu_frame_irq_flag = 0;
    apu_dmc_irq_flag = 0;
    memset(apu_length, 0, sizeof(apu_length));
    memset(apu_length_halt, 0, sizeof(apu_length_halt));
    apu_channel_enabled = 0;
    apu_dmc_rate = apu_dmc_rates[0];
    apu_dmc_irq_enable = apu_dmc_loop = 0;
    apu_dmc_sample_address = 0xc000;
    apu_dmc_sample_length = 1;
    apu_dmc_address = 0xc000;
    apu_dmc_bytes_remaining = 0;
    apu_dmc_buffer_full = 0;
    apu_dmc_timer = APU_DMC_POWER_ON_TIMER;
    apu_dmc_bits_remaining = 8;
    apu_dmc_start_delay = apu_dmc_disable_delay = 0;
    // power on behaves as if $4017 was written with 0 (4-step mode, IRQ enabled) shortly before the first instruction
    apu_frame_counter_mode = 0;
    apu_frame_step = 0;
    apu_frame_counter_cycles = APU_FRAME_POWER_ON;
    apu_frame_write_cycles = 0;
    apu_frame_irq_visible = 0;
    apu_frame_blocking = 0;
    apu_update_next_event();
    printf(" ...done\n");
    return 0;
}

void shutdown_apu() {
    printf("apu - shutting down\n");
    printf(" ...done\n");
}

/**
 * Frame counter
 */
#ifndef APU_LENGTH_DELAY
#define APU_LENGTH_DELAY 1
#endif

static void apu_length_apply() {
    for (uint32_t i = 0; i < 4; i++) {
        if ((apu_length_pending & (1 << i)) && (apu_length[i] == apu_length_previous[i])) {
            apu_length[i] = apu_length_reload[i];
        }
        if (apu_length_pending & (0x10 << i)) {
            apu_length_halt[i] = apu_length_new_halt[i];
        }
    }
    apu_length_pending = 0;
}

// a pending length counter write that has taken effect by 'time'
static inline void apu_length_settle(uint32_t time) {
    if (apu_length_pending && ((int32_t)(time - apu_length_pending_time) >= 0)) {
        apu_length_apply();
    }
}

static void apu_clock_half_frame() {
    for (uint32_t i = 0; i < 4; i++) {
        if (apu_length[i] && !apu_length_halt[i]) {
            apu_length[i]--;
        }
    }
}

static void apu_frame_clock(uint32_t time, uint32_t type) {
    if (apu_frame_blocking && ((int32_t)(time - apu_frame_block_time) <= 1)) {
        return;
    }
    if (type == 2) {
        apu_clock_half_frame();
    }
    apu_frame_blocking = 1;
    apu_frame_block_time = time;
}

static void apu_frame_step_event(uint32_t time) {
    // (writes from before this cycle are in effect; one taking effect on this cycle comes after its clock)
    if (apu_length_pending && ((int32_t)(time - apu_length_pending_time) > 0)) {
        apu_length_apply();
    }
    uint32_t mode = (apu_frame_counter_mode & APU_FRAME_FIVE_STEP) ? 1 : 0;
    uint32_t step = apu_frame_step;
    if (!mode && (step >= 3)) {
        // set on each of the last three steps, which also cancels a $4015 read's clearing
        apu_frame_irq_visible = 1;
        apu_frame_irq_read_pending = 0;
        if (!(apu_frame_counter_mode & APU_FRAME_IRQ_INHIBIT)) {
            if (step == 3) {
                apu_frame_irq_set_cycle = time;
            }
            apu_frame_irq_flag = 1;
            apu_update_irq_line();
        } else if (step == 5) {
            apu_frame_irq_visible = 0;
        }
    }
    if (apu_frame_step_type[mode][step]) {
        apu_frame_clock(time, apu_frame_step_type[mode][step]);
    }
    apu_length_settle(time);
    if (++apu_frame_step == 6) {
        apu_frame_step = 0;
        apu_frame_counter_cycles += apu_frame_steps[mode][0];
    } else {
        apu_frame_counter_cycles += apu_frame_steps[mode][apu_frame_step] - apu_frame_steps[mode][step];
    }
}

// the $4017 write's delay ran out: the sequence restarts in the new mode
static void apu_frame_write_event(uint32_t time) {
    apu_frame_counter_mode = apu_frame_write_value & (APU_FRAME_FIVE_STEP | APU_FRAME_IRQ_INHIBIT);
    apu_frame_step = 0;
    apu_frame_counter_cycles = apu_frame_steps[(apu_frame_counter_mode & APU_FRAME_FIVE_STEP) ? 1 : 0][0];
    if (apu_length_pending && ((int32_t)(time - apu_length_pending_time) > 0)) {
        apu_length_apply();
    }
    if (apu_frame_counter_mode & APU_FRAME_FIVE_STEP) {
        apu_frame_clock(time, 2);
    }
    apu_length_settle(time);
}

void apu_write_frame_counter(uint8_t value) {
    // the IRQ inhibit bit takes effect right away
    if (value & APU_FRAME_IRQ_INHIBIT) {
        apu_frame_irq_flag = 0;
        apu_frame_irq_visible = 0;
        apu_frame_irq_read_pending = 0;
        apu_update_irq_line();
    }
    apu_frame_counter_mode = (apu_frame_counter_mode & APU_FRAME_FIVE_STEP) | (value & APU_FRAME_IRQ_INHIBIT);
    apu_frame_write_value = value;
    apu_frame_write_cycles = APU_FRAME_RESET_DELAY + CPU_GET_CYCLE(cpu_cycle_now) + APU_FRAME_WRITE_LEAD;
    apu_update_next_event();
    cpu_cut_batch_for_event(apu_cycles_to_event);
}

/**
 * Length counters
 */
void apu_write_register(uint16_t address, uint8_t value) {
    apu_length_settle(cpu_cycle_now);
    switch (address) {
        case 0x4000: case 0x4004: case 0x4008: case 0x400c: {
            uint32_t channel = (address >> 2) & 3;
            apu_length_new_halt[channel] = value & ((channel == 2) ? 0x80 : 0x20);
            apu_length_pending |= 0x10 << channel;
            apu_length_pending_time = cpu_cycle_now + APU_LENGTH_DELAY;
        } break;
        case 0x4003: case 0x4007: case 0x400b: case 0x400f: {
            uint32_t channel = (address >> 2) & 3;
            if (apu_channel_enabled & (1 << channel)) {
                apu_length_reload[channel] = apu_length_table[value >> 3];
                apu_length_previous[channel] = apu_length[channel];
                apu_length_pending |= 1 << channel;
                apu_length_pending_time = cpu_cycle_now + APU_LENGTH_DELAY;
            }
        } break;
        case 0x4010:
            apu_dmc_irq_enable = value & 0x80;
            apu_dmc_loop = value & 0x40;
#ifdef APU_DMC_RATE_MESEN
            if (apu_dmc_timer <= APU_DMC_RATE_MESEN) {
                apu_dmc_rate_deferred = apu_dmc_rates[value & 0x0f];
            } else
#endif
            apu_dmc_rate = apu_dmc_rates[value & 0x0f];
            if (!apu_dmc_irq_enable) {
                apu_dmc_irq_flag = 0;
                apu_update_irq_line();
            }
            break;
        case 0x4012: apu_dmc_sample_address = 0xc000 | ((uint16_t)value << 6); break;
        case 0x4013: apu_dmc_sample_length = ((uint16_t)value << 4) + 1; break;
        case 0x4015:
            apu_channel_enabled = value & 0x0f;
            for (uint32_t i = 0; i < 4; i++) {
                if (!(value & (1 << i))) {
                    apu_length[i] = 0;
                }
            }
            apu_dmc_irq_flag = 0;
            if (!(value & 0x10)) {
                // takes effect a few cycles later, see apu_dmc_events
                if (!apu_dmc_disable_delay) {
                    apu_dmc_disable_delay = APU_DMC_DISABLE_DELAY + CPU_GET_CYCLE(cpu_cycle_now);
                }
            } else if (!apu_dmc_bytes_remaining) {
                apu_dmc_address = apu_dmc_sample_address;
                apu_dmc_bytes_remaining = apu_dmc_sample_length;
                apu_dmc_start_delay = APU_DMC_START_DELAY + CPU_GET_CYCLE(cpu_cycle_now);
            }
            apu_update_irq_line();
            break;
    }
    apu_update_next_event();
    // the DMC writes can bring its next fetch / IRQ before the end of the running batch, which was sized for the
    // old events: end the batch after this instruction
    if ((address >= 0x4010) && (address != 0x4011)) {
        cpu_cut_batch_for_event(apu_cycles_to_event);
    }
}

FRNES_COLD uint8_t apu_read_status(uint8_t open_bus) {
    apu_length_settle(cpu_cycle_now);
    uint32_t frame_irq = apu_frame_irq_visible;
    if (apu_frame_irq_read_pending && ((int32_t)(apu_frame_irq_read_until - cpu_cycle_now) <= 0)) {
        apu_frame_irq_read_pending = 0;
        apu_frame_irq_visible = frame_irq = 0;
    }
    uint8_t result = (open_bus & 0x20) | (frame_irq ? APU_STATUS_FRAME_IRQ : 0) | (apu_dmc_irq_flag ? APU_STATUS_DMC_IRQ : 0) |
        (apu_dmc_bytes_remaining ? APU_STATUS_DMC : 0);
    for (uint32_t i = 0; i < 4; i++) {
        if (apu_length[i]) {
            result |= 1 << i;
        }
    }
    // reading acknowledges the frame IRQ (not the DMC's): the IRQ line drops now, the flag when the APU goes from a
    // put to a get cycle (reads until then still see it; a step setting it again in between cancels that)
    if (apu_frame_irq_visible && !apu_frame_irq_read_pending) {
        apu_frame_irq_read_until = cpu_cycle_now + 1 + CPU_GET_CYCLE(cpu_cycle_now);
        apu_frame_irq_read_pending = 1;
    }
    apu_frame_irq_flag = 0;
    apu_update_irq_line();
    return result;
}

/**
 * DMC: the output unit clocks one bit every 'rate' cycles, and starting each 8 bit output cycle empties the
 * sample buffer, which makes the reader request the next byte (a DMA that halts the CPU) while bytes remain.
 */
static void apu_dmc_start_transfer(uint32_t time) {
    if (!apu_dmc_buffer_full && apu_dmc_bytes_remaining) {
        cpu_dma_dmc_start(time);
    }
}

uint16_t apu_dmc_dma_address() {
    return apu_dmc_address;
}

// the CPU's DMA read the sample byte (on cycle cpu_cycle_now - 1, the devices are up to date to its end)
void apu_dmc_dma_done(uint8_t value) {
#ifdef FRNES_HOST
    // (host builds: HOST_DMA=1 logs the DMC's DMAs, see tools/hunt.py)
    static int log = -1;
    if (log < 0) {
        log = getenv("HOST_DMA") != NULL;
    }
    if (log) {
        printf("DMCDONE now=%u v=%02X\n", (unsigned)cpu_cycle_now, value);
    }
#endif
    (void)value;
    if (apu_dmc_bytes_remaining) {
        apu_dmc_buffer_full = 1;
        apu_dmc_address = (apu_dmc_address == 0xffff) ? 0x8000 : apu_dmc_address + 1;
        if (--apu_dmc_bytes_remaining == 0) {
            if (apu_dmc_loop) {
                apu_dmc_address = apu_dmc_sample_address;
                apu_dmc_bytes_remaining = apu_dmc_sample_length;
            } else if (apu_dmc_irq_enable) {
                cpu_irq_stamp(cpu_cycle_now);
                apu_dmc_irq_flag = 1;
                apu_update_irq_line();
            }
        }
    }
    // a 1 byte sample whose DMA ends on the APU cycle before the bit counter reloads: the reload requests another
    // DMA, which the DMC aborts a cycle later (one halted CPU cycle), and the sample restarts
    if ((apu_dmc_sample_length == 1) && !apu_dmc_loop && (apu_dmc_bits_remaining == 1) && (apu_dmc_timer <= 2)) {
        apu_dmc_buffer_full = 1;
        apu_dmc_address = apu_dmc_sample_address;
        apu_dmc_bytes_remaining = apu_dmc_sample_length;
        apu_dmc_disable_delay = 3;
    }
    apu_update_next_event();
}

/*
 * For cpu_exact.c's DMC fast path: whether the next DMA's byte can be handed over without the cycle by cycle path
 * (no DMC IRQ, sample restart or $4015 delay comes with it), and the cycles from the last catch-up to the APU's next
 * event other than the DMC's request (a frame counter step, a $4017 write taking effect)
 */
int apu_dmc_dma_plain() {
    return (apu_dmc_bytes_remaining > 1 || apu_dmc_loop || !apu_dmc_irq_enable) && (apu_dmc_sample_length != 1) &&
        !apu_dmc_disable_delay;
}

/*
 * For cpu_exact.c's DMC fast path: whether, after a DMA ending at 'done' or later, the DMC requests the next one before
 * 'end'. The buffer the DMA fills is taken at the next output cycle's start (the bit counter's reload), which requests
 * it: from the DMC timer as of the last catch-up, the bit clocks up to 'done' run with the buffer empty, then the
 * request comes 'bits remaining' clocks later. A rate change waiting ($4010 written, APU_DMC_RATE_MESEN) counts as one
 */
FRNES_COLD_TEXT int apu_dmc_rerequest_before(uint32_t done, uint32_t end) {
    if (!((apu_dmc_bytes_remaining > 1) || apu_dmc_loop)) {
        return 0;
    }
#ifdef APU_DMC_RATE_MESEN
    if (apu_dmc_rate_deferred) {
        return 1;
    }
#endif
    uint32_t time = cpu_devices_synced_cycle;
    int32_t to_clock = apu_dmc_timer;
    uint32_t bits = apu_dmc_bits_remaining;
    while ((int32_t)(time + to_clock - done) <= 0) {
        time += to_clock;
        to_clock = apu_dmc_rate;
        if (--bits == 0) {
            bits = 8;
        }
    }
    uint32_t request = time + to_clock + (bits - 1) * apu_dmc_rate;
    return (int32_t)(request - end) < 0;
}

int32_t apu_cycles_to_event_besides_dmc() {
    int32_t cycles = apu_frame_counter_cycles;
    if (apu_frame_write_cycles && (apu_frame_write_cycles < cycles)) {
        cycles = apu_frame_write_cycles;
    }
    return cycles;
}

static void apu_dmc_bit_clock(uint32_t time) {
    if (--apu_dmc_bits_remaining == 0) {
        apu_dmc_bits_remaining = 8;
        // a new output cycle takes the sample buffer (not while a $4015 write's start delay runs, which requests
        // the DMA itself)
        if (apu_dmc_buffer_full) {
            apu_dmc_buffer_full = 0;
            if (!apu_dmc_start_delay) {
                apu_dmc_start_transfer(time);
            }
        }
    }
}

// cycles until the DMC requests its next DMA (0x7fffffff: none predicted)
static int32_t apu_dmc_cycles_to_request() {
    if (apu_dmc_start_delay) {
        return apu_dmc_start_delay;
    }
    if (apu_dmc_buffer_full && apu_dmc_bytes_remaining) {
        return apu_dmc_timer + (apu_dmc_bits_remaining - 1) * apu_dmc_rate;
    }
    return 0x7fffffff;
}

static void apu_update_next_event() {
    apu_delays_pending = (apu_dmc_start_delay || apu_dmc_disable_delay || apu_frame_write_cycles) ? 1 : 0;
    int32_t request = apu_dmc_cycles_to_request();
    apu_dmc_request_due = request != 0x7fffffff;
    apu_dmc_request_time = cpu_cycle_now + request;
    // the batch ends a few cycles before the request: the instructions the DMA lands in run in cpu_exact.c
    int32_t cycles = request - CPU_EXACT_LEAD;
    if (cycles < 1) {
        cycles = 1;
    }
    if (apu_frame_counter_cycles < cycles) {
        cycles = apu_frame_counter_cycles;
    }
    if (apu_frame_write_cycles && (apu_frame_write_cycles < cycles)) {
        cycles = apu_frame_write_cycles;
    }
    if (apu_dmc_disable_delay && (apu_dmc_disable_delay < cycles)) {
        cycles = apu_dmc_disable_delay;
    }
    apu_cycles_to_event = cycles;
}


/*
 * The DMC's bit clocks with its sample buffer empty: each only counts its bits down (apu_dmc_bit_clock takes no buffer)
 * and reloads the timer. Those coming before 'cycles' and before every other event (the frame counter, a $4017 write
 * taking effect, the $4015 delays: none of them reaching 0 meanwhile) are all taken here at once, as the loop below
 * would one at a time: the counters as it leaves them. Returns the cycles taken (0 for none). In a game whose DMC is
 * idle they are most of the APU's events (Lagrange Point: ~600 a frame)
 */
static inline uint32_t apu_dmc_idle_clocks(uint32_t cycles) {
    if (apu_dmc_buffer_full || apu_dmc_start_delay || ((uint32_t)apu_dmc_timer > cycles)
#ifdef APU_DMC_RATE_MESEN
        || apu_dmc_rate_deferred
#endif
        ) {
        return 0;
    }
    // (strictly before the other events: they come in the loop's order after a clock on their cycle)
    uint32_t limit = cycles + 1;
    if ((uint32_t)apu_frame_counter_cycles < limit) {
        limit = apu_frame_counter_cycles;
    }
    if (apu_dmc_disable_delay && ((uint32_t)apu_dmc_disable_delay < limit)) {
        limit = apu_dmc_disable_delay;
    }
    if (apu_frame_write_cycles && ((uint32_t)apu_frame_write_cycles < limit)) {
        limit = apu_frame_write_cycles;
    }
    // clocks k with timer + (k - 1) * rate < limit
    if ((uint32_t)apu_dmc_timer >= limit) {
        return 0;
    }
    uint32_t clocks = 1 + (limit - 1 - (uint32_t)apu_dmc_timer) / (uint32_t)apu_dmc_rate;
    uint32_t taken = (uint32_t)apu_dmc_timer + (clocks - 1) * (uint32_t)apu_dmc_rate;
    apu_frame_counter_cycles -= taken;
    if (apu_dmc_disable_delay) {
        apu_dmc_disable_delay -= taken;
    }
    if (apu_frame_write_cycles) {
        apu_frame_write_cycles -= taken;
    }
    apu_dmc_timer = apu_dmc_rate;
    apu_dmc_bits_remaining = ((apu_dmc_bits_remaining - 1 + 8 - (clocks & 7)) & 7) + 1;
    return taken;
}

__attribute__((noinline)) void apu_run_events(uint32 cpu_cycles) {
    while (cpu_cycles) {
        uint32_t idle = apu_dmc_idle_clocks(cpu_cycles);
        if (idle) {
            cpu_cycles -= idle;
            apu_events_cycles_after = cpu_cycles;
            continue;
        }
        uint32_t irq_before = apu_frame_irq_flag | apu_dmc_irq_flag;
        // step to the next thing that happens: a frame counter step, a DMC bit clock or a $4015 write's delay
        uint32_t step = cpu_cycles;
        if ((uint32_t)apu_frame_counter_cycles < step) {
            step = apu_frame_counter_cycles;
        }
        if ((uint32_t)apu_dmc_timer < step) {
            step = apu_dmc_timer;
        }
        if (apu_dmc_start_delay && ((uint32_t)apu_dmc_start_delay < step)) {
            step = apu_dmc_start_delay;
        }
        if (apu_dmc_disable_delay && ((uint32_t)apu_dmc_disable_delay < step)) {
            step = apu_dmc_disable_delay;
        }
        if (apu_frame_write_cycles && ((uint32_t)apu_frame_write_cycles < step)) {
            step = apu_frame_write_cycles;
        }
        if (step == 0) {
            step = 1;
        }
        cpu_cycles -= step;
        apu_events_cycles_after = cpu_cycles;
        uint32_t time = cpu_cycle_now - cpu_cycles;
        apu_frame_counter_cycles -= step;
        apu_dmc_timer -= step;
        // the $4015 delays first, then the frame counter and the DMC timer
        if (apu_dmc_disable_delay && ((apu_dmc_disable_delay -= step) <= 0)) {
            apu_dmc_disable_delay = 0;
            apu_dmc_bytes_remaining = 0;
            cpu_dma_dmc_stop();
        }
        if (apu_dmc_start_delay && ((apu_dmc_start_delay -= step) <= 0)) {
            apu_dmc_start_delay = 0;
            apu_dmc_start_transfer(time);
        }
        if (apu_dmc_timer <= 0) {
            apu_dmc_timer += apu_dmc_rate;
#ifdef APU_DMC_RATE_MESEN
            if (apu_dmc_rate_deferred) {
                apu_dmc_rate = apu_dmc_rate_deferred;
                apu_dmc_rate_deferred = 0;
            }
#endif
            apu_dmc_bit_clock(time);
        }
        // (a step on the same cycle as a $4017 write taking effect comes first)
        if (apu_frame_counter_cycles <= 0) {
            apu_frame_step_event(time);
        }
        if (apu_frame_write_cycles && ((apu_frame_write_cycles -= step) <= 0)) {
            apu_frame_write_cycles = 0;
            apu_frame_write_event(time);
        }
        // (the line can already be up for the cartridge)
        if ((apu_frame_irq_flag | apu_dmc_irq_flag) && !irq_before && !mapper_irq_flag) {
            cpu_irq_raise_cycle = cpu_cycle_now - cpu_cycles;
            cpu_irq_raise_recent = 1;
        }
    }
    apu_update_next_event();
}

// (save states: state_core.c, out of the link time optimized code, has this file's chunk)
