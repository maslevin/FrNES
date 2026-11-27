#!/usr/bin/env python3
"""Checks a link's per-frame slot against tools/hot_frame.txt (run by the Makefile after each link):

  python3 tools/hot_frame_check.py MAP SOURCE.c...

Fails when a listed function is missing (no such function in the sources: renamed or deleted, take it off the list),
when a listed function this build has out of line isn't in the slot, or when the slot holds a function that isn't
listed. Functions of the slot's families (ppu_, apu_, mmc3_ ...) that aren't listed are cold and stay out of it: it
names any new ones (not in the map's previous run) so a hot one gets added on purpose."""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hot_slots  # noqa: E402

CLONE = re.compile(r"\.(lto_priv|constprop|isra|part|cold)\.\d+.*$")


def main():
    map_path, sources = sys.argv[1], sys.argv[2:]
    listed = set(hot_slots.frame_members())
    text = open(map_path).read()
    start = re.search(r"^\s+(0x[0-9a-f]+)\s+__frnes_slot_frame = ", text, re.M)
    end = re.search(r"^\s+(0x[0-9a-f]+)\s+__frnes_slot_frame_end = ", text, re.M)
    if not (start and end):
        sys.exit("hot_frame_check: no frame slot in " + map_path)
    lo, hi = int(start.group(1), 16), int(end.group(1), 16)
    # every out of line function of the link: its base name -> the addresses of its sections
    placed = {}
    for m in re.finditer(r"^ \.text\.([A-Za-z_][^\s]*)\s*\n?\s+(0x8c[0-9a-f]+)\s+0x[0-9a-f]+", text, re.M):
        placed.setdefault(CLONE.sub("", m.group(1)), []).append(int(m.group(2), 16))
    in_slot = {n for n, addrs in placed.items() if any(lo <= a < hi for a in addrs)}
    source_text = "\n".join(open(s, errors="replace").read() for s in sources)
    errors = []
    for name in sorted(listed):
        if not re.search(r"\b%s\s*\(" % re.escape(name), source_text):
            errors.append("%s is listed but no source defines it (renamed or deleted: take it off the list)" % name)
        elif name in placed and name not in in_slot:
            errors.append("%s is listed but not in the per-frame slot" % name)
    for name in sorted(in_slot - listed):
        errors.append("%s is in the per-frame slot but not listed" % name)
    if errors:
        sys.exit("hot_frame_check (tools/hot_frame.txt): " + "; ".join(errors))
    cold = sorted(n for n in placed if n not in listed and n.startswith(hot_slots.FRAME_FAMILIES))
    if cold:
        print("hot_frame_check: per-frame families left cold (not in tools/hot_frame.txt): " + " ".join(cold))


if __name__ == "__main__":
    main()
