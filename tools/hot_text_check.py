#!/usr/bin/env python3
"""Fails the link when a function tools/hot_text.txt (profiled hot) or tools/hot_kos.txt (the "kos" slot's) lists lies
outside the hot slots:

  python3 tools/hot_text_check.py ELF MAP

Each listed function the build has (any LTO copy of it: .constprop / .isra / .part / .lto_priv) must start inside one of
the slots' ranges (tools/hot_slots.py); the ones the build doesn't have are skipped."""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hot_slots_fit  # noqa: E402

CLONE = re.compile(r"\.(lto_priv|constprop|isra|part|cold)\.\d+.*$")


def main():
    elf, map_path = sys.argv[1], sys.argv[2]
    listed = [l.strip() for f in ("hot_text.txt", "hot_kos.txt")
              for l in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), f))
              if l.strip() and not l.startswith(("#", "*"))]
    slots = hot_slots_fit.slot_ranges(map_path)
    copies = {}
    for line in subprocess.run([hot_slots_fit.NM, elf], capture_output=True, text=True).stdout.splitlines():
        p = line.split()
        if len(p) == 3 and p[1] in "tT":
            copies.setdefault(CLONE.sub("", p[2].lstrip("_")), []).append((int(p[0], 16), p[2]))
    outside = []
    for name in listed:
        for address, symbol in copies.get(name.lstrip("_"), []):
            if not any(start <= address < end for start, end in slots.values()):
                outside.append("%s (%s at 0x%x)" % (name, symbol, address))
    if outside:
        print("*** tools/hot_text_check.py: profiled hot functions outside the hot slots: %s. Put each in a slot "
              "(FRNES_HOT(group_name), memory.h, or tools/hot_frame.txt) ***" % ", ".join(outside))
        sys.exit(1)
    conflicts = epilogue_conflicts(elf)
    if conflicts:
        print("*** tools/hot_text_check.py: read_byte_6502_c's return (lds.l @r15+,pr .. rts and its delay slot) shares "
              "I-cache sets with the assembly core's WRAM / I/O access path: %s. Every $6000-$7FFF read evicts its caller's "
              "lines (5e03084f: Lagrange Point +8 %%); move read_byte_6502_c in the read slot (tools/hot_slots.py: the read slot's "
              "'. = __frnes_slot_read + OFFSET' after ppu_status_read_sync) ***" % ", ".join(conflicts))
        sys.exit(1)


# the assembly core's labels whose code calls read_byte_6502_c for a WRAM / I/O read (6502_core.s), each up to the next
# label
ASM_ACCESS = ("rd_abs", "wr_abs", "io_read")
OBJDUMP = "/opt/toolchains/dc/sh-elf/bin/sh-elf-objdump"


def ic_sets(start, end):
    """the 8 kB direct mapped I-cache's sets (address bits 12:5) of [start, end)"""
    return set(((a >> 5) & 0xff) for a in range(start & ~31, end, 32))


def epilogue_conflicts(elf):
    """read_byte_6502_c's epilogues' I-cache sets that the assembly core's access path has too (none without it: the C
    core's builds)"""
    syms = []
    for line in subprocess.run([hot_slots_fit.NM, "-n", "-S", elf], capture_output=True, text=True).stdout.splitlines():
        p = line.split()
        if len(p) >= 3 and p[-2] in "tT":
            syms.append((int(p[0], 16), int(p[1], 16) if len(p) == 4 else 0, p[-1]))
    by_name = {name.lstrip("_"): (address, size) for address, size, name in syms}
    if "read_byte_6502_c" not in by_name or not all(n in by_name for n in ASM_ACCESS):
        return []
    starts = sorted(set(a for a, _, _ in syms))
    asm = {}
    for name in ASM_ACCESS:
        address = by_name[name][0]
        following = [a for a in starts if a > address]
        asm[name] = ic_sets(address, following[0] if following else address + 32)
    address, size = by_name["read_byte_6502_c"]
    text = subprocess.run([OBJDUMP, "-d", "--start-address=0x%x" % address, "--stop-address=0x%x" % (address + size), elf],
                          capture_output=True, text=True).stdout
    insns = []
    for line in text.splitlines():
        m = re.match(r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} ){2}\s*(\S+)\s*(.*)$", line)
        if m:
            insns.append((int(m.group(1), 16), m.group(2), m.group(3)))
    out = []
    for i, (a, op, args) in enumerate(insns):
        if op == "lds.l" and args.startswith("@r15+,pr"):
            rts = next((b for b, o, _ in insns[i:i + 4] if o == "rts"), None)
            if rts is None:
                continue
            sets = ic_sets(a, rts + 4)
            for name, s in asm.items():
                if sets & s:
                    out.append("epilogue at 0x%x with %s (sets %s)" % (a, name, " ".join("0x%02x" % x for x in sorted(sets & s))))
    return out


if __name__ == "__main__":
    main()
