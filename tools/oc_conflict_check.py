#!/usr/bin/env python3
"""Fails the link when a hot function's literal pool shares an operand cache set with the hot data it loads (in
.hotdata, or one of the hot tables in .rodata, tools/data_budget.py's HOT_RODATA).

  python3 tools/oc_conflict_check.py [--warn] ELF MAP

(--warn: report them without failing; the Makefile passes it for the A/B layouts, see OC_CHECK_FLAGS)

The operand cache is 16 kB, direct mapped (set = address bits 5-13), and SH4 code loads its constants PC relative
(mov.l @(disp,PC)), through the operand cache like any data. A literal that holds the address of a .hotdata variable
and sits in the same set as that variable evicts it on every load, and the variable then evicts the literal: each
call misses twice. (GCC 15's slot layout put 9 such pairs in mapper_ppu_clock, which cost MM3 3 % of its frame
time.) Which sets a function's literals land in depends on its slot's offset (tools/hot_slots.py), so a refit
(tools/hot_slots_fit.py, which avoids these) or a code change in a hot slot can bring a pair back; this check
catches it at link time.
"""
import collections
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hot_slots_fit  # noqa: E402

OC_SETS = 512
LINE = 32


def oc_set(address):
    return (address // LINE) % OC_SETS


def hot_data_range(elf):
    """.hotdata's start and end (the variables tools/hot_data.py places), from the ELF's symbols"""
    syms = {}
    for line in subprocess.run([hot_slots_fit.NM, elf], capture_output=True, text=True).stdout.splitlines():
        p = line.split()
        if len(p) == 3:
            syms[p[2]] = int(p[0], 16)
    return syms.get("__frnes_hot_data_start"), syms.get("__frnes_hot_data_end")


def literal_data_pairs(elf, start, end, data_start, data_end):
    """[(function, literal address, data address)] for the PC relative literal loads between start and end whose
    literal is the address of a variable in .hotdata"""
    text = subprocess.run([hot_slots_fit.OBJDUMP, "-d", "--start-address=0x%x" % start, "--stop-address=0x%x" % end, elf],
                          capture_output=True, text=True).stdout
    out, function = [], None
    for line in text.splitlines():
        m = re.match(r"^[0-9a-f]+ <(.*)>:$", line)
        if m:
            function = m.group(1)
            continue
        m = re.match(r"^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2} ){2}\s*mov\.l\s+([0-9a-f]+) <[^>]*>,r\d+\s*!\s*([0-9a-f]+)", line)
        if m:
            literal, value = int(m.group(1), 16), int(m.group(2), 16)
            if data_start <= value < data_end:
                out.append((function, literal, value))
    return out


def hot_rodata_ranges(elf):
    """[(start, end)] of the hot tables in .rodata (tools/data_budget.py's HOT_RODATA)"""
    import data_budget
    out = []
    for line in subprocess.run([hot_slots_fit.NM, "-S", elf], capture_output=True, text=True).stdout.splitlines():
        p = line.split()
        if len(p) == 4 and p[2] in "rR" and re.sub(r"\.lto_priv\.\d+$", "", p[3].lstrip("_")) in data_budget.HOT_RODATA:
            out.append((int(p[0], 16), int(p[0], 16) + int(p[1], 16)))
    return out


def conflicts(elf, map_path):
    """[(slot, function, literal, data)] for the slots' literal / data pairs in the same operand cache set: the data
    in .hotdata, or one of the hot tables in .rodata"""
    data_start, data_end = hot_data_range(elf)
    if data_start is None:
        return []
    tables = hot_rodata_ranges(elf)
    out = []
    for slot, (start, end) in sorted(hot_slots_fit.slot_ranges(map_path).items()):
        if end > start:
            pairs = literal_data_pairs(elf, start, end, data_start, data_end)
            if tables:
                pairs += [(f, l, v) for f, l, v in literal_data_pairs(elf, start, end, min(a for a, _ in tables), max(b for _, b in tables))
                          if any(a <= v < b for a, b in tables)]
            for function, literal, value in pairs:
                if oc_set(literal) == oc_set(value):
                    out.append((slot, function, literal, value))
    return out


def main():
    warn = "--warn" in sys.argv
    elf, map_path = [a for a in sys.argv[1:] if a != "--warn"][:2]
    found = conflicts(elf, map_path)
    if not found:
        return
    per_function = collections.Counter((slot, function) for slot, function, _, _ in found)
    print("*** hot code loads hot data through a literal in the same operand cache set (tools/oc_conflict_check.py):")
    for (slot, function), n in per_function.most_common():
        literal, value = next((l, v) for s, f, l, v in found if (s, f) == (slot, function))
        print("***   slot %s, %s: %d pair(s), e.g. literal 0x%x -> 0x%x (set %d)" % (slot, function, n, literal, value, oc_set(value)))
    print("*** move the slot (tools/hot_slots.py; tools/hot_slots_fit.py avoids these) or the variable")
    if not warn:
        sys.exit(1)


if __name__ == "__main__":
    main()
