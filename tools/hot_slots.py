#!/usr/bin/env python3
"""Writes tools/hot_slots.ld, the hot code's fixed slots (included at the start of .text by tools/shlelf_hot.xc).

  python3 tools/hot_slots.py [OUT.ld]

Each slot holds some hot input sections at a fixed position in the caches: its start is congruent to OFFSET modulo
16 kB (the operand cache's size, and twice the 8 kB direct mapped instruction cache), so its code keeps its I-cache
sets and its literal pools keep their OC sets. A slot starts at the first such address after the previous slot's
BUDGET (in the order of packed(), which wastes the least space), whatever the previous slot's code actually takes, so a change that grows one slot within its budget moves no
other slot, and the ASSERT stops the link when a slot outgrows its budget (raise BUDGET here, or move code out of the
hot group). Between slots the space up to the next start is padding.

The layout knobs (Makefile, passed as --defsym) move the whole slotted block: FRNES_HOT_CODE_OFFSET,
FRNES_HOT_CORE_PAD and FRNES_HOT_MID_PAD are all added to every slot's offset. The slots' positions against each other
are the layout's design (chosen from profiles, tools/hot_slots_fit.py), not where code happened to land, so the A/B
layouts (tools/ab_layouts.py) vary only where the hot code sits against the data, .rodata, KOS and the cold code;
before the slots, CORE_PAD and MID_PAD moved groups against each other (a slotted layout's A/B with that meaning
measured layouts that are never built: step 4c's fitted offsets, -11 % on SMB 1-1 at its layout, read +11 % at layout 3).
FRNES_HOT_SLOTS_LOOSE=1 turns the budget checks off for diagnostic builds (stats, profiling), whose hot code is
bigger: a slot over its budget then pushes the following ones along.

The multi-function slots list their sections with SORT_BY_NAME and each function has a section of its own
(FRNES_HOT(group_function), memory.h), so their order doesn't depend on how LTO splits the program into partitions.
The OFFSETs come from the layout before the slots (perf-ready 41e68c0) and are meant to be chosen from PC sample
profiles (tools/icmap.py).
"""
import os
import sys

# the per-frame slot's families, in their order in the slot (tools/hot_frame.txt)
FRAME_FAMILIES = ("ppu_", "apu_", "quarter_frame", "half_frame", "mmc3_", "map_nametables", "video_frame_", "stream_callback")


def kos_members(path=os.path.join(os.path.dirname(os.path.abspath(__file__)), "hot_kos.txt")):
    """tools/hot_kos.txt's functions, in its order"""
    return [l.strip() for l in open(path) if l.strip() and not l.startswith("#")]


def kos_statements():
    """the "kos" slot's input section statements: per name its section, its LTO clones' and GCC's .text.unlikely. ones,
    each sorted by name (not by partition); a line starting with * as it stands (an archive member)"""
    for name in kos_members():
        if name.startswith("*"):
            yield name
        else:
            for pattern in (".text.%s", ".text.%s.*", ".text.unlikely.%s", ".text.unlikely.%s.*"):
                yield "*(SORT_BY_NAME(%s))" % (pattern % name)


# functions the frame slot keeps at a size, padded up to it when smaller: apu_synth_frame at 0x914, its size at
# b28ab3cd in the test builds (where it's in this slot; the player builds put it in zz_main's), so the 16 functions after
# it keep their places: the FPU synth's N163 / FDS paths change its sample loop's code (2026-11-10, abFPU: the shifted
# functions cost ~1-2 % of SMB's emulation time on 3 of the 6 layouts)
FRAME_KEEP = {"apu_synth_frame": 0x914,
              # (mmc3_write: 0x2a4 at b28ab3cd in every build; in the player builds of the sound options it came out
              # 0x14 smaller, an LTO inlining difference, moving video_frame_begin / end, stream_callback and the others
              # after it in every layout: SMB's player +1.4 %)
              "mmc3_write": 0x2a4,
              # (ppu_sync_lazy_write: 0x1a4 at a2968a90 in every build; the emphasis-only $2001 test made it smaller)
              "ppu_sync_lazy_write": 0x1a4,
              # (ppu_restore_line_start: 0x21c in the test builds at a2968a90, 0x214 in the release; 4 smaller in the
              # test builds with the v switch, register allocation. A pair: the test builds' size, the release's, told
              # apart by the test builds' ac_console_flush)
              "ppu_restore_line_start": (0x21c, 0x214)}
# functions of the slot placed at its end, after LIBGCC_HOT, the size they had kept free where they were, so that
# what follows keeps its place whatever their size: the lazy line's switch paths, which the mid-line v switch
# (perf-midline-v) changed, LTO laying them out anew (0x6b8 / 0x2bc at a2968a90 in the test builds; the release's
# 0x6b8 / 0x2bc too). Both run only on lines with a fine X / sprite / v switch, about once a frame in most games
# (__frnes_slot_frame_ppu_end, the frame display's PPU range, doesn't take them in)
FRAME_TAIL = {"ppu_render_line_switches": 0x6b8,
              "ppu_replay_switches": 0x2bc}


def frame_statements():
    """the "frame" slot's input section statements, and __frnes_slot_frame_ppu_end after its PPU functions (the frame
    display's PC samples, frame_hud.c, take the slot's start up to it as the PPU's slow path)"""
    members = frame_members()
    for i, name in enumerate(members):
        if name in FRAME_TAIL:
            yield ". = . + 0x%x;" % FRAME_TAIL[name]
            continue
        if name in FRAME_KEEP:
            # (when it's in this slot: the size it had, so what follows keeps its place)
            yield "__frnes_frame_keep_%s = .;" % name
            yield "*(.text.%s .text.%s.*)" % (name, name)
            size = keep_size(FRAME_KEEP[name])
            yield ". = (. > __frnes_frame_keep_%s) ? MAX(., __frnes_frame_keep_%s + %s) : .;" % (name, name, size)
            continue
        yield "*(.text.%s .text.%s.*)" % (name, name)
        if name.startswith("ppu_") and not (i + 1 < len(members) and members[i + 1].startswith("ppu_")):
            yield "__frnes_slot_frame_ppu_end = .;"


# the timer slot's first function (SORT_BY_NAME's first, timer_cpu_run) at its own statement, padded to the size it had,
# so the slot's others keep their places: at a2968a90 0x7cc in the test builds (cpu_irq_update_line inline), 0x7e4 in
# the release (called); perf-midline-v's PPU code moved LTO's partitions' bounds, and the release took it inline too
TIMER_KEEP = {"timer_cpu_run": (0x7cc, 0x7e4)}


def keep_size(size):
    """a KEEP size: a number, or a pair, the test builds' size and the release's, told apart by the test builds'
    ac_console_flush"""
    if isinstance(size, tuple):
        return "(DEFINED(_ac_console_flush) ? 0x%x : 0x%x)" % size
    return "0x%x" % size


def timer_keep_statements():
    for name in sorted(TIMER_KEEP):
        yield "__frnes_timer_keep_%s = .;" % name
        yield "*(.text.hot.%s)" % name
        yield ". = (. > __frnes_timer_keep_%s) ? MAX(., __frnes_timer_keep_%s + %s) : .;" % (name, name,
                                                                                            keep_size(TIMER_KEEP[name]))


def frame_tail_statements():
    """FRAME_TAIL's functions, at the frame slot's end"""
    for name in frame_members():
        if name in FRAME_TAIL:
            yield "*(.text.%s .text.%s.*)" % (name, name)


def frame_members(path=os.path.join(os.path.dirname(os.path.abspath(__file__)), "hot_frame.txt")):
    """tools/hot_frame.txt's functions, in the slot's order: by family, then by name"""
    names = [l.strip() for l in open(path) if l.strip() and not l.startswith("#")]
    family = lambda n: next((i for i, f in enumerate(FRAME_FAMILIES) if n.startswith(f)), len(FRAME_FAMILIES))
    return sorted(names, key=lambda n: (family(n), n))


# libgcc's helpers that profile hot (count trailing zeros, the division the divide table serves): their archive
# members' code, last in the frame slot (they're not -ffunction-sections, so a member at a time)
LIBGCC_HOT = ["*libgcc.a:_ctzsi2.o(.text)", "*libgcc.a:_div_table.o(.text)"]

# (name, input sections, offset modulo 16 kB, budget, knobs added to the offset)
# (every slot moves by all three knobs: see above)
CORE = RUN = REST = "FRNES_HOT_CODE_OFFSET + FRNES_HOT_CORE_PAD + FRNES_HOT_MID_PAD"
SLOTS = [
    # the assembly core: dispatch, the hot opcode handlers, the I/O paths (its rare opcodes are in .text.cpu_cold)
    ("core", "*(.text.hot.1_core)", 0x1be0, 0x1900, CORE),
    # cpu_run, and the device catch-up at register accesses and batch ends
    ("run", "*(.text.hot.2_run)", 0x0ce0, 0x0300, RUN),
    ("sync", "*(.text.hot.3_sync)", 0x1800, 0x1300, RUN),
    ("sync_ppu", "*(SORT_BY_NAME(.text.hot.3_sync_ppu_*))", 0x2280, 0x0600, RUN),
    # the PPU per scanline: the renderer, sprite evaluation, the MMC3's per scanline clock, fetches, rows
    ("render", "*(.text.hot.4_render)", 0x2a60, 0x0d80, REST),
    ("evaluate", "*(.text.hot.5_evaluate)", 0x3160, 0x0400, REST),
    ("mmc3", "*(SORT_BY_NAME(.text.hot.5_mmc3_*))", 0x3ee0, 0x0e00, REST),
    ("fetch", "*(.text.hot.6_fetch)", 0x16c0, 0x0480, REST),
    ("row", "*(.text.hot.7_row)", 0x0e40, 0x0280, REST),
    # the C read / write handlers (register accesses)
    # (read_byte_6502_c at a fixed offset in the slot, 0xc0, after ppu_status_read_sync: as LTO placed them at
    # ec375925, read_byte_6502_c at 0x74, its hot path shared the assembly core's I-cache lines (set_region): Lagrange
    # Point's WRAM reads, +60k I-cache stall cycles a frame, +3 % in the player. Measured, 0x20 steps over 0x80-0x700:
    # Lagrange's I-cache freeze cycles a frame 119k at 0xc0 (12709970's 118k), 127k-130k at 0x4a0-0x4e0, up to 305k;
    # read_byte_6502_c first and ppu_status_read_sync after, 234k)
    ("read", "*(.text.hot.8_read_status)\n. = __frnes_slot_read + 0x90;\n*(.text.hot.8_read)", 0x0320, 0x0e80, REST),
    ("write", "*(.text.hot.9_write)", 0x1220, 0x2f00, REST),
    # cpu_exact.c's path (the fallback behind the assembly core, DMAs, the DMC fast path's bus plan): rare since the
    # fast path, but a few times a frame in DMC games
    ("exact", "*(SORT_BY_NAME(.text.hot.exact_*))", 0x3e80, 0x3e00, REST),
    # the mappers' CPU cycle counters (VRC, FME-7, FDS) and the expansion sound, per batch in those games
    # (__frnes_slot_timer_expansion: where the expansion sound's code starts, after the counters', for the frame display's
    # PC samples, frame_hud.c; the order is the one SORT_BY_NAME gives all of them)
    ("timer", "\n".join(list(timer_keep_statements())) +
     "\n*(SORT_BY_NAME(.text.hot.timer_c*))\n__frnes_slot_timer_expansion = .;\n*(SORT_BY_NAME(.text.hot.timer_[!c]*))",
     0x3140, 0x1a00, REST),
    # the frame loop (main.c, or test_main.c in test builds)
    ("main", "*(.text.hot.zz_main)", 0x3f80, 0x4000, REST),
    # the rest of the per-frame code (PPU / APU helpers, the synthesizer, video, MMC3 bank switching): the functions
    # tools/hot_frame.txt lists, one statement each (sections sharing a statement are placed in input file order, i.e.
    # by LTO partition)
    ("frame", "\n".join(list(frame_statements()) + LIBGCC_HOT + list(frame_tail_statements())), 0x3098, 0x6000, REST),
    # the C core's interpreter (CPU_C_ONLY builds: cpu.c's C cpu_run, 37 kB, 43 % of the C core's samples) and what it
    # calls through memory.h's read_word, and the bank switching both cores share (set_6502_bank_pointer); last, so it
    # moves no other slot, and in the assembly core's builds it holds only the bank switching
    ("c", "*(SORT_BY_NAME(.text.hot.c_*))", 0x1000, 0x9800, REST),
    # the per-frame code outside the hot groups (KOS's interrupt and frame code, the emulator's helpers, the common
    # boards: tools/hot_kos.txt), one statement each by section name (with GCC's .text.unlikely. for the ones it guesses
    # cold), a function's LTO clones after it, sorted by name (not by
    # partition); after the "c" slot, so it moves no other slot,
    # and the cold code that grows (boards, mappers) comes after it
    ("kos", "\n".join(kos_statements()), 0x0260, 0x7000, REST),
]
WINDOW = 0x4000
# slots placed after all the others whatever their offset, in this order (the "c" and "kos" slots: added last, they move no
# other slot)
LAST = ("c", "kos")


def packed():
    """The slots in the order they take the least memory in (with the knobs at 0): the core first, then each time the
    one starting closest after the previous one's budget. The order only decides the addresses, not the cache
    positions, and any order is correct (a slot that doesn't fit before its position goes one 16 kB window on)."""
    last = [slot for slot in SLOTS if slot[0] in LAST]
    rest, end, order = [slot for slot in SLOTS[1:] if slot[0] not in LAST], SLOTS[0][2] + SLOTS[0][3], [SLOTS[0]]
    while rest:
        best = min(rest, key=lambda slot: (slot[2] - end) % WINDOW)
        rest.remove(best)
        order.append(best)
        end += (best[2] - end) % WINDOW + best[3]
    order += last
    return order


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "tools/hot_slots.ld"
    lines = ["/* generated by tools/hot_slots.py (see there): the hot code's fixed slots. Edit the table there and run it. */",
             "FRNES_HOT_SLOTS_LOOSE = DEFINED(FRNES_HOT_SLOTS_LOOSE) ? FRNES_HOT_SLOTS_LOOSE : 0;",
             # (section relative, as . is here: the 16 kB boundary at or before the end of the startup code, the start
             # of .text; each symbol is assigned once, ld evaluates the script in several passes)
             "__frnes_hot_base = . - (ABSOLUTE(.) & 0x%x);" % (WINDOW - 1),
             "__frnes_slot_start_end = .;"]
    previous = "__frnes_slot_start_end"
    for name, sections, offset, budget, knobs in packed():
        assert 0 <= offset < WINDOW and budget % 0x20 == 0, name
        slot = "__frnes_slot_%s" % name
        # the first address at or after the previous slot's end congruent to the offset (plus the knobs) modulo 16 kB
        lines += [
            "%s_at = __frnes_hot_base + ((0x%x + %s) & 0x%x);" % (slot, offset, knobs, WINDOW - 1),
            "%s = %s_at + ((((%s - %s_at) + 0x%x) & ~0x%x) - 0x%x);" % (slot, slot, previous, slot, 2 * WINDOW - 1,
                                                                      WINDOW - 1, WINDOW),
            ". = %s;" % slot,
            sections,
            "ASSERT(FRNES_HOT_SLOTS_LOOSE || (. - %s <= 0x%x), \"hot slot %s over its 0x%x byte budget (tools/hot_slots.py)\");" % (
                slot, budget, name, budget),
            "%s_end = MAX(%s + 0x%x, .);" % (slot, slot, budget),
        ]
        previous = slot + "_end"
    # (a hot group not in the table: after the slots, unchecked; then the rest of the code from a 16 kB boundary, so
    # its cache positions don't depend on the slots' offsets either)
    lines += [". = %s;" % previous, "*(SORT_BY_NAME(.text.hot.*))",
              ". = __frnes_hot_base + (((. - __frnes_hot_base) + 0x%x) & ~0x%x);" % (WINDOW - 1, WINDOW - 1)]
    open(out, "w").write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
