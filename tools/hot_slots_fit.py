#!/usr/bin/env python3
"""Chooses tools/hot_slots.py's slot offsets from PC sample profiles: the offsets whose hot lines share the fewest
instruction cache sets, summed over several games.

  python3 tools/hot_slots_fit.py NAME:WEIGHT:LOG:MAP:ELF [NAME:WEIGHT:LOG:MAP:ELF ...] [--write]

Each profile is a replay's console log with AC PCS lines (test_main.c -DAC_PC_SAMPLES, -DAC_PC_SAMPLES_FROM= the first
frame of the game's heaviest section) and the linker map of the build that ran (made with the current
tools/hot_slots.ld), and its ELF: samples in the test harness's own functions (_ac_*: the AC_GUARD heap check, the
console output) are left out, the player doesn't have them. The other samples are counted per 32 byte line and normalised to 1 per game, times WEIGHT; a line in a
slot moves with the slot's offset, any other line (KOS, the cold code, the test harness) stays where it was.

The cost of an I-cache set is the sum, over each pair of lines mapping to it, of the smaller of their weights (two hot
lines in one set of a direct mapped cache evict each other about as often as the lighter one runs), the cost of a game
the sum over the sets, and the fit lowers the weighted sum over the games: coordinate descent over the slots, each
offset tried at every 32 bytes of the 8 kB cache (keeping its 16 kB window bit), until nothing improves. It prints
each game's cost with the current and the fitted offsets; --write puts the fitted ones into tools/hot_slots.py's table.

An offset that puts one of the slot's literals in the same operand cache set as the .hotdata variable it holds the
address of is never chosen (tools/oc_conflict_check.py: each load then evicts the other; the pairs are read from the
first profile's ELF, and their sets only depend on the slot's offset, since .hotdata stays put).
"""
import bisect
import collections
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hot_slots  # noqa: E402

LINE, SETS, ICACHE = 32, 256, 0x2000
OC_SETS, WINDOW = 512, 0x4000
NM = "/opt/toolchains/dc/sh-elf/bin/sh-elf-nm"
OBJDUMP = "/opt/toolchains/dc/sh-elf/bin/sh-elf-objdump"


def slot_ranges(map_path):
    """{slot: (start, end of its code)} from the map: the __frnes_slot_NAME symbols and the input sections after them"""
    starts = {}
    text = open(map_path).read()
    for m in re.finditer(r"^\s+(0x[0-9a-f]+)\s+__frnes_slot_([a-z0-9_]+) = ", text, re.M):
        if not m.group(2).endswith(("_at", "_end")):
            starts[m.group(2)] = int(m.group(1), 16)
    ends = {}
    for m in re.finditer(r"^\s+(0x[0-9a-f]+)\s+__frnes_slot_([a-z0-9_]+)_end = MAX \(\(__frnes_slot_[a-z0-9_]+ \+ (0x[0-9a-f]+)\), \.\)",
                         text, re.M):
        ends[m.group(2)] = int(m.group(1), 16)
    return {name: (start, ends.get(name, start)) for name, start in starts.items()}


def harness_ranges(elf_path):
    """the address ranges of the test harness's functions (_ac_*) in ELF"""
    out = []
    nm = subprocess.run([NM, "-S", "-n", elf_path], capture_output=True, text=True)
    for line in nm.stdout.splitlines():
        p = line.split()
        if len(p) == 4 and p[2] in "tT" and p[3].startswith("_ac_"):
            out.append((int(p[0], 16), int(p[0], 16) + int(p[1], 16)))
    return sorted(out)


def profile(log_path, map_path, elf_path, weight):
    """(per slot: {line offset in the slot: weight}, the other lines: {I-cache set: weight}) for one game"""
    harness = harness_ranges(elf_path)
    starts = [a for a, _ in harness]
    def in_harness(pc):
        i = bisect.bisect_right(starts, pc) - 1
        return i >= 0 and pc < harness[i][1]
    pcs = [int(x, 16) for line in open(log_path) if line.startswith("AC PCS") for x in line.split()[2:]]
    pcs = [pc for pc in pcs if not in_harness(pc)]
    if not pcs:
        sys.exit("no AC PCS samples in " + log_path)
    ranges = slot_ranges(map_path)
    per_slot = collections.defaultdict(collections.Counter)
    background = collections.Counter()
    unit = weight / len(pcs)
    for pc in pcs:
        for name, (start, end) in ranges.items():
            if start <= pc < end:
                per_slot[name][(pc - start) // LINE] += unit
                break
        else:
            background[(pc // LINE) % SETS] += unit
    return per_slot, background


def oc_pairs(map_path, elf_path):
    """{slot: [(the literal's offset in the slot, the operand cache set of the .hotdata variable it addresses)]}"""
    import oc_conflict_check
    data_start, data_end = oc_conflict_check.hot_data_range(elf_path)
    out = {}
    if data_start is None:
        return out
    for slot, (start, end) in slot_ranges(map_path).items():
        if end > start:
            out[slot] = [(literal - start, (value // LINE) % OC_SETS)
                         for _, literal, value in oc_conflict_check.literal_data_pairs(elf_path, start, end, data_start, data_end)]
    return out


def oc_conflicts(pairs, name, offset):
    """how many of the slot's literal / data pairs share an operand cache set with the slot at offset (mod 16 kB)"""
    return sum(1 for literal, data_set in pairs.get(name, []) if (((offset + literal) % WINDOW) // LINE) % OC_SETS == data_set)


def game_cost(game, offsets):
    per_slot, background = game
    sets = collections.defaultdict(list)
    for s, w in background.items():
        sets[s].append(w)
    for name, lines in per_slot.items():
        base = offsets[name] // LINE
        for line, w in lines.items():
            sets[(base + line) % SETS].append(w)
    cost = 0.0
    for ws in sets.values():
        if len(ws) > 1:
            ws.sort()
            # sum over pairs of the smaller weight: each weight times the number of heavier lines
            cost += sum(w * (len(ws) - 1 - i) for i, w in enumerate(ws))
    return cost


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    games = []
    for spec in args:
        name, weight, log_path, map_path, elf_path = spec.split(":")
        games.append((name, float(weight), profile(log_path, map_path, elf_path, float(weight))))
    current = {name: offset for name, _, offset, _, _ in hot_slots.SLOTS}
    fitted = dict(current)
    _, _, _, map0, elf0 = args[0].split(":")
    pairs = oc_pairs(map0, elf0)
    total = lambda offsets: sum(game_cost(g, offsets) for _, _, g in games)
    best = total(fitted)
    improved = True
    while improved:
        improved = False
        for name in fitted:
            window = fitted[name] & ~(ICACHE - 1)
            for candidate in range(0, ICACHE, LINE):
                if oc_conflicts(pairs, name, window | candidate):
                    continue
                trial = dict(fitted, **{name: window | candidate})
                cost = total(trial)
                if cost < best - 1e-12:
                    best, fitted, improved = cost, trial, True
    print("%-10s %8s %10s %10s" % ("game", "weight", "current", "fitted"))
    for name, weight, g in games:
        print("%-10s %8.2f %10.5f %10.5f" % (name, weight, game_cost(g, current), game_cost(g, fitted)))
    print("%-10s %8s %10.5f %10.5f" % ("total", "", total(current), best))
    conflicts = lambda offsets: sum(oc_conflicts(pairs, name, offsets[name]) for name in offsets)
    print("literal / hot data operand cache conflicts: current %d, fitted %d" % (conflicts(current), conflicts(fitted)))
    for name in current:
        if fitted[name] != current[name]:
            print("  %-9s 0x%04x -> 0x%04x" % (name, current[name], fitted[name]))
    if "--write" in sys.argv:
        path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "hot_slots.py")
        src = open(path).read()
        for name in current:
            src = re.sub(r'(\("%s", .*?, )0x%04x(, 0x)' % (re.escape(name), current[name]),
                         lambda m: m.group(1) + "0x%04x" % fitted[name] + m.group(2), src, count=1, flags=re.S)
        open(path, "w").write(src)
        print("wrote", path)


if __name__ == "__main__":
    main()
