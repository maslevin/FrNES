#!/usr/bin/env python3
"""Post-link check on a build's map.txt: writable .data stays small, and the per-frame .bss buffers sit at their pinned
operand cache offsets.

  python3 tools/data_budget.py [--hot-rodata] BUILD/map.txt

(--hot-rodata: also report hot tables that moved within .rodata and APU buffers that moved within .bss; the
AccuracyCoin builds, which HOT_RODATA's and APU_BSS's offsets are from: the release build orders them differently)

.bss follows .data, and .bss holds the hot buffers (the frame buffer, the APU log and ring, the VRC7 block): every byte
.data grows moves them onto other operand cache sets (the 16 kB cache is indexed by address bits 13:5). So:
- significant static tables and constants are `static const` with FRNES_HOT_RODATA or FRNES_COLD_RODATA (memory.h),
  and generated tables (tools/gen_*.py) are emitted const: .data must stay under DATA_BUDGET. FRNES_COLD_RODATA's
  tables (the game database, read at ROM load only) are the last thing in .rodata, so they move no hot table
- .rodata starts at RODATA_AT modulo 16 kB (tools/shlelf_hot.xc), relative to .hotdata like the .bss pins, and the hot
  tables in it (HOT_RODATA) are reported if they move from where they were in 72b0dea's AccuracyCoin build
- the frame buffer starts .bss at FRAME_AT modulo 16 kB (tools/bss_pins.ld, relative to .hotdata's offset, which the
  A/B layouts move), and in the AccuracyCoin builds the APU's per-frame buffers after it (APU_BSS) are reported if
  they move
"""
import os
import re
# (.hotdata's start at FRNES_HOT_DATA_OFFSET into its block: __frnes_hot_data_block, before a data placement's
# sections (tools/shlelf_hot.xc); __frnes_hot_data_start in a map from before it)
import sys

DATA_BUDGET = 0x4000
# the frame buffer's offset modulo 16 kB at HOT_DATA_OFFSET 0x800 (tools/bss_pins.ld), and where the APU's per-frame
# buffers follow it in .bss at 72b0dea (reported in the AccuracyCoin builds when they move)
FRAME_AT = 0x3820
APU_BSS = {"chunk.0": 0x04e0, "ring": 0x0700, "opll_block": 0x0920, "vrc7": 0x2530, "write_log": 0x2748}
# .rodata's start modulo 16 kB at HOT_DATA_OFFSET 0x800 (tools/shlelf_hot.xc), and the hot tables in it: offset from
# the start of the tables (.rodata's start plus FRNES_RODATA_OFFSET): tools/shlelf_hot.xc places them first, in this order
RODATA_AT = 0x0808
HOT_RODATA = {"ppu_nibble_spread": 0x0, "flags.0": 0x40, "ex_plan_mode": 0x44, "apu_length_table": 0x144,
              "sequence_4": 0x164, "sequence_5": 0x174, "dmc_rate_table": 0x188, "noise_period_table": 0x1a8,
              "duty_table": 0x1c8, "apu_frame_step_type": 0x278, "apu_frame_steps": 0x284}


def rodata_errors(text, tables_too):
    """(errors, warnings) for .rodata's pinned start and the hot tables in it"""
    hot = re.search(r"^\s+0x([0-9a-f]+)\s+__frnes_hot_data_block = \.", text, re.M) or re.search(r"^\s+0x([0-9a-f]+)\s+__frnes_hot_data_start = \.", text, re.M)
    rodata = re.search(r"^\.rodata\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)", text, re.M)
    knob = re.search(r"^\s+0x([0-9a-f]+)\s+FRNES_RODATA_OFFSET = 0x", text, re.M)
    if not hot or not rodata:
        return ["no .hotdata / .rodata in the map"], []
    shift = (int(hot.group(1), 16) & 0x3fff) - 0x800
    start = int(rodata.group(1), 16)
    if (start & 0x3fff) != ((RODATA_AT + shift) & 0x3fff):
        return [".rodata starts at 0x%x, 0x%x modulo 16 kB instead of 0x%x (tools/shlelf_hot.xc)" % (start, start & 0x3fff, (RODATA_AT + shift) & 0x3fff)], []
    tables = start + (int(knob.group(1), 16) if knob else 0)
    warnings = []
    if not tables_too:
        return [], []
    for name, offset in sorted(HOT_RODATA.items(), key=lambda i: i[1]):
        found = re.findall(r"^ \.rodata\.%s(?:\.lto_priv\.\d+)?\s*\n?\s+0x([0-9a-f]+)\s+0x[0-9a-f]+" % re.escape(name), text, re.M)
        if len(found) == 1 and int(found[0], 16) - tables != offset:
            warnings.append("hot table .rodata.%s at +0x%x in .rodata instead of +0x%x: on other operand cache sets than at "
                            "72b0dea (HOT_RODATA)" % (name, int(found[0], 16) - tables, offset))
    return [], warnings


def bss_pin_errors(text, ac_checks):
    """(errors, warnings) for the frame buffer's pinned offset and the APU buffers after it"""
    hot = re.search(r"^\s+0x([0-9a-f]+)\s+__frnes_hot_data_block = \.", text, re.M) or re.search(r"^\s+0x([0-9a-f]+)\s+__frnes_hot_data_start = \.", text, re.M)
    if not hot:
        return ["no .hotdata in the map"], []
    shift = (int(hot.group(1), 16) & 0x3fff) - 0x800

    def where(name):
        found = re.findall(r"^ \.bss\.%s(?:\.lto_priv\.\d+)?\s*\n?\s+0x([0-9a-f]+)\s+0x[0-9a-f]+" % re.escape(name), text, re.M)
        return int(found[0], 16) if len(found) == 1 else None
    frame = where("ppu_framebuffer")
    if frame is None:
        return [".bss.ppu_framebuffer isn't in the build once: renamed? (tools/bss_pins.ld)"], []
    if (frame & 0x3fff) != ((FRAME_AT + shift) & 0x3fff):
        return [".bss.ppu_framebuffer at 0x%x, 0x%x modulo 16 kB instead of 0x%x (tools/bss_pins.ld)" % (frame, frame & 0x3fff, (FRAME_AT + shift) & 0x3fff)], []
    warnings = []
    for name, offset in sorted(APU_BSS.items(), key=lambda i: i[1]) if ac_checks else []:
        address = where(name)
        if address is not None and (address & 0x3fff) != ((offset + shift) & 0x3fff):
            warnings.append(".bss.%s at 0x%x modulo 16 kB instead of 0x%x: on other operand cache sets than at 72b0dea "
                            "(APU_BSS; something before it in .bss changed size)" % (name, address & 0x3fff, (offset + shift) & 0x3fff))
    return [], warnings

def rodata_room(text):
    """(the bytes between the code and .rodata's fixed address, that address), or None without .rodata"""
    rodata = re.search(r"^\.rodata\s+0x([0-9a-f]+)\s+0x[0-9a-f]+", text, re.M)
    if not rodata:
        return None
    start = int(rodata.group(1), 16)
    ends = [int(a, 16) + int(z, 16) for a, z in re.findall(r"^\.\S+\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)", text, re.M)
            if 0x8c010000 <= int(a, 16) < start]
    return (start - max(ends) if ends else 0), start


def hotdata_room(text):
    """(the bytes between the end of .rodata and what follows it, .frnes_cold_text's cold code among it, and .hotdata's
    fixed address, that address), or None
    without .hotdata / .rodata (tools/shlelf_hot.xc: .hotdata FRNES_RODATA_SPAN past the .rodata window)"""
    hotdata = re.search(r"^\.hotdata\s+0x([0-9a-f]+)\s+0x[0-9a-f]+", text, re.M)
    rodata = re.search(r"^\.rodata\s+0x([0-9a-f]+)\s+0x[0-9a-f]+", text, re.M)
    if not (hotdata and rodata):
        return None
    start, rodata_at = int(hotdata.group(1), 16), int(rodata.group(1), 16)
    ends = [int(a, 16) + int(z, 16) for a, z in re.findall(r"^\.\S+\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)", text, re.M)
            if rodata_at <= int(a, 16) < start]
    return (start - max(ends) if ends else 0), start


def headroom_warnings(text):
    """a warning when the room before .rodata's fixed address (the Makefile's RODATA_WINDOW_*) is under 4 kB: the link
    stops when the code reaches it"""
    room = rodata_room(text)
    if room and room[0] < 0x1000:
        return ["only 0x%x bytes between the code and .rodata's fixed address (0x%x): a little more code stops the link "
                "(raise the Makefile's RODATA_WINDOW_* for this build by 0x4000 and A/B it)" % room]
    return []


def bss_order_warnings(map_path, text):
    """a warning for each section the build's bss_order.ld (tools/bss_order.py, copied next to the map) pins that the
    link doesn't have: renamed or gone, its pin does nothing (and its old place stays empty)"""
    order = os.path.join(os.path.dirname(map_path), "bss_order.ld")
    if not os.path.exists(order):
        return []
    have = set(re.sub(r"\.lto_priv\.\d+$", "", n) for n in re.findall(r"^ (\.bss\.\S+)", text, re.M))
    pinned = re.findall(r"^\*\((\.bss\.\S+) ", open(order).read(), re.M)
    missing = [n for n in pinned if n not in have]
    if missing:
        return ["%d .bss section(s) tools/bss_order pins aren't in the link (renamed or gone: tools/bss_order_regen.sh): %s"
                % (len(missing), " ".join(missing[:8]))]
    return []


def heap_errors(text):
    """the heap's start (_end) at FRNES_HEAP_AT modulo 16 kB, relative to .hotdata's offset (tools/shlelf_hot.xc)"""
    end = re.search(r"^\s+0x([0-9a-f]+)\s+_end = \.", text, re.M)
    at = re.search(r"^\s+0x([0-9a-f]+)\s+FRNES_HEAP_AT = ", text, re.M)
    hot = re.search(r"^\s+0x([0-9a-f]+)\s+__frnes_hot_data_block = \.", text, re.M) or re.search(r"^\s+0x([0-9a-f]+)\s+__frnes_hot_data_start = \.", text, re.M)
    if not (end and at and hot):
        return ["no _end / FRNES_HEAP_AT in the map: the heap pin (tools/shlelf_hot.xc, Makefile) is gone"]
    shift = (int(hot.group(1), 16) & 0x3fff) - 0x800
    want = (int(at.group(1), 16) + shift) & 0x3fff
    if (int(end.group(1), 16) & 0x3fff) != want:
        return ["_end (the heap's start) at 0x%x, 0x%x modulo 16 kB instead of 0x%x (FRNES_HEAP_AT)" % (
            int(end.group(1), 16), int(end.group(1), 16) & 0x3fff, want)]
    return []


def main():
    hot_rodata = "--hot-rodata" in sys.argv
    map_path = [a for a in sys.argv[1:] if a != "--hot-rodata"][0]
    text = open(map_path).read()
    data = re.search(r"^\.data\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)", text, re.M)
    if not data:
        sys.exit("data_budget.py: no .data in %s" % sys.argv[1])
    data_size = int(data.group(2), 16)
    errors = []
    if data_size > DATA_BUDGET:
        errors.append(".data is 0x%x bytes, over its 0x%x budget: a large table that isn't const (make it static const "
                      "with FRNES_HOT_RODATA / FRNES_COLD_RODATA, memory.h)" % (data_size, DATA_BUDGET))
    bss_e, bss_w = bss_pin_errors(text, hot_rodata)
    errors += bss_e
    for w in bss_w:
        print("data_budget.py: warning: %s" % w)
    errors += heap_errors(text)
    for w in bss_order_warnings(map_path, text):
        print("data_budget.py: warning: %s" % w)
    room = rodata_room(text)
    if room:
        print("data_budget.py: 0x%x bytes (%.1f kB) of room before .rodata at 0x%x" % (room[0], room[0] / 1024.0, room[1]))
    room = hotdata_room(text)
    if room:
        print("data_budget.py: 0x%x bytes (%.1f kB) of room after .rodata and the cold code, before .hotdata at 0x%x"
              % (room[0], room[0] / 1024.0, room[1]))
        if room[0] < 0x1000:
            print("data_budget.py: warning: only 0x%x bytes between .rodata's end and the cold code (.frnes_cold_text) "
                  "and .hotdata at 0x%x: a little more cold code or a cold table more stops the link of a build with a "
                  "fixed .rodata window (raise FRNES_RODATA_SPAN, tools/shlelf_hot.xc, by 0x4000 and A/B it)" % room)
    for w in headroom_warnings(text):
        print("data_budget.py: warning: %s" % w)
    rodata_e, rodata_w = rodata_errors(text, hot_rodata)
    errors += rodata_e
    for w in rodata_w:
        print("data_budget.py: warning: %s" % w)
    for e in errors:
        print("data_budget.py: %s" % e)
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
