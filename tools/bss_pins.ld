/* FrNES: .bss starts at a fixed operand cache offset (tools/shlelf_hot.xc INCLUDEs this, between .sbss and .bss).

   The 16 kB operand cache is indexed by address bits 13:5, so where the per-frame buffers sit modulo 16 kB decides
   which lines they share with .hotdata, the NES windows and each other. .bss used to follow .data, and a 103 kB
   table added to .data moved all of them (1943 / MM3 lost 1.0-1.6 % at one layout). The frame buffer (0xf000 bytes,
   first in .bss at 72b0dea too) is padded up to 0x3820 modulo 16 kB, relative to FRNES_HOT_DATA_OFFSET (0x800 ships)
   so the A/B layouts move it together with .hotdata, and the ordinary .bss follows it in the order the link gives,
   so the shipping layout keeps every .bss symbol's cache sets. (Pinning the APU buffers as a group of their own,
   out of that order, moved the 224 small KOS / newlib / harness symbols after them and cost 0.3-0.7 % on the asm
   core.) tools/data_budget.py fails the link if the frame buffer moves and, in the AccuracyCoin builds, warns when
   the APU buffers move within .bss. An output section of its own because only outside an output section is `.` an
   absolute address; it is inside __bss_start .. end, so KOS zeroes it (and the padding) at boot. */
.frnes_bss_frame (. + ((0x3820 + FRNES_HOT_DATA_OFFSET - 0x800 - .) & 0x3fff)) :
{
  *(.bss.ppu_framebuffer .bss.ppu_framebuffer.lto_priv.*)
}
