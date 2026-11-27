# Building FrNES

FrNES builds against mainline [KallistiOS](https://github.com/KallistiOS/KallistiOS) with the stock SH4 toolchain
(`sh-elf-gcc` 15.2). The build takes KOS from the environment: source the `environ.sh` of the KOS you want, then
`make`. `KOS_BASE` picks the KOS, and `KOS_PORTS` the kos-ports (libpng, zlib, bzip2).

Two recipes, both from KOS commit `d81aa3fd` (mainline master, 2026-10-27):

## Release builds: mainline KOS, three patches, LTO

What the released FrNES binaries are built with.

1. Mainline KOS `d81aa3fd` with these patches from `tools/patches/`, in this order:
   - `kos-libkosfat-long-name-search.patch`, `kos-libkosfat-readdir.patch`, `kos-libkosfat-utf8-name-search.patch`:
     libkosfat bugs the SD card browser hits (long names, directories past their first cluster, multi-byte names);
     upstream as KallistiOS PR #1567.
   - `kos-mmu_set_sq_addr.patch`: `mmu_set_sq_addr` (called by `sq_lock` whenever the MMU is on: every frame for the
     TA, and for texture and sound loads) reloads the store queues' UTLB entries 0x3e / 0x3f with LDTLB instead of
     writing the UTLB data array from cached code, which left stray TLB values in RAM.
2. `environ.sh` from `doc/environ.sh.sample` with these `KOS_CFLAGS` changes:
   - `-O3` (for `-O2`)
   - `-flto=auto -ffat-lto-objects` (the commented line, enabled)
   - `-freorder-blocks-algorithm=simple`
   - `-fno-omit-frame-pointer` (for `-fomit-frame-pointer`)
3. Build KOS (`make` in `$KOS_BASE`, including `addons`), then FrNES (`make release`, `make frontend`).

With KOS built as LTO objects, FrNES's link optimizes KOS's per-frame code (interrupts, PVR lists, store queues)
together with the emulator's and places it in the "kos" hot slot (`tools/hot_kos.txt`).

## Stock KOS builds: no KOS changes, no LTO

For building FrNES on an unmodified KOS. About 0.8 % slower than a release build (busy time per frame, 0.4-1.4 % across
the release player A/B's 9 games, 2026-11-05).

1. Mainline KOS `d81aa3fd`, plus the three libkosfat patches above until PR #1567 is merged (without them the SD
   card browser misses files with long or multi-byte names, and lists only part of large directories).
2. `environ.sh` as above, but without `-flto=auto -ffat-lto-objects`.
3. Build KOS, then FrNES, as above.

FrNES itself does what the MMU part of the old KOS branch (`MMU_modifications`) did, in `kos_mainline.c` (cold, built
without LTO):

- `frnes_kos_map_static` (`memory.c` maps its pages with it): `mmu_page_map_static`, returning the UTLB entry it loaded
  (read back from MMUCR.URB), which `memory.c` keeps a shadow of for its page remaps; mainline returns 0. A plain call
  from a non-LTO file: a link `--wrap` of `mmu_page_map_static` didn't take with LTO KOS, where LTO had already inlined
  KOS's copy into FrNES's callers.
- `--wrap=mmu_set_sq_addr` (`Makefile`): the LDTLB reload of entries 0x3e / 0x3f, as `kos-mmu_set_sq_addr.patch` does
  in KOS.

`--wrap` only redirects calls between separately compiled objects, so it only takes effect when KOS isn't built with
LTO (with LTO, KOS's own `mmu_set_sq_addr` is used, which is why the release recipe patches it). Against a KOS that
already has the patch it's harmless.

## Notes

- KOS's `env/environ_dreamcast.sh` sets `-matomic-model=soft-gusa` (mainline); both recipes keep it.
- `/opt/toolchains/dc/kos` stays the default when `KOS_BASE` isn't set.
