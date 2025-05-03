#include "6502.h"

#include <stdlib.h>

#include "pNesX.h"
#include "K6502_rw.h"

#define RAM_ADDR (void*)0x7c000000

uint8* generate_random_buffer(uint32 buffer_length) {
    uint8* buffer = (uint8*)malloc(buffer_length);
    uint32 index;
    for (index = 0; index < buffer_length; index++) {
        buffer[index] = rand() % 0x000000ff;
    }
    return buffer;
}

const char* PROFILING_MODE_STRINGS[] = {
    "No Mode (0x00)",
    "Operand Read Accesses", // 0x01
    "Operand Write Accesses", // 0x02
    "UTLB Misses", // 0x03
    "Operand Cache Read Misses",
    "Operand Cache Write Misses",
    "Instruction Fetches",
    "Instruction TLB Misses",
    "Instruction Cache Misses",
    "Operand Accesses",
    "Instruction Fetches",
    "On Chip RAM Operand Accesses",
    "No Mode (0x0C)", 
    "On Chip IO Accesses",
    "Operand Accesses",
    "Operand Cache Misses",
    "Branches Issued",
    "Branches Taken",
    "Subroutines Issued",
    "Instructions Issued",
    "Parallel Instructions Issued",
    "FPU Instructions Issued",
    "Interrupts",
    "NMIs",
    "TRAPA Instructions",
    "UBC A Matches",
    "UBC B Matches",
    "No Mode (0x1B)",
    "No Mode (0x1C)",
    "No Mode (0x1D)",
    "No Mode (0x1E)",
    "No Mode (0x1F)",
    "No Mode (0x20)",
    "Instruction Cache Fills",
    "Operand Cache Fills",
    "Elapsed Time",
    "Pipeline Freezes by Instruction Cache Misses",
    "Pipeline Freezes by Data Cache Misses",
    "No Mode",
    "Pipeline Freezes by Branch Mode",
    "Pipeline Freezes by CPU Register Mode",
    "Pipeline Freezes by FPU Mode"
};

void test_read_byte_c() {
    uint8* RAM_pattern = generate_random_buffer(0x800);
    memcpy(RAM, RAM_pattern, 0x800);

    uint8* WRAM_pattern = generate_random_buffer(0x2000);
    uint8* ROMBANK0_pattern = generate_random_buffer(0x2000);
    uint8* ROMBANK1_pattern = generate_random_buffer(0x2000);
    uint8* ROMBANK2_pattern = generate_random_buffer(0x2000);
    uint8* ROMBANK3_pattern = generate_random_buffer(0x2000);

    // TODO: when changing the bank pointers, we just need to change them in the ROMBANKS array, the global values should not be needed eventually
    ROMBANK_WRAM = WRAM_pattern;
    ROMBANK0 = ROMBANK0_pattern;
    ROMBANK1 = ROMBANK1_pattern;
    ROMBANK2 = ROMBANK2_pattern;
    ROMBANK3 = ROMBANK3_pattern;

    printf("Test 6502 Read Byte Support\n");
//    reset_6502();

    struct timespec startTime;
    struct timespec endTime;
    clock_gettime(CLOCK_MONOTONIC, &startTime);

    uint32 addr;
    bool RAM_test_passed = true;
    for (addr = 0; addr < 0x2000; addr++) {
        uint8 byte_test = K6502_Read(addr);
        if (byte_test != RAM_pattern[addr % 0x800]) {
//            printf("ERROR: byte at [0x%04X] mismatch of pattern - RAM [0x%02x] - Pattern [0x%02x]\n", (uint16)addr, byte_test, RAM_pattern[addr % 0x800]);
            RAM_test_passed = false;
        }
    }

    bool WRAM_test_passed = true;
    for (addr = 0x6000; addr < 0x8000; addr++) {
        uint8 byte_test = K6502_Read(addr);
        if (byte_test != WRAM_pattern[addr % 0x2000]) {
//            printf("ERROR: byte at [0x%04X] mismatch of pattern - WRAM [0x%02x] - Pattern [0x%02x]\n", (uint16)addr, byte_test, WRAM_pattern[addr % 0x2000]);
            WRAM_test_passed = false;
        }
    }

    bool ROMBANK0_test_passed = true;
    for (addr = 0x8000; addr < 0xa000; addr++) {
        uint8 byte_test = K6502_Read(addr);
        if (byte_test != ROMBANK0_pattern[addr % 0x2000]) {
//            printf("ERROR: byte at [0x%04X] mismatch of pattern - ROMBANK0 [0x%02x] - Pattern [0x%02x]\n", (uint16)addr, byte_test, ROMBANK0_pattern[addr % 0x2000]);
            ROMBANK0_test_passed = false;
        }
    }

    bool ROMBANK1_test_passed = true;
    for (addr = 0xa000; addr < 0xc000; addr++) {
        uint8 byte_test = K6502_Read(addr);
        if (byte_test != ROMBANK1_pattern[addr % 0x2000]) {
//            printf("ERROR: byte at [0x%04X] mismatch of pattern - ROMBANK1 [0x%02x] - Pattern [0x%02x]\n", (uint16)addr, byte_test, ROMBANK1_pattern[addr % 0x2000]);
            ROMBANK1_test_passed = false;
        }
    }

//    printf("Test Reading 6502 ROMBANK2 Region 0xc000-0xdfff\n");
    bool ROMBANK2_test_passed = true;
    for (addr = 0xc000; addr < 0xe000; addr++) {
        uint8 byte_test = K6502_Read(addr);
        if (byte_test != ROMBANK2_pattern[addr % 0x2000]) {
//            printf("ERROR: byte at [0x%04X] mismatch of pattern - ROMBANK2 [0x%02x] - Pattern [0x%02x]\n", (uint16)addr, byte_test, ROMBANK2_pattern[addr % 0x2000]);
            ROMBANK2_test_passed = false;
        }
    }

//    printf("Test Reading 6502 ROMBANK3 Region 0xe000-0xffff\n");
    bool ROMBANK3_test_passed = true;
    for (addr = 0xe000; addr <= 0xffff; addr++) {
        uint8 byte_test = K6502_Read(addr);
        if (byte_test != ROMBANK3_pattern[addr % 0x2000]) {
//            printf("ERROR: byte at [0x%04X] mismatch of pattern - ROMBANK3 [0x%02x] - Pattern [0x%02x]\n", (uint16)addr, byte_test, ROMBANK3_pattern[addr % 0x2000]);
            ROMBANK3_test_passed = false;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &endTime);

    uint64 executionTime = ((endTime.tv_sec * 1000000000ULL) + endTime.tv_nsec) - ((startTime.tv_sec * 1000000000ULL) + startTime.tv_nsec);
    printf("Test Execution took [%llu] nanoseconds\n", executionTime);
    printf("Test Reading 6502 RAM Region 0x0000-0x1fff: [%s]\n", RAM_test_passed ? "PASSED" : "FAILED");
    printf("Test Reading 6502 WRAM Region 0x6000-0x7fff: [%s]\n", WRAM_test_passed ? "PASSED" : "FAILED");
    printf("Test Reading 6502 ROMBANK0 Region 0x8000-0x9fff: [%s]\n", ROMBANK0_test_passed ? "PASSED" : "FAILED");
    printf("Test Reading 6502 ROMBANK1 Region 0xa000-0xbfff: [%s]\n", ROMBANK1_test_passed ? "PASSED" : "FAILED");
    printf("Test Reading 6502 ROMBANK2 Region 0xc000-0xdfff: [%s]\n", ROMBANK2_test_passed ? "PASSED" : "FAILED");
    printf("Test Reading 6502 ROMBANK3 Region 0xe000-0xffff: [%s]\n", ROMBANK3_test_passed ? "PASSED" : "FAILED"); 

    free(RAM_pattern);
    free(WRAM_pattern);
    free(ROMBANK0_pattern);
    free(ROMBANK1_pattern);
    free(ROMBANK2_pattern);
    free(ROMBANK3_pattern);
}

void run_multitest_test_read_byte() {
    uint8 profiling_modes_to_run[] = {
        PMCR_UTLB_MISS_MODE,
        PMCR_OPERAND_CACHE_READ_MISS_MODE,
        PMCR_OPERAND_CACHE_WRITE_MISS_MODE,
        PMCR_INSTRUCTION_TLB_MISS_MODE,
        PMCR_INSTRUCTION_CACHE_MISS_MODE,
        PMCR_PIPELINE_FREEZE_BY_ICACHE_MISS_MODE,
        PMCR_PIPELINE_FREEZE_BY_DCACHE_MISS_MODE,
        PMCR_PIPELINE_FREEZE_BY_BRANCH_MODE,
        PMCR_PIPELINE_FREEZE_BY_CPU_REGISTER_MODE,
        PMCR_PIPELINE_FREEZE_BY_FPU_MODE
    };

    for (uint8 profiling_mode_index = 0; profiling_mode_index < 10; profiling_mode_index++) {
        uint8 profiling_mode = profiling_modes_to_run[profiling_mode_index];
        printf("TEST PASS [%i] : [%s]\n", profiling_mode_index + 1, PROFILING_MODE_STRINGS[profiling_mode]);
        perf_cntr_clear(PRFC1);
        perf_cntr_start(PRFC1, profiling_mode, PMCR_COUNT_CPU_CYCLES);
        test_read_byte_c();        
        perf_cntr_stop(PRFC1);
        if (profiling_mode <= 0x1a) {
            printf("COUNTED [%llu] EVENTS\n", perf_cntr_count(PRFC1));
        } else {
            printf("COUNTED [%llu] CPU CYCLES LOST\n", perf_cntr_count(PRFC1));
        }
    }
}