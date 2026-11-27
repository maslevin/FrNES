/*
 * kos_compat.h - what KallistiOS master no longer declares that the emulator still uses: the fixed width type names
 * (uint8 / uint16 / uint32 / uint64 and the signed ones) and arch_real_exit's prototype. Included in every Dreamcast build by the
 * Makefile (-include); the host build's host/kos.h defines the same.
 */
#ifndef FRNES_KOS_COMPAT_H
#define FRNES_KOS_COMPAT_H
#include <stdint.h>
#include <stdlib.h>
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
// (still in KOS, its prototype now private to kernel/arch/dreamcast/kernel/init.c: the exit that skips the atexit
// handlers and KOS's shutdown, for the fatal paths)
void arch_real_exit(int ret_code) __attribute__((noreturn));
#endif
