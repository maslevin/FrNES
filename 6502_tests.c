#include "6502.h"

#include <stdlib.h>

#include "pNesX.h"

#define RAM_ADDR (void*)0x7c000000

uint8* generate_random_buffer(uint32 buffer_length) {
    uint8* buffer = (uint8*)malloc(buffer_length);
    uint32 index;
    for (index = 0; index < buffer_length; index++) {
        buffer[index] = rand() % 0x000000ff;
    }
    return buffer;
}

void test_read_byte() {
    uint8* RAM_pattern = generate_random_buffer(0x800);
    memcpy(RAM_ADDR, RAM_pattern, 0x800);

    uint8* WRAM_pattern = generate_random_buffer(0x2000);
    uint8* ROMBANK0_pattern = generate_random_buffer(0x2000);
    uint8* ROMBANK1_pattern = generate_random_buffer(0x2000);
    uint8* ROMBANK2_pattern = generate_random_buffer(0x2000);
    uint8* ROMBANK3_pattern = generate_random_buffer(0x2000);

    ROMBANK_WRAM = WRAM_pattern;
    ROMBANK0 = ROMBANK0_pattern;
    ROMBANK1 = ROMBANK1_pattern;
    ROMBANK2 = ROMBANK2_pattern;
    ROMBANK3 = ROMBANK3_pattern;

    printf("Test 6502 Read Byte Support\n");
    reset_6502();

    printf("Test Reading 6502 RAM Region 0x0000-0x1fff\n");
    uint32 addr;
    bool test_passed = true;
    for (addr = 0; addr < 0x2000; addr++) {
        uint8 byte_test = read_byte_6502(addr);
        if (byte_test != RAM_pattern[addr % 0x800]) {
            printf("ERROR: byte at [0x%04X] mismatch of pattern - RAM [0x%02x] - Pattern [0x%02x]\n", (uint16)addr, byte_test, RAM_pattern[addr % 0x800]);
            test_passed = false;
        }
    }

    if (test_passed) {
        printf("TEST PASSED\n");
    } else {
        printf("TEST FAILED\n");
    }    

    printf("Test Reading 6502 WRAM Region 0x6000-0x7fff\n");
    test_passed = true;
    for (addr = 0x6000; addr < 0x8000; addr++) {
        uint8 byte_test = read_byte_6502(addr);
        if (byte_test != WRAM_pattern[addr % 0x2000]) {
            printf("ERROR: byte at [0x%04X] mismatch of pattern - WRAM [0x%02x] - Pattern [0x%02x]\n", (uint16)addr, byte_test, WRAM_pattern[addr % 0x2000]);
            test_passed = false;
        }
    }

    if (test_passed) {
        printf("TEST PASSED\n");
    } else {
        printf("TEST FAILED\n");
    }    

    printf("Test Reading 6502 ROMBANK0 Region 0x8000-0x9fff\n");
    test_passed = true;
    for (addr = 0x8000; addr < 0xa000; addr++) {
        uint8 byte_test = read_byte_6502(addr);
        if (byte_test != ROMBANK0_pattern[addr % 0x2000]) {
            printf("ERROR: byte at [0x%04X] mismatch of pattern - ROMBANK0 [0x%02x] - Pattern [0x%02x]\n", (uint16)addr, byte_test, ROMBANK0_pattern[addr % 0x2000]);
            test_passed = false;
        }
    }

    if (test_passed) {
        printf("TEST PASSED\n");
    } else {
        printf("TEST FAILED\n");
    }

    printf("Test Reading 6502 ROMBANK1 Region 0xa000-0xbfff\n");
    test_passed = true;
    for (addr = 0xa000; addr < 0xc000; addr++) {
        uint8 byte_test = read_byte_6502(addr);
        if (byte_test != ROMBANK1_pattern[addr % 0x2000]) {
            printf("ERROR: byte at [0x%04X] mismatch of pattern - ROMBANK1 [0x%02x] - Pattern [0x%02x]\n", (uint16)addr, byte_test, ROMBANK1_pattern[addr % 0x2000]);
            test_passed = false;
        }
    }

    if (test_passed) {
        printf("TEST PASSED\n");
    } else {
        printf("TEST FAILED\n");
    }

    printf("Test Reading 6502 ROMBANK2 Region 0xc000-0xdfff\n");
    test_passed = true;
    for (addr = 0xc000; addr < 0xe000; addr++) {
        uint8 byte_test = read_byte_6502(addr);
        if (byte_test != ROMBANK2_pattern[addr % 0x2000]) {
            printf("ERROR: byte at [0x%04X] mismatch of pattern - ROMBANK2 [0x%02x] - Pattern [0x%02x]\n", (uint16)addr, byte_test, ROMBANK2_pattern[addr % 0x2000]);
            test_passed = false;
        }
    }

    if (test_passed) {
        printf("TEST PASSED\n");
    } else {
        printf("TEST FAILED\n");
    }    

    printf("Test Reading 6502 ROMBANK3 Region 0xe000-0xffff\n");
    test_passed = true;
    for (addr = 0xe000; addr <= 0xffff; addr++) {
        uint8 byte_test = read_byte_6502(addr);
        if (byte_test != ROMBANK3_pattern[addr % 0x2000]) {
            printf("ERROR: byte at [0x%04X] mismatch of pattern - ROMBANK3 [0x%02x] - Pattern [0x%02x]\n", (uint16)addr, byte_test, ROMBANK3_pattern[addr % 0x2000]);
            test_passed = false;
        }
    }

    if (test_passed) {
        printf("TEST PASSED\n");
    } else {
        printf("TEST FAILED\n");
    }

    free(RAM_pattern);
    free(WRAM_pattern);
    free(ROMBANK0_pattern);
    free(ROMBANK1_pattern);
    free(ROMBANK2_pattern);
    free(ROMBANK3_pattern);
}