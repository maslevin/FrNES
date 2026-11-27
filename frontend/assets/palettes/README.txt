NES palettes for the Video Options' Palette row (each .pal: 64 colors as RGB, or 512 with the emphasis colors after;
the first 64 are used).

FrNES.pal                 FrNES's own colors (the emulator's built-in palette)
NTSC (computed).pal       tools/gen_ntsc_palette.py 1.0 105, computed from the 2C02's NTSC signal levels
2C02G (NTSC US).pal       nesdev wiki, File:2C02G_U_wiki.pal (2C02G, 7.5 IRE setup black)
2C02G (NTSC Japan).pal    nesdev wiki, File:2C02G_J_wiki.pal (2C02G, blank-level black)
2C07 (PAL).pal            nesdev wiki, File:2C07_wiki.pal
Vs. System 2C03 (RGB).pal nesdev wiki, File:2C03_wiki.pal (the RGB PPUs, 2C03 / 2C05: the Vs. System's and
                          PlayChoice-10's colors. Vs. games use their own PPU's palette whatever is chosen here:
                          vs_palette, mapper_boards.c)

The nesdev wiki palettes are from https://www.nesdev.org/wiki/PPU_palettes.
