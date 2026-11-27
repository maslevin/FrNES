#include <stddef.h>
#include "6502_registers.h"

#include "cpu.h"
#include "memory.h"
#include "ppu.h"
#include "apu.h"
#include "mapper.h"
#define PPU_MAPPER_RENDERING() (ppu_registers -> PPUMASK & (PPUMASK_ENABLE_BACKGROUND_RENDERING | PPUMASK_ENABLE_SPRITE_RENDERING))

// Macros to read from 6502 Memory directly, if we know it will never hit a range which requires registers
// otherwise use read_byte / read_word functions, which take register ranges into account
#define READ_BYTE(w) NES_WINDOW_BYTE(w)
#define READ_WORD(w) (NES_WINDOW_BYTE(w) | (NES_WINDOW_BYTE((w) + 1) << 8))

// Memory mapped I/O from inside cpu_run: record the cycle the instruction started at so the PPU / APU can be
// brought up to it, and account for OAM DMA stalls after writes
#define IO_SYNC() cpu_cycle_now = cpu_cycle_total + cycles_done
// (I/O reads give ppu_poll_skip the PC after the instruction, on the host; a poll loop's iterations it skips are taken
// when the instruction ends, READ_SKIP)
_Static_assert(offsetof(_6502_registers_t, READ_SKIP) == 28 && offsetof(_6502_registers_t, IO_PC) == 30 && sizeof(_6502_registers_t) <= 32,
    "6502_core.s REG_READ_SKIP / REG_IO_PC, and the PPU registers at +32");
#ifdef FRNES_HOST
#define IO_READ_PC() (cpu_registers -> IO_PC = r_pc)
#else
// (the C core on the console leaves the skip to the assembly core: a PC stored at each of its read sites would take
// 3.6 kB of its hot slot; IO_PC 0 never skips)
#define IO_READ_PC() 0
#endif
#define IO_READ(a) (IO_SYNC(), IO_READ_PC(), read_byte(a))
#define IO_READ_WORD(a) (IO_SYNC(), read_word(a))
#define IO_WRITE(a, v) do { IO_SYNC(); write_byte(a, v); if (cpu_registers -> DMA_Cycles) { cycles += cpu_registers -> DMA_Cycles; cpu_registers -> DMA_Cycles = 0; } } while (0)
// The same, for an access on cycle 'offset' of the instruction (0 = the opcode fetch). The data access of a read
// or write instruction is its last cycle, so the PPU / APU are brought up to that cycle, not the instruction start.
#define IO_SYNC_AT(offset) cpu_cycle_now = cpu_cycle_total + cycles_done + (offset)
#define IO_READ_AT(a, offset) (IO_SYNC_AT(offset), IO_READ_PC(), read_byte(a))
#define IO_WRITE_AT(a, v, offset) do { IO_SYNC_AT(offset); write_byte(a, v); if (cpu_registers -> DMA_Cycles) { cycles += cpu_registers -> DMA_Cycles; cpu_registers -> DMA_Cycles = 0; } } while (0)

// Data accesses (everything except zero page, the stack and opcode fetches) go through read_byte / write_byte.
// Inlining a RAM / ROM fast path here was measured slower on SMB (82.6 vs 85.8 fps): the extra code in every
// handler costs more in instruction cache misses than the calls it saves.
#define MEM_READ(a) IO_READ_AT(a, cycles - 1)
#define MEM_WRITE(a, v) IO_WRITE_AT(a, v, cycles - 1)

// Zero page is always internal RAM, so it never needs to go through read_byte / write_byte
#define ZP_READ(a) NES_WINDOW_BYTE((uint8_t)(a))
#define ZP_WRITE(a, b) NES_WINDOW_BYTE((uint8_t)(a)) = (b)

// Operand fetch from the program counter. RAM and ROM are read directly, but programs can also execute
// from registers / open bus ($2000-$7FFF), which isn't mapped and has to go through read_byte
#define IS_IO_FETCH(pc) ((uint16_t)((pc) - 0x2000) < 0x6000)
// Checked once per instruction at the opcode: an instruction starting at $07FE-$7FFF fetches everything through
// read_byte (RAM mirrors, registers, open bus, WRAM), otherwise all its bytes are in canonical RAM ($0000-$07FF)
// or ROM and are read directly. RAM mirrors must not be fetched directly: they are separate MMU mappings of the
// same pages and could return stale data from the SH4 operand cache after a write through $0000-$07FF.
#define IS_IO_INSTRUCTION(pc) ((uint16_t)((pc) - 0x07fe) < 0x7802)
// $6000-$7FFF, when it's plain PRG RAM (WRAM present, no mapper answering its reads): read directly through the
// window like ROM (writes go through the same window addresses, so the operand cache can't be stale)
#define IS_DIRECT_6000(pc) (((uint16_t)((pc) - 0x6000) < 0x2000) && (WRAM != NULL) && !mapper_6000_reads)
#define FETCH_BYTE() (__builtin_expect(io_instruction, 0) ? IO_READ(r_pc++) : READ_BYTE(r_pc++))

#define PUSH_BYTE(b) NES_WINDOW_BYTE(BASE_STACK + r_sp--) = (b);
#define PUSH_WORD(w) PUSH_BYTE((w) >> 8); PUSH_BYTE((w) & 0xff);
#define POP_BYTE() NES_WINDOW_BYTE(BASE_STACK + ++r_sp)

#define RESET_FLAG(f) r_p &= ~(f)
/*
 * Interrupt hijacking: BRK and the IRQ sequence pick their vector on their 5th cycle, so an NMI that comes up
 * before then takes the vector over (the pushed status keeps BRK's B flag). One that comes up on the last two
 * cycles is taken after the first instruction of the handler. 'start' is the sequence's first cycle.
 */
#define INTERRUPT_VECTOR(vector, start) \
    ((cpu_registers -> NMI_Request || \
      (ppu_cycles_to_vblank_nmi() <= cpu_cycle_total + (start) - devices_synced_cycle + 8)) ? \
        INTERRUPT_VECTOR_RACE(vector, start) : (mapper_vector_fetch(vector), read_word(vector)))
/* (only when an NMI is pending or the VBlank NMI is due: bringing the PPU up to mid instruction isn't free) */
#define INTERRUPT_VECTOR_RACE(vector, start) \
    ({ \
        cpu_cycle_now = cpu_cycle_total + (start) + INTERRUPT_HIJACK_CYCLE; \
        cpu_sync_devices(); \
        uint16_t chosen = (vector); \
        /* (an NMI raised on the very last dot before that point is too late) */ \
        if (cpu_registers -> NMI_Request && !(ppu_nmi_raised && (ppu_nmi_dots_after < INTERRUPT_HIJACK_LATE_DOTS))) { \
            cpu_registers -> NMI_Request = 0; \
            chosen = VECTOR_NMI; \
        } \
        mapper_vector_fetch(chosen); \
        uint16_t target = read_word(chosen); \
        cpu_cycle_now = cpu_cycle_total + (start) + 7; \
        cpu_sync_devices(); \
        if (cpu_registers -> NMI_Request == 1) { \
            cpu_registers -> NMI_Request = 2; \
        } \
        target; \
    })
#define INTERRUPT_HIJACK_CYCLE 4
#ifndef INTERRUPT_HIJACK_LATE_DOTS
#define INTERRUPT_HIJACK_LATE_DOTS 2
#endif
// a VBlank NMI raised less than this many dots before the end of a batch came after its last instruction's poll
#ifndef CPU_NMI_LATE_DOTS
#define CPU_NMI_LATE_DOTS 5
#endif
// called by CLI / SEI / PLP with the status from before them: the poll made during the instruction (before its
// last cycle) saw that I flag, which only matters with an IRQ pending
#define IRQ_POLL_BEFORE(old) \
    if (cpu_registers -> IRQ_Request) { \
        cpu_registers -> IRQ_Poll = ((old) & FLAG_I) ? IRQ_POLL_I_SET : IRQ_POLL_I_CLEAR; \
    }
#define SET_FLAG(f) r_p |= (f)

// N and Z flag values for a result byte
#define NZ(b) (((b) & FLAG_N) | ((b) ? 0 : FLAG_Z))
#define TEST(b) r_p = (r_p & ~(FLAG_N | FLAG_Z)) | NZ(b)

/**
 * Addressing modes, each computes the effective address into 'address' and advances PC past the operand.
 * The _R variants add the page crossing cycle penalty used by read instructions.
 */
#define ADDR_ZP()  address = FETCH_BYTE()
#define ADDR_ZPX() address = (uint8_t)(FETCH_BYTE() + r_x)
#define ADDR_ZPY() address = (uint8_t)(FETCH_BYTE() + r_y)
#define ADDR_ABS() address = FETCH_BYTE(); address |= FETCH_BYTE() << 8
#define ADDR_ABX() ADDR_ABS(); base_address = address; address += r_x
#define ADDR_ABY() ADDR_ABS(); base_address = address; address += r_y
// Indexed accesses read the address with the index added to the low byte only (before the high byte is fixed)
// while they work out the carry: reads do it when a page is crossed (the extra cycle), writes and read-modify-
// writes always. Only register reads ($2000-$401F) have side effects, so the dummy read is only made for those.
// Over open bus ($4020-$7FFF) the dummy read leaves the operand's high byte on the data bus, which the read after
// it then gets (not the high byte of the address it reads).
#define DUMMY_READ(a) do { uint16_t dummy_address = (a); if ((uint16_t)(dummy_address - 0x2000) < 0x2020) { IO_READ_AT(dummy_address, cycles - 2); } \
    else if ((uint16_t)(dummy_address - 0x4020) < 0x3fe0) { cpu_io_bus_value = cpu_internal_bus_value = dummy_address >> 8; cpu_io_bus_cycle = cpu_cycle_total + cycles_done + cycles - 2; } } while (0)
#define PAGE_DUMMY_READ() DUMMY_READ((base_address & 0xff00) | (address & 0x00ff))
#define ADDR_ABX_R() ADDR_ABX(); if ((base_address ^ address) & 0x0100) { cycles++; PAGE_DUMMY_READ(); }
#define ADDR_ABY_R() ADDR_ABY(); if ((base_address ^ address) & 0x0100) { cycles++; PAGE_DUMMY_READ(); }
#define ADDR_ABX_W() ADDR_ABX(); PAGE_DUMMY_READ()
#define ADDR_ABY_W() ADDR_ABY(); PAGE_DUMMY_READ()
#define ADDR_IZX() byte_value = FETCH_BYTE() + r_x; address = ZP_READ(byte_value) | (ZP_READ(byte_value + 1) << 8)
#define ADDR_IZY() byte_value = FETCH_BYTE(); base_address = ZP_READ(byte_value) | (ZP_READ(byte_value + 1) << 8); address = base_address + r_y
#define ADDR_IZY_R() ADDR_IZY(); if ((base_address ^ address) & 0x0100) { cycles++; PAGE_DUMMY_READ(); }
#define ADDR_IZY_W() ADDR_IZY(); PAGE_DUMMY_READ()
// read-modify-write: the dummy read is on cycle 3 (abs,X / abs,Y) / 4 ((zp),Y), three before the read
#define RMW_DUMMY_READ() do { uint16_t dummy_address = (base_address & 0xff00) | (address & 0x00ff); if ((uint16_t)(dummy_address - 0x2000) < 0x2020) { IO_READ_AT(dummy_address, cycles - 4); } } while (0)
#define ADDR_ABX_M() ADDR_ABX(); RMW_DUMMY_READ()
#define ADDR_ABY_M() ADDR_ABY(); RMW_DUMMY_READ()
#define ADDR_IZY_M() ADDR_IZY(); RMW_DUMMY_READ()

/**
 * ALU operations on a value
 */
#define ORA(b) r_a |= (b); TEST(r_a)
#define AND(b) r_a &= (b); TEST(r_a)
#define EOR(b) r_a ^= (b); TEST(r_a)
#define LDA(b) r_a = (b); TEST(r_a)
#define LDX(b) r_x = (b); TEST(r_x)
#define LDY(b) r_y = (b); TEST(r_y)
#define BIT_TEST(b) byte_value = (b); r_p = (r_p & ~(FLAG_N | FLAG_V | FLAG_Z)) | (byte_value & (FLAG_N | FLAG_V)) | ((byte_value & r_a) ? 0 : FLAG_Z)
#define COMPARE(r, b) byte_value = (b); r_p = (r_p & ~(FLAG_N | FLAG_Z | FLAG_C)) | ((r) >= byte_value ? FLAG_C : 0) | NZ((uint8_t)((r) - byte_value))
#define CMP(b) COMPARE(r_a, b)
#define CPX(b) COMPARE(r_x, b)
#define CPY(b) COMPARE(r_y, b)

// The NES 2A03 has no decimal mode, so the D flag is ignored
#define ADC(b) \
    byte_value = (b); \
    word_value = r_a + byte_value + (r_p & FLAG_C); \
    r_p = (r_p & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) | \
        ((word_value >> 8) & FLAG_C) | \
        (((r_a ^ word_value) & (byte_value ^ word_value) & 0x80) ? FLAG_V : 0) | \
        NZ((uint8_t)word_value); \
    r_a = (uint8_t)word_value
#define SBC(b) ADC((uint8_t)~(b))

/**
 * Shift / rotate operations, these update and evaluate to the new value of 'b' (an lvalue)
 */
#define ASL_V(b) r_p = (r_p & ~FLAG_C) | ((b) >> 7); (b) <<= 1; TEST(b)
#define LSR_V(b) r_p = (r_p & ~FLAG_C) | ((b) & FLAG_C); (b) >>= 1; TEST(b)
#define ROL_V(b) previous_status = r_p & FLAG_C; r_p = (r_p & ~FLAG_C) | ((b) >> 7); (b) = ((b) << 1) | previous_status; TEST(b)
#define ROR_V(b) previous_status = r_p & FLAG_C; r_p = (r_p & ~FLAG_C) | ((b) & FLAG_C); (b) = ((b) >> 1) | (previous_status << 7); TEST(b)
#define INC_V(b) (b)++; TEST(b)
#define DEC_V(b) (b)--; TEST(b)

// read-modify-write on memory at 'address' (or zero page with the ZP variant)
// read-modify-write writes the unmodified value back first (a register or mapper write sees both)
#define RMW(op) rmw_value = IO_READ_AT(address, cycles - 3); if (address >= 0x2000) { IO_WRITE_AT(address, rmw_value, cycles - 2); if (cpu_registers -> NMI_Request == 2) { cpu_registers -> NMI_Request = 1; } } op(rmw_value); MEM_WRITE(address, rmw_value)
#define RMW_ZP(op) rmw_value = ZP_READ(address); op(rmw_value); ZP_WRITE(address, rmw_value)

/**
 * Unofficial read-modify-write combinations
 */
#define SLO_V(b) ASL_V(b); ORA(b)
#define RLA_V(b) ROL_V(b); AND(b)
#define SRE_V(b) LSR_V(b); EOR(b)
#define RRA_V(b) ROR_V(b); ADC(b)
#define DCP_V(b) (b)--; CMP(b)
#define ISC_V(b) (b)++; SBC(b)

#define LAX(b) r_a = r_x = (b); TEST(r_a)

// SHA / SHX / SHY / SHS store a value ANDed with the high byte of the base address + 1,
// and when the index crosses a page the stored value also replaces the high byte of the address
#define SH_STORE(v) \
    byte_value = (v) & ((base_address >> 8) + 1); \
    if ((base_address ^ address) & 0xff00) { \
        address = (byte_value << 8) | (address & 0x00ff); \
    } \
    MEM_WRITE(address, byte_value)

// Magic constant ORed into A by the unstable ANE / LXA opcodes
#define UNSTABLE_MAGIC 0xEE

// Branches, 2 cycles not taken, +1 taken, +1 more when the target is on a different page.
// A taken branch dummy reads the next opcode, and on a page cross also the target address before the high
// byte is fixed. Those reads only have side effects (and only need doing) in the register / open bus range.
#define BRA(cond) \
    byte_value = FETCH_BYTE(); \
    if (cond) { \
        address = r_pc + (int8_t)byte_value; \
        if (__builtin_expect(IS_IO_FETCH(r_pc), 0)) { \
            IO_READ(r_pc); \
        } \
        if ((r_pc ^ address) & 0xff00) { \
            base_address = (r_pc & 0xff00) | (address & 0x00ff); \
            if (__builtin_expect(IS_IO_FETCH(base_address), 0)) { \
                IO_READ(base_address); \
            } \
            cycles += 2; \
        } else { \
            cycles += 1; \
            /* polls before its second cycle, not its last (see cpu_poll_early) */ \
            cpu_poll_early = 1; \
        } \
        r_pc = address; \
    }

/**
 * Base cycle counts per opcode (page crossing and branch penalties are added in the handlers)
 */
// (in .hotdata with the asm core's dispatch table: read every instruction, its operand cache sets shouldn't move with the
// size of unrelated code before .rodata; kept in builds without the C core too, so the hot data's layout doesn't change)
static const uint8_t cycle_table[256] FRNES_HOT_RODATA __attribute__((used)) = {
/*       0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
/* 0 */  7, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6,
/* 1 */  2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
/* 2 */  6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6,
/* 3 */  2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
/* 4 */  6, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6,
/* 5 */  2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
/* 6 */  6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6,
/* 7 */  2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
/* 8 */  2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
/* 9 */  2, 6, 2, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5,
/* A */  2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
/* B */  2, 5, 2, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,
/* C */  2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
/* D */  2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
/* E */  2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
/* F */  2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7
};

#ifdef DEBUG
static const char* const opcode_names[256] = {
/*        0      1      2      3      4      5      6      7      8      9      A      B      C      D      E      F */
/* 0 */ "BRK", "ORA", "JAM", "SLO", "NOP", "ORA", "ASL", "SLO", "PHP", "ORA", "ASL", "ANC", "NOP", "ORA", "ASL", "SLO",
/* 1 */ "BPL", "ORA", "JAM", "SLO", "NOP", "ORA", "ASL", "SLO", "CLC", "ORA", "NOP", "SLO", "NOP", "ORA", "ASL", "SLO",
/* 2 */ "JSR", "AND", "JAM", "RLA", "BIT", "AND", "ROL", "RLA", "PLP", "AND", "ROL", "ANC", "BIT", "AND", "ROL", "RLA",
/* 3 */ "BMI", "AND", "JAM", "RLA", "NOP", "AND", "ROL", "RLA", "SEC", "AND", "NOP", "RLA", "NOP", "AND", "ROL", "RLA",
/* 4 */ "RTI", "EOR", "JAM", "SRE", "NOP", "EOR", "LSR", "SRE", "PHA", "EOR", "LSR", "ALR", "JMP", "EOR", "LSR", "SRE",
/* 5 */ "BVC", "EOR", "JAM", "SRE", "NOP", "EOR", "LSR", "SRE", "CLI", "EOR", "NOP", "SRE", "NOP", "EOR", "LSR", "SRE",
/* 6 */ "RTS", "ADC", "JAM", "RRA", "NOP", "ADC", "ROR", "RRA", "PLA", "ADC", "ROR", "ARR", "JMP", "ADC", "ROR", "RRA",
/* 7 */ "BVS", "ADC", "JAM", "RRA", "NOP", "ADC", "ROR", "RRA", "SEI", "ADC", "NOP", "RRA", "NOP", "ADC", "ROR", "RRA",
/* 8 */ "NOP", "STA", "NOP", "SAX", "STY", "STA", "STX", "SAX", "DEY", "NOP", "TXA", "ANE", "STY", "STA", "STX", "SAX",
/* 9 */ "BCC", "STA", "JAM", "SHA", "STY", "STA", "STX", "SAX", "TYA", "STA", "TXS", "SHS", "SHY", "STA", "SHX", "SHA",
/* A */ "LDY", "LDA", "LDX", "LAX", "LDY", "LDA", "LDX", "LAX", "TAY", "LDA", "TAX", "LXA", "LDY", "LDA", "LDX", "LAX",
/* B */ "BCS", "LDA", "JAM", "LAX", "LDY", "LDA", "LDX", "LAX", "CLV", "LDA", "TSX", "LAE", "LDY", "LDA", "LDX", "LAX",
/* C */ "CPY", "CMP", "NOP", "DCP", "CPY", "CMP", "DEC", "DCP", "INY", "CMP", "DEX", "AXS", "CPY", "CMP", "DEC", "DCP",
/* D */ "BNE", "CMP", "JAM", "DCP", "NOP", "CMP", "DEC", "DCP", "CLD", "CMP", "NOP", "DCP", "NOP", "CMP", "DEC", "DCP",
/* E */ "CPX", "SBC", "NOP", "ISC", "CPX", "SBC", "INC", "ISC", "INX", "SBC", "NOP", "SBC", "CPX", "SBC", "INC", "ISC",
/* F */ "BEQ", "SBC", "JAM", "ISC", "NOP", "SBC", "INC", "ISC", "SED", "SBC", "NOP", "ISC", "NOP", "SBC", "INC", "ISC"
};

// One line per instruction, laid out to be easy to compare against a Mesen trace log
static void TraceRegisters(uint16_t pc, uint8_t opcode, uint8_t a, uint8_t x, uint8_t y, uint8_t sp, uint8_t s) {
    printf("%04X  %02X %s  A:%02X X:%02X Y:%02X S:%02X P:%c%c--%c%c%c%c\n",
        pc, opcode, opcode_names[opcode], a, x, y, sp,
        (s & FLAG_N) ? 'N' : 'n', (s & FLAG_V) ? 'V' : 'v', (s & FLAG_D) ? 'D' : 'd',
        (s & FLAG_I) ? 'I' : 'i', (s & FLAG_Z) ? 'Z' : 'z', (s & FLAG_C) ? 'C' : 'c');
}
#define Trace(opcode) TraceRegisters((uint16_t)(r_pc - 1), opcode, r_a, r_x, r_y, r_sp, r_p)
#else
#define Trace(...)
#endif

int initialize_cpu() {
    printf("cpu - initializing\n");
    cpu_registers -> PC = read_word( VECTOR_RESET );
    printf(" ...loaded reset vector [0x%04x] - [0x%04x]\n", VECTOR_RESET, cpu_registers -> PC);
    cpu_registers -> SP = 0xfd;
    cpu_registers -> A = cpu_registers -> X = cpu_registers -> Y = 0;
    cpu_registers -> S = (FLAG_I | FLAG_R);

    cpu_registers -> NMI_Request = 0;
    cpu_registers -> IRQ_Request = 0;
    cpu_registers -> IRQ_Poll = 0;
    cpu_registers -> PAD1_Latch = cpu_registers -> PAD2_Latch = 0;
    cpu_registers -> PAD1_Bit = cpu_registers -> PAD2_Bit = 0;
    cpu_registers -> PAD_Strobe = 0;
    cpu_registers -> DMA_Cycles = 0;
    printf(" ...done\n");

    return 0;
}

void shutdown_cpu() {
    printf("cpu - shutting down\n");
    printf(" ...done\n");
}

// CPU_PROFILE_OPCODES counts executed opcodes, for choosing which ones to hand optimize
#ifdef CPU_PROFILE_OPCODES
uint32_t cpu_opcode_counts[256];
#define PROFILE_OPCODE(op) cpu_opcode_counts[op]++
#else
#define PROFILE_OPCODE(op)
#endif

// CPU time, in CPU cycles since power on (wraps, only differences are used)
uint32_t cpu_cycle_total = 0;
// cycle at the start of the instruction currently making a memory mapped I/O access
uint32_t cpu_cycle_now = 0;
uint8_t cpu_batch_cut = 0;
#ifdef CPU_TRACE_PC
typedef struct { uint16_t pc; uint8_t a, x, y, p, s; uint32_t cycle; } cpu_trace_entry_t;
static cpu_trace_entry_t cpu_trace[CPU_TRACE_COUNT];
static uint32_t cpu_trace_count = 0, cpu_trace_on = 0;
void cpu_trace_dump() {
    for (uint32_t i = 0; i < cpu_trace_count; i++) {
        cpu_trace_entry_t* e = &cpu_trace[i];
        printf("TRACE pc=%04X a=%02X x=%02X y=%02X p=%02X s=%02X sl=0 dot=0 cyc=%lu\n", e -> pc, e -> a, e -> x, e -> y,
            e -> p, e -> s, (unsigned long)e -> cycle);
    }
}
#else
void cpu_trace_dump() {}
#endif
uint32_t cpu_batch_end = 0;
#ifdef FRNES_HOST
// host builds: set by host/asm_stub.c while it runs instructions like the assembly core would
int cpu_host_like_asm;
// host builds: called at the start of every instruction (host_main.c's --trace)
void (*cpu_host_trace)(uint16_t pc, uint8_t a, uint8_t x, uint8_t y, uint8_t p, uint8_t s, uint32_t cycle);
#endif
// the batch's last instruction was a taken branch that stayed on its page, which polls for interrupts a cycle early
uint8_t cpu_poll_early = 0;
// the cycle the last instruction ended on (before any DMA stall that follows it)
uint32_t cpu_instruction_end = 0;
#define APU_IRQ_POLL_MISS_CYCLES 2
// how far the PPU and APU have been brought up to date
uint32_t cpu_devices_synced_cycle = 0;
// the cycle the IRQ line last went up during (see cpu.h), and whether a poll has yet to see it
uint32_t cpu_irq_raise_cycle = 0;
uint8_t cpu_irq_raise_recent = 0;

void cpu_irq_raise_at(uint8_t source, uint32_t cycle) {
    cpu_irq_stamp(cycle);
    mapper_irq_flag |= source;
    cpu_irq_update_line();
}

void cpu_irq_clear(uint8_t source) {
    mapper_irq_flag &= ~source;
    cpu_irq_update_line();
}
#define devices_synced_cycle cpu_devices_synced_cycle

#ifdef FRNES_PERF_SPLIT
/* diagnostic: performance counter 1 (set up by the test harness, AC_PERF_EVENT) counted in apu_run and ppu_run */
#include <dc/perfctr.h>
uint64_t perf_split_apu = 0, perf_split_ppu = 0;
#endif

// the last VBlank NMI a device catch-up raised: the cycle it caught up to and how many dots before that the NMI came up
// (an access in the middle of an instruction can raise it, see cpu_nmi_raised_late)
uint32_t cpu_nmi_raise_cycle = 0;
uint32_t cpu_nmi_raise_dots = 0;

// Interrupts are polled before an instruction's last cycle: a VBlank NMI that came up less than CPU_NMI_LATE_DOTS
// before the end of the instruction ending at 'boundary' is taken one instruction later
// (a taken branch that stays on its page polls before its second cycle, cpu_poll_early: one cycle more)
static inline int cpu_nmi_raised_late(uint32_t boundary) {
    int32_t cycles = (int32_t)(boundary - cpu_nmi_raise_cycle);
    return (cycles >= 0) && (cycles <= 2) && ((uint32_t)cycles * 3 + cpu_nmi_raise_dots < CPU_NMI_LATE_DOTS + 3u * cpu_poll_early);
}

__attribute__((noinline, noclone)) void cpu_sync_devices_io(uint16_t address) {
    // ($4025's mirroring bit syncs the PPU itself)
    if (!board_low_unsynced || (address < 0x4020)) {
        cpu_sync_devices();
    }
}

/*
 * The interrupt poll at an instruction boundary, for both cores (the C core's loop, and cpu_run's fallback to
 * cpu_exact.c behind the assembly core): 'boundary' the cycle the next instruction starts on, 'p' the status flags
 * then. Returns 1 when an NMI is taken here, 2 when an IRQ is, 0 to run the instruction. An NMI raised by the last
 * cycle of the previous instruction (NMI_Request 2, or a VBlank NMI raised less than CPU_NMI_LATE_DOTS before it) is
 * polled at the end of the next one. IRQ is level triggered, the source keeps IRQ_Request set until it is
 * acknowledged; the poll happens before the last cycle of each instruction, so after CLI / SEI / PLP it still sees
 * the I flag from before them (IRQ_Poll).
 */
static __attribute__((noinline, cold)) int cpu_boundary_poll(uint8_t p, uint32_t boundary, uint16_t pc, uint8_t x) {
    (void)pc;
    (void)x;
    if ((cpu_registers -> NMI_Request == 2) ||
        (cpu_registers -> NMI_Request && cpu_nmi_raised_late(boundary))) {
        cpu_registers -> NMI_Request = 1;
    } else if (cpu_registers -> NMI_Request) {
        return 1;
    }
    uint32_t irq_blocked = cpu_registers -> IRQ_Poll ? (cpu_registers -> IRQ_Poll == IRQ_POLL_I_SET) : (p & FLAG_I);
    cpu_registers -> IRQ_Poll = 0;
    // an APU IRQ raised during the last cycles of the previous instruction (or since, during a DMA stall) came after
    // its poll, and one more cycle after a taken branch that stayed on its page (it polls before its second cycle)
    if (cpu_irq_raise_recent) {
        if ((int32_t)(cpu_instruction_end - cpu_irq_raise_cycle) < (int32_t)(APU_IRQ_POLL_MISS_CYCLES + cpu_poll_early)) {
            irq_blocked = 1;
        } else {
            cpu_irq_raise_recent = 0;
        }
    }
    cpu_poll_early = 0;
#ifdef CPU_IRQ_LOG
    if ((pc >= (CPU_IRQ_LOG)) && (pc < (CPU_IRQ_LOG) + 0x10)) {
        printf("IRQLOG pc=%04X x=%02X p=%02X blocked=%u poll=%u cyc=%lu after=%lu\n", pc, x, p, (unsigned)irq_blocked,
            (unsigned)cpu_registers -> IRQ_Poll, (unsigned long)cpu_instruction_end, (unsigned long)cpu_irq_raise_cycle);
    }
#endif
    return (cpu_registers -> IRQ_Request && !irq_blocked) ? 2 : 0;
}

FRNES_HOT(3_sync) void cpu_sync_devices() {
    ppu_nmi_raised = 0;
    uint32_t delta = cpu_cycle_now - devices_synced_cycle;
    if (delta) {
        devices_synced_cycle = cpu_cycle_now;
#if defined(FRNES_PERF_SPLIT)
        uint64_t c0 = perf_cntr_count(PRFC1);
        apu_run(delta);
        uint64_t c1 = perf_cntr_count(PRFC1);
        ppu_run(delta);
        perf_split_ppu += perf_cntr_count(PRFC1) - c1;
        perf_split_apu += c1 - c0;
#elif defined(PPU_STATS)
        extern uint64_t ppu_stat_apu_us, ppu_stat_ppu_us;
        uint64_t t0 = timer_us_gettime64();
        apu_run(delta);
        uint64_t t1 = timer_us_gettime64();
        ppu_run(delta);
        ppu_stat_ppu_us += timer_us_gettime64() - t1;
        ppu_stat_apu_us += t1 - t0;
#else
        apu_run(delta);
        ppu_run(delta);
#endif
        if (ppu_nmi_raised) {
            cpu_nmi_raise_cycle = cpu_cycle_now;
            cpu_nmi_raise_dots = ppu_nmi_dots_after;
        }
    }
}

static uint32_t cpu_cycles_to_device_event();

// the batch being run was sized for a mapper's CPU cycle counter, before the next device event (cpu_batch_budget)
uint8_t cpu_batch_mapper_only = 0;
uint32_t cpu_batch_device_event = 0;
// the batch was cut, or it has to end with the devices caught up anyway (cpu_cut_batch_for_event)
uint8_t cpu_batch_resync = 0;

uint32_t cpu_batch_budget() {
    // the devices' events are counted from where they were last caught up to, which a batch ended for a mapper
    // counter leaves behind cpu_cycle_total (the mapper counters count from cpu_cycle_now, which is cpu_cycle_total
    // here)
    uint32_t behind = cpu_cycle_total - devices_synced_cycle;
    uint32_t device = cpu_cycles_to_device_event();
    device = (device > behind) ? device - behind : 1;
    cpu_batch_device_event = cpu_cycle_total + device;
    cpu_batch_mapper_only = 0;
    if (mapper_cpu_timer) {
        uint32_t mapper_cycles = mapper_cycles_to_event();
        if (mapper_cycles < device) {
            cpu_batch_mapper_only = 1;
            return mapper_cycles ? mapper_cycles : 1;
        }
    }
    return device;
}

uint32_t cpu_cycles_to_next_event() {
    uint32_t cycles = cpu_cycles_to_device_event();
    // a mapper's CPU cycle IRQ counter (FME-7, VRC)
    if (mapper_cpu_timer) {
        uint32_t mapper_cycles = mapper_cycles_to_event();
        if (mapper_cycles < cycles) {
            cycles = mapper_cycles;
        }
    }
    return cycles ? cycles : 1;
}

/*
 * (cpu_exact.c's DMC fast path) the cycle of the next event the devices or a mapper counter have besides the DMC's
 * request: until then nothing changes an interrupt line or needs a catch-up. cpu_cycle_now is the current cycle.
 */
uint32_t cpu_next_event_besides_dmc() {
    uint32_t cycles = ppu_cycles_to_next_event();
    int32_t apu_cycles = apu_cycles_to_event_besides_dmc();
    if (apu_cycles < (int32_t)cycles) {
        cycles = (apu_cycles > 0) ? (uint32_t)apu_cycles : 0;
    }
    if (mapper_irq_armed && PPU_MAPPER_RENDERING()) {
        uint32_t mapper_cycles = ppu_cycles_to_mapper_clock(mapper_clocks_to_irq());
        if (mapper_cycles < cycles) {
            cycles = mapper_cycles;
        }
    }
    uint32_t event = devices_synced_cycle + cycles;
    if (mapper_cpu_timer) {
        // (the mapper counters count from cpu_cycle_now)
        uint32_t mapper_cycles = mapper_cycles_to_event();
        if ((mapper_cycles != 0xffffffff) && ((int32_t)(cpu_cycle_now + mapper_cycles - event) < 0)) {
            event = cpu_cycle_now + mapper_cycles;
        }
    }
    return event;
}

// the PPU's, the APU's and a mapper scanline IRQ's next events, from where the devices were last caught up to
static uint32_t cpu_cycles_to_device_event() {
    uint32_t cycles = ppu_cycles_to_next_event();
    // the next APU frame counter step or DMC sample fetch
    if ((apu_cycles_to_event > 0) && ((uint32_t)apu_cycles_to_event < cycles)) {
        cycles = apu_cycles_to_event;
    }
    // an armed mapper scanline IRQ: the mapper clock it can fire at
    if (mapper_irq_armed && PPU_MAPPER_RENDERING()) {
        uint32_t mapper_cycles = ppu_cycles_to_mapper_clock(mapper_clocks_to_irq());
        if (mapper_cycles < cycles) {
            cycles = mapper_cycles;
        }
    }
#ifdef CPU_CUT_EVERY
    // test builds: extra batch boundaries, results must not depend on where batches end
    if (cycles > CPU_CUT_EVERY) {
        cycles = CPU_CUT_EVERY;
    }
#endif
    return cycles ? cycles : 1;
}

void cpu_run_devices() {
    cpu_cycle_now = cpu_cycle_total;
    // a batch sized for a mapper's CPU cycle counter (the FDS drive's byte IRQs, 150 cycles apart while it reads),
    // run to its planned end without a cut (an access raising an interrupt, a device event moved earlier) ends with the PPU and APU left where they are: they catch up at the next device event or register access as
    // they would have mid batch, rather than in a short catch-up every few hundred cycles
    if (cpu_batch_mapper_only && !cpu_batch_resync && ((int32_t)(cpu_cycle_total - cpu_batch_end) >= 0) &&
        ((int32_t)(cpu_cycle_total - cpu_batch_device_event) < 0)) {
        // (the counter can have been switched off during the batch: a write that disables it cuts nothing)
        if (mapper_cpu_timer) {
            mapper_cpu_run(cpu_cycle_total);
        }
        return;
    }
    cpu_batch_resync = 0;
    cpu_sync_devices();
    // (a mapper's CPU cycle counter can't be read back, so it only needs to catch up at the end of each batch: the
    // batch ends at its next IRQ, see cpu_cycles_to_next_event)
    if (mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_total);
    }
    // Interrupts are polled before an instruction's last cycle: a VBlank NMI that came up during the last cycle of
    // the batch's final instruction (less than 3 dots before its end) is taken one instruction later
    if (ppu_nmi_raised && (ppu_nmi_dots_after < CPU_NMI_LATE_DOTS + 3u * cpu_poll_early) && (cpu_registers -> NMI_Request == 1)) {
        cpu_registers -> NMI_Request = 2;
    }
}

/**
 * @brief Runs 6502 instructions (and services interrupts) until at least 'budget' cycles have passed
 *
 * The 6502 registers live in local variables for the whole batch. Memory mapped I/O goes through IO_READ /
 * IO_WRITE, which first bring the PPU and APU up to the cycle where the current instruction starts (the same
 * point the PPU was at when it ran after every instruction). The caller picks the budget with
 * cpu_cycles_to_next_event() so a batch never runs past vblank / NMI, the end of a frame or an APU IRQ.
 *
 * @return the number of cycles run, or CPU_HALTED for an unimplemented (JAM) opcode with PC left on it
 */
#if defined(CPU_C_ONLY) || defined(FRNES_HOST)
#ifdef CPU_C_ONLY
// (the C core's interpreter: in its own hot slot, "c", tools/hot_slots.py, so cold code growing elsewhere can't move it)
FRNES_HOT(c_run) uint32 cpu_run(uint32 budget) {
#else
static uint32 cpu_run_c(uint32 budget) {
#endif
    uint8_t byte_value;
    uint8_t previous_status;
    uint16_t word_value;
    uint16_t address;
    uint16_t base_address;
    uint8_t rmw_value;
    uint32 cycles;
    uint32 cycles_done = 0;

    uint16_t r_pc = cpu_registers -> PC;
    uint8_t r_a = cpu_registers -> A;
    uint8_t r_x = cpu_registers -> X;
    uint8_t r_y = cpu_registers -> Y;
    uint8_t r_sp = cpu_registers -> SP;
    uint8_t r_p = cpu_registers -> S;
#ifdef CPU_C_ONLY
    cpu_batch_cut = 0;
    cpu_batch_end = cpu_cycle_total + budget;
#endif

    while (cycles_done < budget) {

    // a DMA due, or code running from the registers / open bus (it sees the data bus as it is on each cycle): the
    // instruction (or interrupt) runs cycle by cycle in cpu_exact.c
    int exact = cpu_exact_due(cpu_cycle_total + cycles_done) || CPU_EXACT_FETCH(r_pc);
#ifdef FRNES_HOST
    // (host_asm_stub.c: runs whole batches like the assembly core, which only looks at the budget)
    if (cpu_host_like_asm) {
        exact = 0;
    }
#endif
    if (cpu_exact_polled) {
        // cpu_exact.c made the poll for this boundary already (an NMI it saw too late waits for the next one)
        cpu_exact_polled = 0;
        cpu_poll_early = 0;
        cpu_registers -> IRQ_Poll = 0;
        if (cpu_registers -> NMI_Request) {
            cpu_registers -> NMI_Request = 1;
        }
        goto execute;
    }
    // (with no NMI, no IRQ the I flag lets through and no poll state left by the last instruction, the poll only
    // clears cpu_poll_early: the C core's per-instruction path keeps just this test, the poll itself is out of line.
    // A masked IRQ is common, 1943 keeps one pending all the time)
    int poll = 0;
    if (cpu_registers -> NMI_Request | cpu_registers -> IRQ_Poll | cpu_irq_raise_recent |
        (cpu_registers -> IRQ_Request && !(r_p & FLAG_I))) {
        poll = cpu_boundary_poll(r_p, cpu_cycle_total + cycles_done, r_pc, r_x);
    } else {
        cpu_poll_early = 0;
    }
    if (poll == 1) {
        if (exact) {
            goto exact_interrupt;
        }
        cpu_registers -> NMI_Request = 0;
        PUSH_WORD(r_pc);
        PUSH_BYTE((r_p & ~FLAG_B) | FLAG_R);
        SET_FLAG(FLAG_I);
        mapper_vector_fetch(VECTOR_NMI);
        r_pc = IO_READ_WORD(VECTOR_NMI);
        cycles_done += 7;
        cpu_instruction_end = cpu_cycle_total + cycles_done;
        continue;
    }
    if (poll == 2) {
        if (exact) {
            goto exact_interrupt;
        }
        PUSH_WORD(r_pc);
        PUSH_BYTE((r_p & ~FLAG_B) | FLAG_R);
        SET_FLAG(FLAG_I);
        r_pc = INTERRUPT_VECTOR(VECTOR_IRQ, cycles_done);
        cycles_done += 7;
        cpu_instruction_end = cpu_cycle_total + cycles_done;
        continue;
    }

#ifdef CPU_TRACE_PC
    /* debugging: an instruction trace in RAM from the first time PC reaches CPU_TRACE_PC, CPU_TRACE_COUNT entries,
       without changing how the CPU runs (build with CPU_C_ONLY); printed by cpu_trace_dump() */
    if (!cpu_trace_on && (r_pc == (CPU_TRACE_PC))) {
        cpu_trace_on = 1;
    }
    if (cpu_trace_on && (cpu_trace_count < (CPU_TRACE_COUNT))) {
        cpu_trace_entry_t* e = &cpu_trace[cpu_trace_count++];
        e -> pc = r_pc; e -> a = r_a; e -> x = r_x; e -> y = r_y; e -> p = r_p; e -> s = r_sp;
        e -> cycle = cpu_cycle_total + cycles_done;
    }
#endif
execute:
#ifdef FRNES_HOST
    if (cpu_host_trace) {
        cpu_host_trace(r_pc, r_a, r_x, r_y, r_p, r_sp, cpu_cycle_total + cycles_done);
    }
#endif
    if (__builtin_expect(exact, 0)) {
        uint32_t exact_cycles;
        int exact_interrupt_flag;
        exact_interrupt_flag = 0;
        if (0) {
exact_interrupt:
            exact_interrupt_flag = 1;
        }
        cpu_registers -> PC = r_pc;
        cpu_registers -> A = r_a;
        cpu_registers -> X = r_x;
        cpu_registers -> Y = r_y;
        cpu_registers -> SP = r_sp;
        cpu_registers -> S = r_p;
        exact_cycles = cpu_exact_run(cpu_cycle_total + cycles_done, exact_interrupt_flag);
        r_pc = cpu_registers -> PC;
        r_a = cpu_registers -> A;
        r_x = cpu_registers -> X;
        r_y = cpu_registers -> Y;
        r_sp = cpu_registers -> SP;
        r_p = cpu_registers -> S;
        if (exact_cycles == CPU_HALTED) {
            cycles_done = CPU_HALTED;
            goto done;
        }
        cycles_done += exact_cycles;
        cpu_instruction_end = cpu_cycle_total + cycles_done;
#ifdef CPU_C_ONLY
        if (cpu_batch_cut) {
            break;
        }
#endif
        continue;
    }
    uint32_t io_instruction = IS_IO_INSTRUCTION(r_pc) && !IS_DIRECT_6000(r_pc);
    uint8_t opcode = FETCH_BYTE();
    PROFILE_OPCODE(opcode);
    Trace(opcode);
    cycles = cycle_table[opcode];

    switch (opcode) {
        /**
         * Loads
         */
        case 0xA9: LDA(FETCH_BYTE()); break;
        case 0xA5: ADDR_ZP();    LDA(ZP_READ(address)); break;
        case 0xB5: ADDR_ZPX();   LDA(ZP_READ(address)); break;
        case 0xAD: ADDR_ABS();   LDA(MEM_READ(address)); break;
        case 0xBD: ADDR_ABX_R(); LDA(MEM_READ(address)); break;
        case 0xB9: ADDR_ABY_R(); LDA(MEM_READ(address)); break;
        case 0xA1: ADDR_IZX();   LDA(MEM_READ(address)); break;
        case 0xB1: ADDR_IZY_R(); LDA(MEM_READ(address)); break;

        case 0xA2: LDX(FETCH_BYTE()); break;
        case 0xA6: ADDR_ZP();    LDX(ZP_READ(address)); break;
        case 0xB6: ADDR_ZPY();   LDX(ZP_READ(address)); break;
        case 0xAE: ADDR_ABS();   LDX(MEM_READ(address)); break;
        case 0xBE: ADDR_ABY_R(); LDX(MEM_READ(address)); break;

        case 0xA0: LDY(FETCH_BYTE()); break;
        case 0xA4: ADDR_ZP();    LDY(ZP_READ(address)); break;
        case 0xB4: ADDR_ZPX();   LDY(ZP_READ(address)); break;
        case 0xAC: ADDR_ABS();   LDY(MEM_READ(address)); break;
        case 0xBC: ADDR_ABX_R(); LDY(MEM_READ(address)); break;

        /**
         * Stores
         */
        case 0x85: ADDR_ZP();  ZP_WRITE(address, r_a); break;
        case 0x95: ADDR_ZPX(); ZP_WRITE(address, r_a); break;
        case 0x8D: ADDR_ABS(); MEM_WRITE(address, r_a); break;
        case 0x9D: ADDR_ABX_W(); MEM_WRITE(address, r_a); break;
        case 0x99: ADDR_ABY_W(); MEM_WRITE(address, r_a); break;
        case 0x81: ADDR_IZX(); MEM_WRITE(address, r_a); break;
        case 0x91: ADDR_IZY_W(); MEM_WRITE(address, r_a); break;

        case 0x86: ADDR_ZP();  ZP_WRITE(address, r_x); break;
        case 0x96: ADDR_ZPY(); ZP_WRITE(address, r_x); break;
        case 0x8E: ADDR_ABS(); MEM_WRITE(address, r_x); break;

        case 0x84: ADDR_ZP();  ZP_WRITE(address, r_y); break;
        case 0x94: ADDR_ZPX(); ZP_WRITE(address, r_y); break;
        case 0x8C: ADDR_ABS(); MEM_WRITE(address, r_y); break;

        /**
         * Register transfers
         */
        case 0xAA: LDX(r_a); break;              // TAX
        case 0xA8: LDY(r_a); break;              // TAY
        case 0x8A: LDA(r_x); break;              // TXA
        case 0x98: LDA(r_y); break;              // TYA
        case 0xBA: LDX(r_sp); break;             // TSX
        case 0x9A: r_sp = r_x; break; // TXS

        /**
         * Stack
         */
        case 0x48: PUSH_BYTE(r_a); break;                            // PHA
        case 0x08: PUSH_BYTE(r_p | FLAG_B | FLAG_R); break;          // PHP
        case 0x68: LDA(POP_BYTE()); break;                                          // PLA
        case 0x28: {                                                  // PLP
            uint8_t status_before = r_p;
            r_p = (POP_BYTE() & ~FLAG_B) | FLAG_R;
            IRQ_POLL_BEFORE(status_before);
        } break;

        /**
         * Logical / arithmetic
         */
        case 0x09: ORA(FETCH_BYTE()); break;
        case 0x05: ADDR_ZP();    ORA(ZP_READ(address)); break;
        case 0x15: ADDR_ZPX();   ORA(ZP_READ(address)); break;
        case 0x0D: ADDR_ABS();   ORA(MEM_READ(address)); break;
        case 0x1D: ADDR_ABX_R(); ORA(MEM_READ(address)); break;
        case 0x19: ADDR_ABY_R(); ORA(MEM_READ(address)); break;
        case 0x01: ADDR_IZX();   ORA(MEM_READ(address)); break;
        case 0x11: ADDR_IZY_R(); ORA(MEM_READ(address)); break;

        case 0x29: AND(FETCH_BYTE()); break;
        case 0x25: ADDR_ZP();    AND(ZP_READ(address)); break;
        case 0x35: ADDR_ZPX();   AND(ZP_READ(address)); break;
        case 0x2D: ADDR_ABS();   AND(MEM_READ(address)); break;
        case 0x3D: ADDR_ABX_R(); AND(MEM_READ(address)); break;
        case 0x39: ADDR_ABY_R(); AND(MEM_READ(address)); break;
        case 0x21: ADDR_IZX();   AND(MEM_READ(address)); break;
        case 0x31: ADDR_IZY_R(); AND(MEM_READ(address)); break;

        case 0x49: EOR(FETCH_BYTE()); break;
        case 0x45: ADDR_ZP();    EOR(ZP_READ(address)); break;
        case 0x55: ADDR_ZPX();   EOR(ZP_READ(address)); break;
        case 0x4D: ADDR_ABS();   EOR(MEM_READ(address)); break;
        case 0x5D: ADDR_ABX_R(); EOR(MEM_READ(address)); break;
        case 0x59: ADDR_ABY_R(); EOR(MEM_READ(address)); break;
        case 0x41: ADDR_IZX();   EOR(MEM_READ(address)); break;
        case 0x51: ADDR_IZY_R(); EOR(MEM_READ(address)); break;

        case 0x24: ADDR_ZP();  BIT_TEST(ZP_READ(address)); break;
        case 0x2C: ADDR_ABS(); BIT_TEST(MEM_READ(address)); break;

        case 0x69: ADC(FETCH_BYTE()); break;
        case 0x65: ADDR_ZP();    ADC(ZP_READ(address)); break;
        case 0x75: ADDR_ZPX();   ADC(ZP_READ(address)); break;
        case 0x6D: ADDR_ABS();   ADC(MEM_READ(address)); break;
        case 0x7D: ADDR_ABX_R(); ADC(MEM_READ(address)); break;
        case 0x79: ADDR_ABY_R(); ADC(MEM_READ(address)); break;
        case 0x61: ADDR_IZX();   ADC(MEM_READ(address)); break;
        case 0x71: ADDR_IZY_R(); ADC(MEM_READ(address)); break;

        case 0xE9: SBC(FETCH_BYTE()); break;
        case 0xE5: ADDR_ZP();    SBC(ZP_READ(address)); break;
        case 0xF5: ADDR_ZPX();   SBC(ZP_READ(address)); break;
        case 0xED: ADDR_ABS();   SBC(MEM_READ(address)); break;
        case 0xFD: ADDR_ABX_R(); SBC(MEM_READ(address)); break;
        case 0xF9: ADDR_ABY_R(); SBC(MEM_READ(address)); break;
        case 0xE1: ADDR_IZX();   SBC(MEM_READ(address)); break;
        case 0xF1: ADDR_IZY_R(); SBC(MEM_READ(address)); break;

        case 0xC9: CMP(FETCH_BYTE()); break;
        case 0xC5: ADDR_ZP();    CMP(ZP_READ(address)); break;
        case 0xD5: ADDR_ZPX();   CMP(ZP_READ(address)); break;
        case 0xCD: ADDR_ABS();   CMP(MEM_READ(address)); break;
        case 0xDD: ADDR_ABX_R(); CMP(MEM_READ(address)); break;
        case 0xD9: ADDR_ABY_R(); CMP(MEM_READ(address)); break;
        case 0xC1: ADDR_IZX();   CMP(MEM_READ(address)); break;
        case 0xD1: ADDR_IZY_R(); CMP(MEM_READ(address)); break;

        case 0xE0: CPX(FETCH_BYTE()); break;
        case 0xE4: ADDR_ZP();  CPX(ZP_READ(address)); break;
        case 0xEC: ADDR_ABS(); CPX(MEM_READ(address)); break;

        case 0xC0: CPY(FETCH_BYTE()); break;
        case 0xC4: ADDR_ZP();  CPY(ZP_READ(address)); break;
        case 0xCC: ADDR_ABS(); CPY(MEM_READ(address)); break;

        /**
         * Increments / decrements
         */
        case 0xE6: ADDR_ZP();  RMW_ZP(INC_V); break;
        case 0xF6: ADDR_ZPX(); RMW_ZP(INC_V); break;
        case 0xEE: ADDR_ABS(); RMW(INC_V); break;
        case 0xFE: ADDR_ABX_M(); RMW(INC_V); break;
        case 0xE8: INC_V(r_x); break;    // INX
        case 0xC8: INC_V(r_y); break;    // INY

        case 0xC6: ADDR_ZP();  RMW_ZP(DEC_V); break;
        case 0xD6: ADDR_ZPX(); RMW_ZP(DEC_V); break;
        case 0xCE: ADDR_ABS(); RMW(DEC_V); break;
        case 0xDE: ADDR_ABX_M(); RMW(DEC_V); break;
        case 0xCA: DEC_V(r_x); break;    // DEX
        case 0x88: DEC_V(r_y); break;    // DEY

        /**
         * Shifts / rotates
         */
        case 0x0A: ASL_V(r_a); break;
        case 0x06: ADDR_ZP();  RMW_ZP(ASL_V); break;
        case 0x16: ADDR_ZPX(); RMW_ZP(ASL_V); break;
        case 0x0E: ADDR_ABS(); RMW(ASL_V); break;
        case 0x1E: ADDR_ABX_M(); RMW(ASL_V); break;

        case 0x4A: LSR_V(r_a); break;
        case 0x46: ADDR_ZP();  RMW_ZP(LSR_V); break;
        case 0x56: ADDR_ZPX(); RMW_ZP(LSR_V); break;
        case 0x4E: ADDR_ABS(); RMW(LSR_V); break;
        case 0x5E: ADDR_ABX_M(); RMW(LSR_V); break;

        case 0x2A: ROL_V(r_a); break;
        case 0x26: ADDR_ZP();  RMW_ZP(ROL_V); break;
        case 0x36: ADDR_ZPX(); RMW_ZP(ROL_V); break;
        case 0x2E: ADDR_ABS(); RMW(ROL_V); break;
        case 0x3E: ADDR_ABX_M(); RMW(ROL_V); break;

        case 0x6A: ROR_V(r_a); break;
        case 0x66: ADDR_ZP();  RMW_ZP(ROR_V); break;
        case 0x76: ADDR_ZPX(); RMW_ZP(ROR_V); break;
        case 0x6E: ADDR_ABS(); RMW(ROR_V); break;
        case 0x7E: ADDR_ABX_M(); RMW(ROR_V); break;

        /**
         * Jumps / subroutines / interrupts
         */
        // JMP abs
        case 0x4C:
            ADDR_ABS();
            // a jump to itself (the usual "wait for NMI" idle loop) does nothing but burn 3 cycles per pass, and
            // nothing can interrupt it before the batch ends, so run all the passes to the end of the batch at once
            // (unless an NMI is already pending, delayed by one instruction, or an IRQ the I flag lets through: one
            // raised just before the jump's poll is taken after it)
#ifdef FRNES_HOST
            // (not while tracing: the trace shows every pass)
            if ((address == (uint16_t)(r_pc - 3)) && !cpu_registers -> NMI_Request && !(cpu_registers -> IRQ_Request && !(r_p & FLAG_I)) && !cpu_host_trace) {
#else
            if ((address == (uint16_t)(r_pc - 3)) && !cpu_registers -> NMI_Request && !(cpu_registers -> IRQ_Request && !(r_p & FLAG_I))) {
#endif
                uint32_t remaining = budget - cycles_done;
                // (not past the point a DMC request predicted since the batch was sized, by the DMA before it, is
                // handed over to cpu_exact.c at: the assembly core's budget stops there, see cpu_run)
                if (apu_dmc_request_due) {
                    int32_t window = (int32_t)(apu_dmc_request_time - CPU_EXACT_LEAD - (cpu_cycle_total + cycles_done));
                    if (window < (int32_t)remaining) {
                        remaining = (window > 0) ? (uint32_t)window : 0;
                    }
                }
                if (remaining > 3) {
                    cycles = ((remaining + 2) / 3) * 3;
                }
            }
            r_pc = address;
            break;

        // JMP (ind), the pointer high byte is fetched without carrying into the page
        case 0x6C:
            ADDR_ABS();
            r_pc = IO_READ_AT(address, 3) | (IO_READ_AT((address & 0xff00) | ((address + 1) & 0x00ff), 4) << 8);
            break;

        // JSR abs, pushes the address of the last byte of the instruction. The high byte of the target is read
        // after the push, so a JSR whose push overwrites its own operand jumps through the new byte (as hardware)
        case 0x20:
            address = FETCH_BYTE();
            PUSH_WORD(r_pc);
            address |= FETCH_BYTE() << 8;
            r_pc = address;
            break;

        // RTS
        case 0x60:
            word_value = POP_BYTE();
            word_value |= POP_BYTE() << 8;
            r_pc = word_value + 1;
            break;

        // RTI
        case 0x40:
            r_p = (POP_BYTE() & ~FLAG_B) | FLAG_R;
            word_value = POP_BYTE();
            word_value |= POP_BYTE() << 8;
            r_pc = word_value;
            break;

        // BRK, skips the padding byte after the opcode
        case 0x00:
            r_pc++;
            PUSH_WORD(r_pc);
            PUSH_BYTE(r_p | FLAG_B | FLAG_R);
            SET_FLAG(FLAG_I);
            r_pc = INTERRUPT_VECTOR(VECTOR_IRQ, cycles_done);
            break;

        /**
         * Branches
         */
        case 0x10: BRA(!(r_p & FLAG_N)); break;  // BPL
        case 0x30: BRA(r_p & FLAG_N); break;     // BMI
        case 0x50: BRA(!(r_p & FLAG_V)); break;  // BVC
        case 0x70: BRA(r_p & FLAG_V); break;     // BVS
        case 0x90: BRA(!(r_p & FLAG_C)); break;  // BCC
        case 0xB0: BRA(r_p & FLAG_C); break;     // BCS
        case 0xD0: BRA(!(r_p & FLAG_Z)); break;  // BNE
        case 0xF0: BRA(r_p & FLAG_Z); break;     // BEQ

        /**
         * Flags
         */
        case 0x18: RESET_FLAG(FLAG_C); break;   // CLC
        case 0x38: SET_FLAG(FLAG_C); break;     // SEC
        case 0x58: IRQ_POLL_BEFORE(r_p); RESET_FLAG(FLAG_I); break;   // CLI
        case 0x78: IRQ_POLL_BEFORE(r_p); SET_FLAG(FLAG_I); break;     // SEI
        case 0xB8: RESET_FLAG(FLAG_V); break;   // CLV
        case 0xD8: RESET_FLAG(FLAG_D); break;   // CLD
        case 0xF8: SET_FLAG(FLAG_D); break;     // SED

        // NOP
        case 0xEA: break;

        /**
         * Unofficial opcodes
         */
        // NOPs, the memory forms still perform their read
        case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA: break;
        case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2: r_pc++; break;
        case 0x04: case 0x44: case 0x64: ADDR_ZP(); break;
        case 0x14: case 0x34: case 0x54: case 0x74: case 0xD4: case 0xF4: ADDR_ZPX(); break;
        case 0x0C: ADDR_ABS(); MEM_READ(address); break;
        case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC: ADDR_ABX_R(); MEM_READ(address); break;

        // SLO
        case 0x07: ADDR_ZP();  RMW_ZP(SLO_V); break;
        case 0x17: ADDR_ZPX(); RMW_ZP(SLO_V); break;
        case 0x0F: ADDR_ABS(); RMW(SLO_V); break;
        case 0x1F: ADDR_ABX_M(); RMW(SLO_V); break;
        case 0x1B: ADDR_ABY_M(); RMW(SLO_V); break;
        case 0x03: ADDR_IZX(); RMW(SLO_V); break;
        case 0x13: ADDR_IZY_M(); RMW(SLO_V); break;

        // RLA
        case 0x27: ADDR_ZP();  RMW_ZP(RLA_V); break;
        case 0x37: ADDR_ZPX(); RMW_ZP(RLA_V); break;
        case 0x2F: ADDR_ABS(); RMW(RLA_V); break;
        case 0x3F: ADDR_ABX_M(); RMW(RLA_V); break;
        case 0x3B: ADDR_ABY_M(); RMW(RLA_V); break;
        case 0x23: ADDR_IZX(); RMW(RLA_V); break;
        case 0x33: ADDR_IZY_M(); RMW(RLA_V); break;

        // SRE
        case 0x47: ADDR_ZP();  RMW_ZP(SRE_V); break;
        case 0x57: ADDR_ZPX(); RMW_ZP(SRE_V); break;
        case 0x4F: ADDR_ABS(); RMW(SRE_V); break;
        case 0x5F: ADDR_ABX_M(); RMW(SRE_V); break;
        case 0x5B: ADDR_ABY_M(); RMW(SRE_V); break;
        case 0x43: ADDR_IZX(); RMW(SRE_V); break;
        case 0x53: ADDR_IZY_M(); RMW(SRE_V); break;

        // RRA
        case 0x67: ADDR_ZP();  RMW_ZP(RRA_V); break;
        case 0x77: ADDR_ZPX(); RMW_ZP(RRA_V); break;
        case 0x6F: ADDR_ABS(); RMW(RRA_V); break;
        case 0x7F: ADDR_ABX_M(); RMW(RRA_V); break;
        case 0x7B: ADDR_ABY_M(); RMW(RRA_V); break;
        case 0x63: ADDR_IZX(); RMW(RRA_V); break;
        case 0x73: ADDR_IZY_M(); RMW(RRA_V); break;

        // DCP
        case 0xC7: ADDR_ZP();  RMW_ZP(DCP_V); break;
        case 0xD7: ADDR_ZPX(); RMW_ZP(DCP_V); break;
        case 0xCF: ADDR_ABS(); RMW(DCP_V); break;
        case 0xDF: ADDR_ABX_M(); RMW(DCP_V); break;
        case 0xDB: ADDR_ABY_M(); RMW(DCP_V); break;
        case 0xC3: ADDR_IZX(); RMW(DCP_V); break;
        case 0xD3: ADDR_IZY_M(); RMW(DCP_V); break;

        // ISC
        case 0xE7: ADDR_ZP();  RMW_ZP(ISC_V); break;
        case 0xF7: ADDR_ZPX(); RMW_ZP(ISC_V); break;
        case 0xEF: ADDR_ABS(); RMW(ISC_V); break;
        case 0xFF: ADDR_ABX_M(); RMW(ISC_V); break;
        case 0xFB: ADDR_ABY_M(); RMW(ISC_V); break;
        case 0xE3: ADDR_IZX(); RMW(ISC_V); break;
        case 0xF3: ADDR_IZY_M(); RMW(ISC_V); break;

        // SAX
        case 0x87: ADDR_ZP();  ZP_WRITE(address, r_a & r_x); break;
        case 0x97: ADDR_ZPY(); ZP_WRITE(address, r_a & r_x); break;
        case 0x8F: ADDR_ABS(); MEM_WRITE(address, r_a & r_x); break;
        case 0x83: ADDR_IZX(); MEM_WRITE(address, r_a & r_x); break;

        // LAX
        case 0xA7: ADDR_ZP();    LAX(ZP_READ(address)); break;
        case 0xB7: ADDR_ZPY();   LAX(ZP_READ(address)); break;
        case 0xAF: ADDR_ABS();   LAX(MEM_READ(address)); break;
        case 0xBF: ADDR_ABY_R(); LAX(MEM_READ(address)); break;
        case 0xA3: ADDR_IZX();   LAX(MEM_READ(address)); break;
        case 0xB3: ADDR_IZY_R(); LAX(MEM_READ(address)); break;

        // ANC, carry is copied from the N flag
        case 0x0B: case 0x2B:
            AND(FETCH_BYTE());
            r_p = (r_p & ~FLAG_C) | (r_a >> 7);
            break;

        // ALR
        case 0x4B:
            r_a &= FETCH_BYTE();
            LSR_V(r_a);
            break;

        // ARR, AND then ROR A with C from bit 6 and V from bit 6 xor bit 5
        case 0x6B:
            r_a = ((r_a & FETCH_BYTE()) >> 1) | ((r_p & FLAG_C) << 7);
            r_p = (r_p & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                NZ(r_a) |
                ((r_a >> 6) & FLAG_C) |
                (((r_a >> 6) ^ (r_a >> 5)) & 1 ? FLAG_V : 0);
            break;

        // AXS, X = (A & X) - immediate, flags as CMP
        case 0xCB:
            previous_status = r_a & r_x;
            COMPARE(previous_status, FETCH_BYTE());
            r_x = previous_status - byte_value;
            break;

        // SBC immediate duplicate
        case 0xEB: SBC(FETCH_BYTE()); break;

        // ANE (unstable)
        case 0x8B: LDA((r_a | UNSTABLE_MAGIC) & r_x & FETCH_BYTE()); break;

        // LXA (unstable)
        // (LXA's "magic" constant is taken as $FF: A = X = immediate, as Blargg's instr_test and Mesen 2 have it)
        case 0xAB: LAX(FETCH_BYTE()); break;

        // LAE / LAS, A = X = SP = memory & SP
        case 0xBB:
            ADDR_ABY_R();
            r_sp &= MEM_READ(address);
            LAX(r_sp);
            break;

        // SHA
        case 0x93: ADDR_IZY_W(); SH_STORE(r_a & r_x); break;
        case 0x9F: ADDR_ABY_W(); SH_STORE(r_a & r_x); break;
        // SHX
        case 0x9E: ADDR_ABY_W(); SH_STORE(r_x); break;
        // SHY
        case 0x9C: ADDR_ABX_W(); SH_STORE(r_y); break;
        // SHS / TAS, SP = A & X then stored like SHA
        case 0x9B:
            ADDR_ABY_W();
            r_sp = r_a & r_x;
            SH_STORE(r_sp);
            break;

        // JAM opcodes (0x02, 0x12, ... 0xF2) halt the CPU and fall through to here

        default:
            // leave PC on the opcode so the caller can report it
            r_pc--;
            cycles_done = CPU_HALTED;
            goto done;
    }

#ifdef FRNES_HOST
    // (a $2002 poll loop's iterations skipped by its read: the instruction takes them, ppu_poll_skip)
    if (__builtin_expect(cpu_registers -> READ_SKIP != 0, 0)) {
        cycles += cpu_registers -> READ_SKIP;
        cpu_registers -> READ_SKIP = 0;
    }
#endif
    cycles_done += cycles;
    cpu_instruction_end = cpu_cycle_total + cycles_done;
#ifdef CPU_C_ONLY
    if (cpu_batch_cut) {
        break;
    }
#endif
    }

done:
    cpu_registers -> PC = r_pc;
    cpu_registers -> A = r_a;
    cpu_registers -> X = r_x;
    cpu_registers -> Y = r_y;
    cpu_registers -> SP = r_sp;
    cpu_registers -> S = r_p;
    if (cycles_done != CPU_HALTED) {
        cpu_cycle_total += cycles_done;
    }
    return cycles_done;
}
#endif

#if defined(FRNES_HOST) && !defined(CPU_C_ONLY)
uint32 cpu_run_c_host(uint32 budget) {
    return cpu_run_c(budget);
}
#endif

#ifndef CPU_C_ONLY
/**
 * The SH4 assembly core (6502_core.s) runs the most frequent opcodes; whenever it stops before the budget is used
 * up (an opcode it doesn't implement, code outside the RAM / ROM regions it fetches directly, a pending interrupt)
 * the C core runs one instruction and the assembly core takes over again. Build with CPU_C_ONLY for the C core alone.
 */
uint32 cpu_run_asm(uint32 budget);
#ifndef FRNES_HOST
// (cpu_exact.c's DMC fast path) the opcodes the assembly core runs: the ones its dispatch table doesn't send to
// op_fallback (the table holds $80-$FF, then $00-$7F)
extern void* const cpu_asm_dispatch_table[256];
extern const char cpu_asm_op_fallback[];
int cpu_asm_runs(uint8_t opcode) {
    return cpu_asm_dispatch_table[opcode ^ 0x80] != (const void*)cpu_asm_op_fallback;
}
#endif

/*
 * What the assembly core leaves (an interrupt, the poll after CLI / SEI / PLP, BRK, unofficial opcodes, code outside
 * the regions it fetches from, an instruction a DMA lands in): one instruction or interrupt sequence in cpu_exact.c,
 * after the boundary's poll (unless cpu_exact.c made it already). The player has no C core; the C core is the
 * reference in CPU_C_ONLY and host builds.
 */
static FRNES_HOT(exact_fallback) __attribute__((cold)) uint32 cpu_run_fallback() {
    int interrupt = 0;
    if (cpu_exact_polled) {
        // cpu_exact.c made this boundary's poll: the next instruction runs without one, in the assembly core unless
        // an interrupt is pending (it only starts with none; one cpu_exact.c saw too late is taken after this
        // instruction, by cpu_exact.c)
        cpu_exact_polled = 0;
        cpu_poll_early = 0;
        cpu_registers -> IRQ_Poll = 0;
        if (cpu_registers -> NMI_Request) {
            cpu_registers -> NMI_Request = 1;
        } else if (!(cpu_registers -> IRQ_Request && !(cpu_registers -> S & FLAG_I)) && !cpu_exact_due(cpu_cycle_total)) {
            return 0;
        }
    } else {
        interrupt = cpu_boundary_poll(cpu_registers -> S, cpu_cycle_total, cpu_registers -> PC, cpu_registers -> X) != 0;
    }
#ifdef FRNES_HOST
    if (cpu_host_trace && !interrupt) {
        cpu_host_trace(cpu_registers -> PC, cpu_registers -> A, cpu_registers -> X, cpu_registers -> Y, cpu_registers -> S,
            cpu_registers -> SP, cpu_cycle_total);
    }
#endif
    // (outside a DMA the instruction ends at its boundary and the next poll is cpu_boundary_poll's, as with the C core;
    // a DMA's instructions and interrupt sequences keep cpu_exact.c's own poll)
    uint8_t status_before = cpu_registers -> S;
    uint8_t opcode = 0;
    // (not with an NMI pending, nor for BRK: cpu_exact.c's own sequence takes an NMI that comes up during BRK's vector
    // fetch after the handler's first instruction, as the C core's INTERRUPT_VECTOR_RACE does)
    // (the opcode is only peeked where the window maps memory: code in the registers / open bus runs with
    // cpu_exact.c's own poll)
    uint16_t pc = cpu_registers -> PC;
    if (!interrupt && !cpu_exact_due(cpu_cycle_total) && !cpu_registers -> NMI_Request &&
        (!IS_IO_INSTRUCTION(pc) || IS_DIRECT_6000(pc)) && ((opcode = NES_WINDOW_BYTE(pc)) != 0x00)) {
        interrupt = CPU_EXACT_DEFER;
    }
    // an instruction ahead of a DMC request, or the one its DMA lands in: the assembly core runs it when the DMA's
    // halt can be applied as a stall (cpu_dma_fast)
    if (!interrupt && !cpu_registers -> NMI_Request && cpu_exact_due(cpu_cycle_total)) {
        int32_t stall = cpu_dma_fast(cpu_cycle_total);
        if (stall >= 0) {
            uint32 cycles = cpu_run_asm(1);
            if (cycles) {
                cycles += stall;
                cpu_cycle_total += cycles;
                cpu_instruction_end = cpu_cycle_total;
                return cycles;
            }
            // (not reached: cpu_dma_fast only takes instructions the assembly core runs)
            cpu_cycle_total += stall;
            return stall;
        }
    }
    uint32 cycles = cpu_exact_run(cpu_cycle_total, interrupt);
    if (cycles == CPU_HALTED) {
        return CPU_HALTED;
    }
    // CLI / SEI / PLP: their poll still saw the I flag from before them (IRQ_POLL_BEFORE in the C core)
    if ((interrupt == CPU_EXACT_DEFER) && ((opcode == 0x58) || (opcode == 0x78) || (opcode == 0x28)) &&
        cpu_registers -> IRQ_Request) {
        cpu_registers -> IRQ_Poll = (status_before & FLAG_I) ? IRQ_POLL_I_SET : IRQ_POLL_I_CLEAR;
    }
    cpu_cycle_total += cycles;
    cpu_instruction_end = cpu_cycle_total;
    return cycles;
}

FRNES_HOT(2_run) uint32 cpu_run(uint32 budget) {
    uint32 cycles_done = 0;
    cpu_batch_end = cpu_cycle_total + budget;
    while (cycles_done < budget) {
        // the C core takes the interrupt poll left over from CLI / SEI / PLP (the assembly core only looks at I),
        // a poll cpu_exact.c made, and the instructions a DMA lands in
        if (!cpu_registers -> IRQ_Poll && !cpu_exact_polled && !cpu_exact_due(cpu_cycle_total)) {
            // (the assembly core only stops at its budget: not past the point the next DMC request has to be handed
            // over at, which an instruction run cycle by cycle can have moved since the batch was sized)
            uint32 asm_budget = budget - cycles_done;
            if (apu_dmc_request_due) {
                uint32 window = apu_dmc_request_time - CPU_EXACT_LEAD - cpu_cycle_total;
                if (window < asm_budget) {
                    asm_budget = window;
                }
            }
            // (the assembly core only sets cpu_poll_early when a taken branch ends its batch: it describes the batch's
            // last instruction, or the one before the batch when the batch ran nothing)
            uint8_t poll_early = cpu_poll_early;
            cpu_poll_early = 0;
            uint32 cycles = cpu_run_asm(asm_budget);
            cpu_cycle_total += cycles;
            cycles_done += cycles;
            if (cycles) {
                cpu_instruction_end = cpu_cycle_total;
            } else {
                cpu_poll_early = poll_early;
            }
            if ((cycles_done >= budget) || cpu_batch_cut) {
                break;
            }
            // (an access that raised an interrupt line ends the assembly core's run, also when the IRQ is masked: with
            // nothing for cpu_exact.c to do, the assembly core goes on)
            if (cycles && !cpu_registers -> NMI_Request && !cpu_registers -> IRQ_Poll && !cpu_exact_polled &&
                !(cpu_registers -> IRQ_Request && !(cpu_registers -> S & FLAG_I)) && !cpu_exact_due(cpu_cycle_total)) {
                continue;
            }
        }
        uint32 cycles = cpu_run_fallback();
        if (cycles == CPU_HALTED) {
            return CPU_HALTED;
        }
        cycles_done += cycles;
        if (cpu_batch_cut) {
            break;
        }
    }
    cpu_batch_cut = 0;
    return cycles_done;
}
#endif

// (save states: state_core.c, out of the link time optimized code, has this file's chunk)
