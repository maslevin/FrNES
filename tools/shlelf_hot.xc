/* FrNES: KOS utils/ldscripts/shlelf.xc with the .text.hot.* group placed first (see .text) and the emulator's hot
   global data at a fixed operand cache position (see .hotdata) */
/* Script for -z combreloc: combine and sort reloc sections */
OUTPUT_FORMAT("elf32-shl", "elf32-shl",
	      "elf32-shl")
OUTPUT_ARCH(sh)
STARTUP(_kos_startup.o)
INPUT(crti.o)
INPUT(crtbegin.o)
INPUT(crtend.o)
INPUT(crtn.o)
LOAD_OFFSET = DEFINED(LOAD_OFFSET) ? LOAD_OFFSET : 0x8c010000 ;
FRNES_HOT_DATA_OFFSET = DEFINED(FRNES_HOT_DATA_OFFSET) ? FRNES_HOT_DATA_OFFSET : 0x800 ;
FRNES_HOT_CODE_OFFSET = DEFINED(FRNES_HOT_CODE_OFFSET) ? FRNES_HOT_CODE_OFFSET : 0 ;
FRNES_HOT_CORE_PAD = DEFINED(FRNES_HOT_CORE_PAD) ? FRNES_HOT_CORE_PAD : 0 ;
FRNES_HOT_MID_PAD = DEFINED(FRNES_HOT_MID_PAD) ? FRNES_HOT_MID_PAD : 0 ;
/* FrNES: with a fixed .rodata window, .hotdata's fixed 16 kB block, this far past the window (.hotdata below) */
FRNES_RODATA_SPAN = DEFINED(FRNES_RODATA_SPAN) ? FRNES_RODATA_SPAN : 0x54000 ;
FRNES_RODATA_OFFSET = DEFINED(FRNES_RODATA_OFFSET) ? FRNES_RODATA_OFFSET : 0 ;
FRNES_RODATA_WINDOW = DEFINED(FRNES_RODATA_WINDOW) ? FRNES_RODATA_WINDOW : 0 ;

SECTIONS
{
  /* Read-only sections, merged into text segment: */
  PROVIDE (__executable_start = LOAD_OFFSET); . = LOAD_OFFSET;
  .text           :
  {
    /* FrNES: KOS's startup code (_kos_startup.o) stays at the load address (it is what a CD boot jumps to), then the
       emulator's hot code at fixed cache positions, so the code that runs every scanline / instruction
       doesn't evict itself from the 8 KB direct mapped instruction cache */
    *_kos_startup.o(.text .text.*)
    /* the hot code in fixed slots, each at a fixed position in the instruction and operand caches whatever the size
       of the others (tools/hot_slots.py writes tools/hot_slots.ld; the layout knobs FRNES_HOT_CODE_OFFSET,
       FRNES_HOT_CORE_PAD and FRNES_HOT_MID_PAD move its groups there), then the rest of the code */
    INCLUDE tools/hot_slots.ld
    *libc.a:libc_a-memcpy.o(.text .text.*)
    *libc.a:libc_a-memset.o(.text .text.*)
    *libkallisti.a:entry.o(.text .text.*)
    EXCLUDE_FILE(*mapper_boards.o *input_ports.o *nsf.o *game_menu.o *state_core.o *frame_hud_perf.o *fds.o *frontend/*.o) *(.text .stub .text.[!~]* .gnu.linkonce.t.*)
    /* FrNES: after all the library code, the code that changes size most and runs least: the boards (mapper.c's
       FRNES_BOARD_TEXT, fds.c whole: not LTO, so a file pattern finds it) and the cold code (FRNES_COLD_TEXT). A
       board growing or a new one then moves no library or emulator code that runs every frame (KOS is LTO'd with the
       emulator, so no pattern can put the libraries before the emulator's code instead); tools/text_drift.py checks
       it. The other cold files built without LTO, mapper_boards.c's boards among them, are in .frnes_cold_text after
       .rodata. Their functions a slot names (tools/hot_slots.py) are in the slots above */
    __frnes_text_tail = .;
    *(.text.~frnes_board)
    *fds.o(.text .text.*)
    *(.text.~frnes_cold)
    /* FrNES: the frame display's code (frame_hud.c) after it: the cold functions keep their places as it changes */
    *(.text.~frnes_cold_hud)
    /* FrNES: the frontend's (make frontend, frontend/*.o, built without LTO) last, so that it doesn't move anything
       of the emulator's or KOS's */
    *frontend/*.o(.text .stub .text.*)
    /* .gnu.warning sections are handled specially by elf32.em.  */
    *(.gnu.warning)
  } =0

  /* Custom sections, aligned to the icache size */
  .sub0 : ALIGN(8192) {
    *(.sub0*)
  }
  .sub1 : ALIGN(8192) {
    *(.sub1*)
  }
  .sub2 : ALIGN(8192) {
    *(.sub2*)
  }
  .sub3 : ALIGN(8192) {
    *(.sub3*)
  }
  .sub4 : ALIGN(8192) {
    *(.sub4*)
  }
  .sub5 : ALIGN(8192) {
    *(.sub5*)
  }
  .sub6 : ALIGN(8192) {
    *(.sub6*)
  }
  .sub7 : ALIGN(8192) {
    *(.sub7*)
  }
  .sub8 : ALIGN(8192) {
    *(.sub8*)
  }
  .sub9 : ALIGN(8192) {
    *(.sub9*)
  }

  .init           :
  {
    KEEP (*(.init))
  } =0
  .fini           :
  {
    KEEP (*(.fini))
  } =0
  .interp         : { *(.interp) }
  .note.gnu.build-id : { *(.note.gnu.build-id) }
  .hash           : { *(.hash) }
  .gnu.hash       : { *(.gnu.hash) }
  .dynsym         : { *(.dynsym) }
  .dynstr         : { *(.dynstr) }
  .gnu.version    : { *(.gnu.version) }
  .gnu.version_d  : { *(.gnu.version_d) }
  .gnu.version_r  : { *(.gnu.version_r) }
  .rel.dyn        :
    {
      *(.rel.init)
      *(.rel.text .rel.text.* .rel.gnu.linkonce.t.*)
      *(.rel.fini)
      *(.rel.rodata .rel.rodata.* .rel.gnu.linkonce.r.*)
      *(.rel.data.rel.ro* .rel.gnu.linkonce.d.rel.ro.*)
      *(.rel.data .rel.data.* .rel.gnu.linkonce.d.*)
      *(.rel.tdata .rel.tdata.* .rel.gnu.linkonce.td.*)
      *(.rel.tbss .rel.tbss.* .rel.gnu.linkonce.tb.*)
      *(.rel.ctors)
      *(.rel.dtors)
      *(.rel.got)
      *(.rel.sdata .rel.sdata.* .rel.gnu.linkonce.s.*)
      *(.rel.sbss .rel.sbss.* .rel.gnu.linkonce.sb.*)
      *(.rel.sdata2 .rel.sdata2.* .rel.gnu.linkonce.s2.*)
      *(.rel.sbss2 .rel.sbss2.* .rel.gnu.linkonce.sb2.*)
      *(.rel.bss .rel.bss.* .rel.gnu.linkonce.b.*)
    }
  .rela.dyn       :
    {
      *(.rela.init)
      *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*)
      *(.rela.fini)
      *(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*)
      *(.rela.data .rela.data.* .rela.gnu.linkonce.d.*)
      *(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*)
      *(.rela.tbss .rela.tbss.* .rela.gnu.linkonce.tb.*)
      *(.rela.ctors)
      *(.rela.dtors)
      *(.rela.got)
      *(.rela.sdata .rela.sdata.* .rela.gnu.linkonce.s.*)
      *(.rela.sbss .rela.sbss.* .rela.gnu.linkonce.sb.*)
      *(.rela.sdata2 .rela.sdata2.* .rela.gnu.linkonce.s2.*)
      *(.rela.sbss2 .rela.sbss2.* .rela.gnu.linkonce.sb2.*)
      *(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*)
    }
  .rel.plt        : { *(.rel.plt) }
  .rela.plt       : { *(.rela.plt) }
  .plt            : { *(.plt) }
  PROVIDE (__etext = .);
  PROVIDE (_etext = .);
  PROVIDE (etext = .);
  /* FrNES: .rodata starts at a fixed operand cache offset, 0x0808 modulo 16 kB (where it is at 72b0dea on KOS with the purge patch), relative to
     FRNES_HOT_DATA_OFFSET like .hotdata and the .bss pins: otherwise it starts wherever .text ends, and cold code
     growing by a few hundred bytes moved the hot tables in it (the PPU's nibble spread, the exact core's plan, the
     APU's tables) onto other sets. tools/data_budget.py checks it. */
  /* (FRNES_RODATA_WINDOW, the Makefile, per build flavour: the fixed 16 kB window .rodata starts in, at the same
     phase, so the code before it can grow without moving .rodata and everything after it; the link stops when the code
     reaches it. 0: the next phase right after the code) */
  __frnes_rodata_at = FRNES_RODATA_WINDOW + ((0x0808 + FRNES_HOT_DATA_OFFSET - 0x800) & 0x3fff);
  ASSERT((FRNES_RODATA_WINDOW == 0) || (. <= __frnes_rodata_at), "FrNES: the code reached the fixed address of .rodata, RODATA_WINDOW_* in the Makefile for this build: raise it by 0x4000, which moves .rodata and everything after it 16 kB later, and A/B it, or move cold code out")
  .rodata (FRNES_RODATA_WINDOW != 0 ? __frnes_rodata_at : . + ((0x0808 + FRNES_HOT_DATA_OFFSET - 0x800 - .) & 0x3fff)) :
  { 
    /* FrNES: FRNES_RODATA_OFFSET (default 0): padding before the constant tables, which moves them in the operand
       cache against the hot data and the NES pages (a layout knob for tools/ab_layouts.py) */
    . = . + FRNES_RODATA_OFFSET;
    /* FrNES: the hot tables first, in this order (tools/data_budget.py's HOT_RODATA checks where they land): otherwise
       their order follows LTO's partitions, which a change to unrelated code reshuffles */
    *(.rodata.ppu_nibble_spread .rodata.ppu_nibble_spread.lto_priv.*)
    *(.rodata.flags.0 .rodata.flags.0.lto_priv.*)
    *(.rodata.ex_plan_mode .rodata.ex_plan_mode.lto_priv.*)
    *(.rodata.apu_length_table .rodata.apu_length_table.lto_priv.*)
    *(.rodata.sequence_4 .rodata.sequence_4.lto_priv.*)
    *(.rodata.sequence_5 .rodata.sequence_5.lto_priv.*)
    *(.rodata.dmc_rate_table .rodata.dmc_rate_table.lto_priv.*)
    *(.rodata.noise_period_table .rodata.noise_period_table.lto_priv.*)
    *(.rodata.duty_table .rodata.duty_table.lto_priv.*)
    *(.rodata.fds_mod_adjust .rodata.fds_mod_adjust.lto_priv.*)
    *(.rodata.fds_master_volume .rodata.fds_master_volume.lto_priv.*)
    *(.rodata.opll_multiple .rodata.opll_multiple.lto_priv.*)
    *(.rodata.opll_ksl .rodata.opll_ksl.lto_priv.*)
    *(.rodata.vrc7_patches .rodata.vrc7_patches.lto_priv.*)
    *(.rodata.apu_frame_step_type .rodata.apu_frame_step_type.lto_priv.*)
    *(.rodata.apu_frame_steps .rodata.apu_frame_steps.lto_priv.*)
    *(EXCLUDE_FILE(*frontend/*.o) .rodata EXCLUDE_FILE(*frontend/*.o) .rodata.* .gnu.linkonce.r.*)
    *frontend/*.o(.rodata .rodata.*)
    /* FrNES: the tables read at ROM load only (FRNES_COLD_RODATA, memory.h) last, so they move no other table */
    *(.frnes_cold_rodata)
    . = ALIGN(4);
    __tdata_align = .;
    LONG (ALIGNOF(.tdata));
    . = ALIGN(4);
    __tbss_align = .;
    LONG (ALIGNOF(.tbss));
    . = ALIGN(4);
    /* FrNES: the frame display's ranges of hot code (frame_hud_perf.c): a placed build's, empty in a slotted one */
    __frnes_hud_ranges = .;
    INCLUDE frnes_hud_ranges.ld
    __frnes_hud_ranges_end = .;
  }
  .rodata1        : { *(.rodata1) }
  .sdata2         :
  {
    *(.sdata2 .sdata2.* .gnu.linkonce.s2.*)
  }
  .sbss2          : { *(.sbss2 .sbss2.* .gnu.linkonce.sb2.*) }
  .eh_frame_hdr : { *(.eh_frame_hdr) }
  .eh_frame       : ONLY_IF_RO { KEEP (*(.eh_frame)) }
  .gcc_except_table   : ONLY_IF_RO { *(.gcc_except_table .gcc_except_table.*) }
  /* FrNES: the ordinary code of the cold files built without LTO (the Makefile's -fno-lto rules, but fds.c's FDS
     timing and mapper_mmc3_sync.c's write filter: the boards, NSF, the in-game menu, the save state chunks, the
     controllers, the frame display's perf lines) after .rodata, in the room before .hotdata's fixed block
     (FRNES_RODATA_SPAN raised 0x40000 -> 0x54000 for it, .hotdata and what follows 80 kB later): in front of the fixed .rodata window its growth filled the window, every board
     added; here it moves nothing but itself, and the link stops at .hotdata as for .rodata's tables (the ASSERT
     below). Their functions a slot names (tools/hot_slots.py) are in the slots in .text. (Not after .bss: KOS's `end`
     is both the end of what it zeroes at boot and the heap's start) */
  .frnes_cold_text :
  {
    __frnes_cold_text_start = .;
    *mapper_boards.o(.text .stub .text.*)
    *input_ports.o(.text .stub .text.*)
    *nsf.o(.text .stub .text.*)
    *game_menu.o(.text .stub .text.*)
    *state_core.o(.text .stub .text.*)
    *frame_hud_perf.o(.text .stub .text.*)
    __frnes_cold_text_end = .;
  }
  /* Adjust the address for the data segment.  We want to adjust up to
     the same address within the page on the next page up.  */
  . = ALIGN(128) + (. & (128 - 1));
  /* Exception handling  */
  .eh_frame       : ONLY_IF_RW { KEEP (*(.eh_frame)) }
  .gcc_except_table   : ONLY_IF_RW { *(.gcc_except_table .gcc_except_table.*) }
  /* Thread Local Storage sections  */
  .tdata	  : 
  { 
    __tdata_start = .;
    *(.tdata .tdata.* .gnu.linkonce.td.*) 
  }
  __tdata_size = SIZEOF(.tdata);
  .tbss	(NOLOAD)	  : 
  { 
    *(.tbss .tbss.* .gnu.linkonce.tb.*) 
    *(.tcommon)
  }
  __tbss_size = SIZEOF(.tbss);
  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  }
  .init_array     :
  {
     PROVIDE_HIDDEN (__init_array_start = .);
     KEEP (*(SORT(.init_array.*)))
     KEEP (*(.init_array))
     PROVIDE_HIDDEN (__init_array_end = .);
  }
  .fini_array     :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(.fini_array))
    KEEP (*(SORT(.fini_array.*)))
    PROVIDE_HIDDEN (__fini_array_end = .);
  }
  .ctors          :
  {
    ___ctors = .;
    /* gcc uses crtbegin.o to find the start of
       the constructors, so we make sure it is
       first.  Because this is a wildcard, it
       doesn't matter if the user does not
       actually link against crtbegin.o; the
       linker won't look for a file to match a
       wildcard.  The wildcard also means that it
       doesn't matter which directory crtbegin.o
       is in.  */
    KEEP (*crtbegin.o(.ctors))
    KEEP (*crtbegin?.o(.ctors))
    /* We don't want to include the .ctor section from
       the crtend.o file until after the sorted ctors.
       The .ctor section from the crtend file contains the
       end of ctors marker and it must be last */
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    ___ctors_end = .;
  }
  .dtors          :
  {
    ___dtors = .;
    KEEP (*crtbegin.o(.dtors))
    KEEP (*crtbegin?.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    ___dtors_end = .;
  }
  .jcr            : { KEEP (*(.jcr)) }
  .data.rel.ro : { *(.data.rel.ro.local* .gnu.linkonce.d.rel.ro.local.*) *(.data.rel.ro* .gnu.linkonce.d.rel.ro.*) }
  .dynamic        : { *(.dynamic) }
  /* FrNES: the emulator's hot globals (zero-initialized ones too, stored as zeros), listed in frnes_hot_data.ld by
     tools/hot_data.py, FRNES_HOT_DATA_OFFSET bytes past a 16 KB boundary: their operand cache sets don't change with
     unrelated code and data. Before .data / .bss, so their wildcards don't take these first. */
  /* FrNES: with a fixed .rodata window (FRNES_RODATA_WINDOW), .hotdata at a fixed 16 kB block too, FRNES_RODATA_SPAN
     past the window: cold tables growing .rodata use the room before it instead of moving .hotdata, .data, .bss and
     the heap's start a block later (vs99-ines' Vs. palettes did); the link stops when .rodata's tail reaches it, as the
     code reaching .rodata does. tools/data_budget.py reports the room. Without a window, the next 16 kB boundary */
  __frnes_hotdata_at = (FRNES_RODATA_WINDOW != 0) ? FRNES_RODATA_WINDOW + FRNES_RODATA_SPAN : 0;
  ASSERT((FRNES_RODATA_WINDOW == 0) || (. <= __frnes_hotdata_at), "FrNES: .rodata and what follows it reached .hotdata's fixed address, FRNES_RODATA_SPAN past RODATA_WINDOW_* in the Makefile for this build: raise FRNES_RODATA_SPAN (tools/shlelf_hot.xc) by 0x4000, which moves .hotdata, .data, .bss and the heap's start 16 kB later, and A/B it, or move cold tables out")
  .hotdata        (FRNES_RODATA_WINDOW != 0 ? __frnes_hotdata_at : ALIGN(0x4000)) :
  {
    . = . + FRNES_HOT_DATA_OFFSET;
    /* FrNES: whatever comes before it (a larger image, FRNES_RODATA_OFFSET) can move .hotdata to a later 16 kB
       boundary, which keeps its operand cache sets (the 16 kB cache is indexed by address bits 13:5); only its
       position within the 16 kB has to stay FRNES_HOT_DATA_OFFSET */
    ASSERT((. & 0x3fff) == (FRNES_HOT_DATA_OFFSET & 0x3fff), "FrNES hot data off its operand cache offset");
    /* FrNES: a data placement's sections first (the Makefile's DATA_PLACE, tools/simplay/data_ld.py; empty without one),
       each at its offset modulo 16 kB, then the hot data it doesn't place (tools/hot_data.py's offsets from after them) */
    __frnes_hot_data_block = .;
    INCLUDE frnes_data_place.ld
    __frnes_hot_data_start = .;
    INCLUDE frnes_hot_data.ld
    . = ALIGN(32);
    *(.frnes_hot_rodata)
    . = ALIGN(32);
    __frnes_hot_data_end = .;
    /* (with a data placement: .data at its offset modulo 16 kB in the build the placement was searched from) */
    INCLUDE frnes_data_place_end.ld
  }
  .data           :
  {
    *(EXCLUDE_FILE(*frontend/*.o) .data EXCLUDE_FILE(*frontend/*.o) .data.* .gnu.linkonce.d.*)
    *frontend/*.o(.data .data.*)
    SORT(CONSTRUCTORS)
  }
  .data1          : { *(.data1) }
  .got            : { *(.got.plt) *(.got) }
  /* We want the small data sections together, so single-instruction offsets
     can access them all, and initialized data all before uninitialized, so
     we can shorten the on-disk segment size.  */
  .sdata          :
  {
    *(.sdata .sdata.* .gnu.linkonce.s.*)
  }
  _edata = .; PROVIDE (edata = .);
  . = ALIGN(8);
  __monitors_start = .;
  .monitors       :
  {
    *(.monitors)
  }
  __monitors_end = .;
  . = ALIGN(8);
  __bss_start = .;
  .sbss           :
  {
    *(.dynsbss)
    *(.sbss .sbss.* .gnu.linkonce.sb.*)
    *(.scommon)
  }
  /* FrNES: the per-frame buffers at their fixed operand cache offsets */
  INCLUDE tools/bss_pins.ld
  .bss            :
  {
   *(.dynbss)
   /* FrNES: all but FRNES_COLD_BSS's section (memory.h), which goes last, after COMMON */
   /* FrNES: .bss's sections at fixed offsets whatever LTO's partitions give (tools/bss_order.py; the Makefile copies
      the build flavour's tools/bss_order/*.ld into the build directory, on the link's -L path), then the others: new
      variables come after the pinned ones */
   INCLUDE bss_order.ld
   *(EXCLUDE_FILE(*frontend/*.o) .bss EXCLUDE_FILE(*frontend/*.o) .bss.[!~]* .gnu.linkonce.b.*)
   /* (the frontend's after the emulator's) */
   *frontend/*.o(.bss .bss.*)
   *(COMMON)
   *(.bss.~frnes_cold)
   /* FrNES: the frame display's state (frame_hud.c), after it: the emulator's cold variables keep their places */
   *(.bss.~frnes_cold_hud)
   /* Align here to ensure that the .bss section occupies space up to
      _end.  Align after .bss to ensure correct alignment even if the
      .bss section disappears because there are no input sections.
      FIXME: Why do we need it? When there is no .bss section, we don't
      pad the .data section.  */
   . = ALIGN(. != 0 ? 32 / 8 : 1);
  }
  . = ALIGN(32 / 8);
  . = ALIGN(32 / 8);
  /* FrNES: the heap's start (KOS's sbrk starts at end) at a fixed offset modulo 16 kB, FRNES_HEAP_AT (the Makefile,
     per build) relative to FRNES_HOT_DATA_OFFSET like the .bss pins: the ROM / VROM pages, the sound buffers and the
     thread stacks are allocated from it, so .bss growing (47 / 37's cold state moved it 0x910) or LTO's .bss padding
     changing would put them all on other operand cache sets. Up to 16 kB less heap. tools/data_budget.py checks it */
  . = . + ((FRNES_HEAP_AT + FRNES_HOT_DATA_OFFSET - 0x800 - .) & 0x3fff);
  _end = .; PROVIDE (end = .);
  .ocram 0x7c001000 (NOLOAD) :
  {
    *(.ocram)
    /* We have 8kb of operand cache RAM. The next line lets ld throw
       an error if we exceed that size.  */
    . = . > 0x2000 ? 0x2000 : .;
  }
  /* Stabs debugging sections.  */
  .stab          0 : { *(.stab) }
  .stabstr       0 : { *(.stabstr) }
  .stab.excl     0 : { *(.stab.excl) }
  .stab.exclstr  0 : { *(.stab.exclstr) }
  .stab.index    0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment       0 : { *(.comment) }
  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */
  /* DWARF 1 */
  .debug          0 : { *(.debug) }
  .line           0 : { *(.line) }
  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_sfnames  0 : { *(.debug_sfnames) }
  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }
  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }
  /* DWARF 3 */
  .debug_pubtypes 0 : { *(.debug_pubtypes) }
  .debug_ranges   0 : { *(.debug_ranges) }
  .gnu.attributes 0 : { KEEP (*(.gnu.attributes)) }
  /DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) }
}
