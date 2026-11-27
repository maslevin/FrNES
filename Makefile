# KallistiOS 2.0.0
#
# SH4_6502_Emulator/Makefile
# (c)2000-2025 Matt Slevinsky
#

include $(KOS_BASE)/Makefile.rules

# the romdisk (KOS's own rules, enabled by KOS_ROMDISK_DIR, list the directory's files as prerequisites unquoted since
# KOS master, and the default ROM's name has spaces): the same two rules with the names escaped
ROMDISK_DIR = romdisk
ROMDISK_FILES := $(shell find $(ROMDISK_DIR) -type f ! -name .DS_Store ! -name .gitignore ! -name Thumbs.db 2>/dev/null | sed 's/ /\\ /g')
romdisk.img: $(ROMDISK_FILES)
	$(KOS_GENROMFS) -f romdisk.img -d $(ROMDISK_DIR) -v -x .gitignore -x .DS_Store -x Thumbs.db

romdisk.o: romdisk.img $(KOS_BASE)/utils/bin2c/bin2c
	$(KOS_BASE)/utils/bin2c/bin2c romdisk.img romdisk_tmp.c romdisk
	kos-cc -o romdisk_tmp.o -c romdisk_tmp.c
	$(KOS_CC) -o romdisk.o -r romdisk_tmp.o $(KOS_LIB_PATHS) -Wl,--whole-archive -lromdiskbase -nostdlib
	rm romdisk_tmp.c romdisk_tmp.o

# the emulator's sources are in emulator/ (objects keep their names: release/cpu.o ...), and __FILE__ stays the bare
# file name, as when they were in the root (NES_WINDOW's messages, assert)
vpath %.c emulator
vpath %.s emulator
vpath %.h emulator
# (the type names KOS no longer defines, kos_compat.h)
override KOS_CFLAGS += -Iemulator -include emulator/kos_compat.h -fmacro-prefix-map=emulator/=

# The emulator's optimization flags, whatever environ.sh defaults to (KOS master's are -O2, no LTO and
# -fomit-frame-pointer): -O3 with LTO is what the hot code layout (tools/hot_slots.py, tools/hot_data.py) and every
# timing baseline were made with. The atomic model stays KOS's (its libraries are built with it).
# (GCC 15's jump threading duplicates paths much more than GCC 13's did: ppu_rendering_dot_impl's 8 dot fetch switch
# was copied about 8 times into cpu_sync_devices, which grew 23 %; capping the statements it may duplicate per path
# brings the per-access catch-up paths back to GCC 13's size)
# (-fomit-frame-pointer: r14 free for allocation in every hot C function; backtraces are not needed in these builds)
# (and its loop unswitching copies loops per invariant condition: the renderer 15 % and the VRC7 span loop 14 % over
# GCC 13's size)
FRNES_OPT_FLAGS = -O3 -freorder-blocks-algorithm=simple -flto=auto -fomit-frame-pointer --param=max-jump-thread-duplication-stmts=5 -fno-unswitch-loops
FRNES_OPT_FILTER = -O0 -O1 -O2 -O3 -Os -Og -fomit-frame-pointer -fno-omit-frame-pointer -flto=auto -flto -ffat-lto-objects \
	-freorder-blocks-algorithm=simple
override KOS_CFLAGS := $(filter-out $(FRNES_OPT_FILTER),$(KOS_CFLAGS)) $(FRNES_OPT_FLAGS)
# (KOS master's KOS_LDFLAGS carries the optimization flags too, and the rules here put it after KOS_CFLAGS: its -O2
# would win, at compile time and in LTO's link time compilation)

# KOS's linker script with the emulator's hot code (.text.hot.* sections) placed first in .text, and the hot global
# data at a fixed operand cache position: each build directory gets frnes_hot_data.ld from tools/hot_data.py
# (FrNES on mainline KOS, kos_mainline.c: --wrap=mmu_set_sq_addr reloads the store queues' UTLB entries 0x3e / 0x3f
# with LDTLB when sq_lock points them somewhere (pvr_list_begin every frame, pvr_txr_load), where mainline writes the
# UTLB data array from cached code, the pattern that left stray TLB values in RAM; it only takes with KOS built without
# LTO, docs/building.md)
# The release's and the frontend's hot code: tools/hot_place.ld, the code placement searched against the simulator's
# traces of 22 movies (integration's tools/simplay; release player A/B on ORANGE 2026-11-24: -0.3 to -6.3 %, faster on
# all 12 movies), in place of tools/hot_slots.ld's slots, through a copy of the linker script in the build directory
# (shlelf_place.xc). PLACE= (empty) builds with the slots as before. The slots' checks (tools/hot_frame_check.py,
# tools/hot_text_check.py) are off for a placed build
PLACE ?= tools/hot_place.ld
# The release's and the frontend's data: tools/data_place.data.ld (with .data_end.ld), the data placement searched
# together with tools/hot_place.ld against exact data traces of the 22 movies on the players' own build (integration's
# tools/simplay/data_ld.py; release player A/B on BLUE / RED 2026-11-25 vs the code placement alone: -7 to -11 %), at
# .hotdata's start; DATA_PLACE= (empty) for none
DATA_PLACE ?= tools/data_place.data.ld
HOT_XC = tools/shlelf_hot.xc
override KOS_LDFLAGS := $(filter-out $(FRNES_OPT_FILTER),$(subst $(KOS_BASE)/utils/ldscripts/shlelf.xc,$(HOT_XC),$(KOS_LDFLAGS))) \
	$(FRNES_OPT_FLAGS) -Wl,--wrap=mmu_set_sq_addr
HOT_DATA_SRCS = $(addprefix emulator/, cpu.c ppu.c apu.c apu_synth.c video.c memory.c mapper.c 6502_readbyte.c 6502_readword.c 6502_writebyte.c)
# where the hot data block starts in the 16 KB operand cache, e.g. make release HOT_DATA_OFFSET=0x3000
HOT_DATA_OFFSET ?= 0x800
# and padding before the hot code, which moves it in the instruction cache (HOT_CODE_OFFSET=0x400 ...)
HOT_CODE_OFFSET ?= 0
# and two more offsets of the whole slotted hot code (HOT_CORE_PAD=0x200 ..., tools/hot_slots.py)
HOT_CORE_PAD ?= 0
# (HOT_MID_PAD=0x1a0 ...; before the slots they moved hot groups against each other)
HOT_MID_PAD ?= 0
# and padding before .rodata, which moves the constant tables in the operand cache (RODATA_OFFSET=0x2a0 ...)
RODATA_OFFSET ?= 0
# tools/oc_conflict_check.py reports literal / hot data operand cache pairs at every link. It only warns for now: the
# mmc3 slot's best measured offset (0x3ea0) keeps 2 such pairs in mapper_ppu_clock, and the conflict free offsets
# next to it (0x3ee0) were slower (the I-cache side of the placement weighs more there). To make it fail the link in
# the shipping layout (the knobs above at their defaults) once a placement has none:
#   OC_CHECK_FLAGS = $(if $(filter 0x800/0/0/0/0,$(HOT_DATA_OFFSET)/$(HOT_CODE_OFFSET)/$(HOT_CORE_PAD)/$(HOT_MID_PAD)/$(RODATA_OFFSET)),,--warn)
OC_CHECK_FLAGS = --warn
# HOT_SLOTS_LOOSE=1: the hot slots' budget checks off (tools/hot_slots.py), for diagnostic builds
HOT_SLOTS_LOOSE ?= 0
# the heap's start (_end) modulo 16 kB, relative to HOT_DATA_OFFSET (tools/shlelf_hot.xc)
HEAP_AT_RELEASE = 0x18f0
HEAP_AT = $(HEAP_AT_RELEASE)
# .bss's layout (tools/bss_order/release.ld: each section at its offset in a reference link): LTO's partitions follow
# the program's size, and .bss their order
BSS_ORDER = release
# .rodata's fixed address (tools/shlelf_hot.xc): the 16 kB window it starts in, at its usual 0x0808 phase (relative to
# HOT_DATA_OFFSET), whatever the code before it adds up to, so code growing moves neither .rodata nor anything after it
# (.hotdata, .data, .bss, the heap) until it reaches it: the link stops there (an ASSERT). tools/data_budget.py reports
# the room. 0 (the debug build and HOT_SLOTS_LOOSE=1 diagnostic builds): right after the code at the next 0x0808 phase.
# (.hotdata goes at a fixed 16 kB block FRNES_RODATA_SPAN past the window: tools/shlelf_hot.xc; players built with
# PLAYBACK / RECORD have their window 16 kB later)
# (a placed build's hot code (PLACE) takes up to 0x3c000 more with the space between its sections: 256 kB later, 16
# operand cache periods, every index the same)
RODATA_WINDOW_RELEASE = $(if $(PLACE),$(if $(PLAYBACK)$(RECORD),0x8c0f4000,0x8c0f0000),$(if $(PLAYBACK)$(RECORD),0x8c0b4000,0x8c0b0000))
RODATA_SPAN_RELEASE = 0x6c000
RODATA_WINDOW = $(if $(filter 1,$(HOT_SLOTS_LOOSE)),0,$(RODATA_WINDOW_RELEASE))
HOT_DATA_LDFLAGS = -Wl,--defsym=FRNES_HEAP_AT=$(HEAP_AT) -Wl,--defsym=FRNES_RODATA_WINDOW=$(RODATA_WINDOW) -Wl,--defsym=FRNES_HOT_SLOTS_LOOSE=$(HOT_SLOTS_LOOSE) -Wl,--defsym=FRNES_HOT_DATA_OFFSET=$(HOT_DATA_OFFSET) -Wl,--defsym=FRNES_HOT_CODE_OFFSET=$(HOT_CODE_OFFSET) -Wl,--defsym=FRNES_HOT_CORE_PAD=$(HOT_CORE_PAD) -Wl,--defsym=FRNES_HOT_MID_PAD=$(HOT_MID_PAD) -Wl,--defsym=FRNES_RODATA_OFFSET=$(RODATA_OFFSET) \
	-Wl,-Map=$(dir $@)map.txt

SRCS = main.c player_main.c frnes_settings.c vmu_save.c save_indicator.c frame_hud.c cartridge.c cpu.c cpu_exact.c ppu.c apu.c apu_synth.c video.c memory.c mapper.c mapper_boards.c fds.c nsf.c game_menu.c 6502_readbyte.c 6502_readword.c 6502_writebyte.c input_recording.c state.c state_hotkeys.c gamegenie.c state_core.c movie.c frame_hud_perf.c mapper_mmc3_sync.c input_ports.c kos_mainline.c
ASMS = 6502_core.s
OBJS = $(SRCS:.c=.o) $(ASMS:.s=.o) romdisk.o 
PROJECTNAME = SH4_6502
ELFFILE = $(PROJECTNAME).elf
BINFILE = $(PROJECTNAME).bin

#DEBUG BUILD SETTINGS
DBGDIR = debug
DBGEXE = $(DBGDIR)/$(ELFFILE)
DBGOBJS = $(addprefix $(DBGDIR)/, $(OBJS))
DBGCFLAGS = -g -O3 -DDEBUG

#RELEASE BUILD SETTINGS
RELDIR = release
RELEXE = $(RELDIR)/$(ELFFILE)
RELBIN = $(RELDIR)/$(BINFILE)
RELOBJS = $(addprefix $(RELDIR)/, $(OBJS))
# player options for main.c:
#   ROM=/pc/roms/mm3.nes      ROM to run (default: Super Mario Bros from the romdisk), /pc/ is dc-tool-ip's -m directory
#   RECORD=1                  record the controller from power on, saved to /pc/recordings/ (see main.c)
#   PLAYBACK=/pc/recordings/x.frm   replay a recording instead of reading the controller
REL_FLAGS =
# the frontend's movie import (movie_bk2.c: .bk2 to .frm before the emulator's handoff) inflates with KOS's zlib port
# (its header isn't in kos-ports/include); only the frontend has it (12 kB with zlib's inflate)
MOVIE_ZLIB_INCLUDE = -I$(or $(KOS_PORTS),$(KOS_BASE)/../kos-ports)/zlib/inst/include
frontend_build/movie_bk2.o: KOS_CFLAGS += $(MOVIE_ZLIB_INCLUDE)
# the player builds' synthesizer frame out of line (apu_synth.c's apu_synth_frame)
REL_FLAGS += -DFRNES_SYNTH_OUTLINE
ifdef ROM
REL_FLAGS += -DFRNES_ROM_PATH='"$(ROM)"'
endif
ifdef RECORD
REL_FLAGS += -DFRNES_RECORD
endif
# HUD=0: no frame display
ifeq ($(HUD),0)
REL_FLAGS += -DFRNES_FRAME_HUD=0
else
# (the display's expansion audio segment)
REL_FLAGS += -DAPU_SYNTH_TIME_EXPANSION
endif
ifdef NO_SAVE
REL_FLAGS += -DFRNES_NO_SAVE
endif
ifdef AUTOSAVE_TEST
REL_FLAGS += -DFRNES_AUTOSAVE_TEST=$(AUTOSAVE_TEST)
endif
ifdef VMU_BACKUP
REL_FLAGS += -DFRNES_VMU_BACKUP
endif
ifdef MAX_FRAMES
REL_FLAGS += -DFRNES_MAX_FRAMES=$(MAX_FRAMES)
endif
# FPS_LOG=1: keep the console on while the game runs (the FPS and HUD lines; main.c's CONSOLE_QUIET for why it's off)
ifdef FPS_LOG
REL_FLAGS += -DFRNES_FPS_LOG
endif
# SOAK=rounds: play a list of ROMs one after another that many times, through frnes_run (main.c); with MAX_FRAMES
ifdef SOAK
REL_FLAGS += -DFRNES_SOAK=$(SOAK)
endif
# HANG_TEST=N: hang with interrupts masked at frame N (for the watchdog in record / playback builds)
ifdef HANG_TEST
REL_FLAGS += -DFRNES_HANG_TEST=$(HANG_TEST)
endif
# HW_WATCHDOG=1: record / playback builds arm the SH4's watchdog, which resets the console on a hang (main.c)
ifdef HW_WATCHDOG
REL_FLAGS += -DFRNES_HW_WATCHDOG
endif
# NO_FPS_PRINT=1: no FPS line every few seconds (a lost dc-load exchange during the printf can hang the console)
ifdef NO_FPS_PRINT
REL_FLAGS += -DFRNES_NO_FPS_PRINT
endif
ifdef PLAYBACK
REL_FLAGS += -DFRNES_PLAYBACK_PATH='"$(PLAYBACK)"'
endif
# MOVIE_HASH=1 (with PLAYBACK): each frame's RAM / CPU register hash, printed after the movie (main.c's MOVIE HASH lines)
ifdef MOVIE_HASH
REL_FLAGS += -DFRNES_MOVIE_HASH
# PERF_EVENT=0x08 (with MOVIE_HASH=1): that performance counter event over each frame's work, the MOVIE PERF lines
ifdef PERF_EVENT
REL_FLAGS += -DFRNES_PERF_EVENT=$(PERF_EVENT)
endif
endif

#CDI BUILD SETTINGS
ISODIR = iso
ISOBIN = $(ISODIR)/1ST_READ.BIN
CDIDIR = cdi
CDIISO = $(CDIDIR)/$(PROJECTNAME).iso
CDIIPBIN = $(CDIDIR)/IP.BIN
CDIBOOTSTRAPISO = $(CDIDIR)/$(PROJECTNAME)-bootstrap.iso
CDIFILE = $(CDIDIR)/$(PROJECTNAME).cdi

.DEFAULT_GOAL := release

prepdbgdir:
	@mkdir -p $(DBGDIR)

prepreldir:
	@mkdir -p $(RELDIR)

prepcdidir:
	@mkdir -p $(CDIDIR) $(ISODIR)

debug: $(DBGEXE)

$(DBGEXE): RODATA_WINDOW = 0
$(DBGEXE): $(DBGOBJS) $(DBGDIR)/frnes_hot_data.ld tools/hot_slots.ld tools/bss_pins.ld $(wildcard tools/bss_order/*.ld) tools/shlelf_hot.xc
	cp tools/bss_order/$(BSS_ORDER).ld $(DBGDIR)/bss_order.ld
	: > $(DBGDIR)/frnes_hud_ranges.ld
	: > $(DBGDIR)/frnes_data_place.ld; : > $(DBGDIR)/frnes_data_place_end.ld
	$(KOS_CC) $(KOS_CFLAGS) $(DBGCFLAGS) $(KOS_LDFLAGS) -L$(DBGDIR) $(HOT_DATA_LDFLAGS) $(KOS_START) -o $(DBGEXE) $(DBGOBJS) -lpng -lz -lm -lbz2 -lkosfat -lsh4zam $(KOS_LIBS)

$(DBGDIR)/%.o: %.c | prepdbgdir
	$(KOS_CC) -c $(KOS_CFLAGS) $(DBGCFLAGS) $(KOS_LDFLAGS) -o $@ $<

$(DBGDIR)/%.o: %.s | prepdbgdir
	$(KOS_CC) -c $(KOS_CFLAGS) $(DBGCFLAGS) $(KOS_LDFLAGS) -o $@ $<

# mapper_boards.c, fds.c and the save state chunks (state_core.c) are built without LTO: their code can't change the
# hot paths' inlining (see mapper_boards.c, state_core.c)
# (mapper_boards.c with FRNES_PLAYER_BUILD in the player builds, its player-only code: the Zapper's controls and reticle)
$(DBGDIR)/mapper_boards.o: mapper_boards.c | prepdbgdir
	$(KOS_CC) -c $(KOS_CFLAGS) $(DBGCFLAGS) $(KOS_LDFLAGS) -fno-lto -DFRNES_PLAYER_BUILD -o $@ $<

# (input_ports.c: the player builds' controllers, cold, without LTO as mapper_boards.c)
$(DBGDIR)/input_ports.o: input_ports.c | prepdbgdir
	$(KOS_CC) -c $(KOS_CFLAGS) $(DBGCFLAGS) $(KOS_LDFLAGS) -fno-lto -o $@ $<

$(DBGDIR)/fds.o: fds.c | prepdbgdir
	$(KOS_CC) -c $(KOS_CFLAGS) $(DBGCFLAGS) $(KOS_LDFLAGS) -fno-lto -o $@ $<

$(DBGDIR)/nsf.o: nsf.c | prepdbgdir
	$(KOS_CC) -c $(KOS_CFLAGS) $(DBGCFLAGS) $(KOS_LDFLAGS) -fno-lto -o $@ $<

$(DBGDIR)/game_menu.o: game_menu.c | prepdbgdir
	$(KOS_CC) -c $(KOS_CFLAGS) $(DBGCFLAGS) $(KOS_LDFLAGS) -fno-lto -o $@ $<

$(DBGDIR)/kos_mainline.o: kos_mainline.c | prepdbgdir
	$(KOS_CC) -c $(KOS_CFLAGS) $(DBGCFLAGS) $(KOS_LDFLAGS) -fno-lto -o $@ $<

$(DBGDIR)/state_core.o: state_core.c | prepdbgdir
	$(KOS_CC) -c $(KOS_CFLAGS) $(DBGCFLAGS) $(KOS_LDFLAGS) -fno-lto -o $@ $<

$(DBGDIR)/frame_hud_perf.o: frame_hud_perf.c | prepdbgdir
	$(KOS_CC) -c $(KOS_CFLAGS) $(DBGCFLAGS) $(KOS_LDFLAGS) -fno-lto -fno-jump-tables -Os -o $@ $<

$(DBGDIR)/mapper_mmc3_sync.o: mapper_mmc3_sync.c | prepdbgdir
	$(KOS_CC) -c $(KOS_CFLAGS) $(DBGCFLAGS) $(KOS_LDFLAGS) -fno-lto -fno-jump-tables -Os -o $@ $<

$(DBGDIR)/romdisk.o: romdisk.o
	cp romdisk.o $(DBGDIR)/romdisk.o

release: $(RELEXE)

$(RELEXE): HOT_DATA_LDFLAGS += $(if $(filter 1,$(HOT_SLOTS_LOOSE)),,-Wl,--defsym=FRNES_RODATA_SPAN=$(RODATA_SPAN_RELEASE))

%/shlelf_place.xc: tools/shlelf_hot.xc $(PLACE)
	mkdir -p $(dir $@)
	sed -e 's#INCLUDE tools/hot_slots.ld#INCLUDE $(abspath $(PLACE))#' tools/shlelf_hot.xc > $@

$(RELEXE): override KOS_LDFLAGS := $(if $(PLACE),$(subst $(HOT_XC),$(RELDIR)/shlelf_place.xc,$(KOS_LDFLAGS)),$(KOS_LDFLAGS))

$(RELEXE): $(if $(PLACE),$(RELDIR)/shlelf_place.xc) $(RELOBJS) $(RELDIR)/frnes_hot_data.ld tools/hot_slots.ld tools/bss_pins.ld $(wildcard tools/bss_order/*.ld) tools/shlelf_hot.xc tools/hot_frame.txt tools/hot_text.txt tools/hot_kos.txt tools/oc_conflict_check.py
	cp tools/bss_order/$(BSS_ORDER).ld $(RELDIR)/bss_order.ld
	$(if $(PLACE),cp $(basename $(PLACE)).hud.ld,: >) $(RELDIR)/frnes_hud_ranges.ld
	$(if $(DATA_PLACE),cp $(DATA_PLACE),: >) $(RELDIR)/frnes_data_place.ld; $(if $(DATA_PLACE),cp $(DATA_PLACE:.data.ld=.data_end.ld),: >) $(RELDIR)/frnes_data_place_end.ld
	$(KOS_CC) $(KOS_CFLAGS) $(KOS_LDFLAGS) -L$(RELDIR) $(HOT_DATA_LDFLAGS) $(KOS_START) -o $(RELEXE) $(RELOBJS) -lpng -lz -lm -lbz2 -lkosfat -lsh4zam $(KOS_LIBS)
	[ -n "$(PLACE)" ] || python3 tools/hot_frame_check.py $(RELDIR)/map.txt $(HOT_DATA_SRCS)
	[ "$(HOT_SLOTS_LOOSE)" = 1 ] || [ -n "$(PLACE)" ] || python3 tools/hot_text_check.py $(RELEXE) $(RELDIR)/map.txt
	python3 tools/data_budget.py $(RELDIR)/map.txt
	[ "$(HOT_SLOTS_LOOSE)" = 1 ] || python3 tools/oc_conflict_check.py $(OC_CHECK_FLAGS) $(if $(PLACE),--warn) $(RELEXE) $(RELDIR)/map.txt
	$(KOS_STRIP) $(RELEXE)
	$(KOS_OBJCOPY) -R .stack -O binary $(RELEXE) $(RELBIN)

$(RELDIR)/%.o: %.c | prepreldir
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -o $@ $<

$(RELDIR)/main.o: main.c FORCE | prepreldir
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) $(REL_FLAGS) -o $@ $<

# (main() for player builds, with LTO as integration's main() was: built without, it changed LTO's partitions and
# with them the hot slots' code, exact_process_dma by 0x170 bytes)
$(RELDIR)/player_main.o: player_main.c FORCE | prepreldir
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) $(REL_FLAGS) -o $@ $<

# (the player options that reach past main.c: the frame display's expansion audio timing, the autosave test's
# compression timing)
$(RELDIR)/apu_synth.o: apu_synth.c FORCE | prepreldir
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) $(REL_FLAGS) -o $@ $<

$(RELDIR)/vmu_save.o: vmu_save.c FORCE | prepreldir
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) $(REL_FLAGS) -o $@ $<

$(RELDIR)/%.o: %.s | prepreldir
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -o $@ $<

$(RELDIR)/mapper_boards.o: mapper_boards.c | prepreldir
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -fno-lto -DFRNES_PLAYER_BUILD -o $@ $<

$(RELDIR)/input_ports.o: input_ports.c | prepreldir
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -fno-lto -o $@ $<

$(RELDIR)/fds.o: fds.c | prepreldir
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -fno-lto -o $@ $<

$(RELDIR)/nsf.o: nsf.c | prepreldir
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -fno-lto -o $@ $<

$(RELDIR)/game_menu.o: game_menu.c | prepreldir
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -fno-lto -o $@ $<

$(RELDIR)/kos_mainline.o: kos_mainline.c | prepreldir
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -fno-lto -o $@ $<

$(RELDIR)/state_core.o: state_core.c | prepreldir
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -fno-lto -o $@ $<

$(RELDIR)/frame_hud_perf.o: frame_hud_perf.c FORCE | prepreldir
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) $(REL_FLAGS) -fno-lto -fno-jump-tables -Os -o $@ $<

$(RELDIR)/mapper_mmc3_sync.o: mapper_mmc3_sync.c FORCE | prepreldir
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) $(REL_FLAGS) -fno-lto -fno-jump-tables -Os -o $@ $<

$(RELDIR)/romdisk.o: romdisk.o
	cp romdisk.o $(RELDIR)/romdisk.o

FORCE:

# the hot code's slots (committed; regenerated when the table or the per-frame list changes)
tools/hot_slots.ld: tools/hot_slots.py tools/hot_frame.txt
	python3 tools/hot_slots.py $@

%/frnes_hot_data.ld: $(HOT_DATA_SRCS) tools/hot_data.py
	@mkdir -p $(dir $@)
	python3 tools/hot_data.py $@ "$(KOS_CC)" "$(KOS_CFLAGS)" $(HOT_DATA_SRCS)

#FRONTEND BUILD SETTINGS (make frontend: the menus in frontend/ over the emulator, release/FrNES.elf)
# The emulator's objects are built as for release (main.c with FRNES_FRONTEND, which leaves out the player's main);
# the romdisk has the menus' font and the ROMs in romdisk/
FEDIR = frontend_build
FEEXE = $(FEDIR)/FrNES.elf
FESRCS = $(wildcard frontend/*.c)
# MONO_FONT=/rd/name.fnt: another monospace font for the lists (its .fnt and pages in frontend/assets)
ifdef MONO_FONT
FE_FONT_FLAGS = -DFRNES_MONO_FONT='"$(MONO_FONT)"'
endif
# (sd_card.c the menus' alone: in the player's link it moved the hot slots' code, 9_write by 0xb8 bytes)
FEOBJS = $(addprefix $(FEDIR)/, $(filter-out player_main.o, $(SRCS:.c=.o)) sd_card.o movie_bk2.o $(ASMS:.s=.o) $(FESRCS:.c=.o)) $(FEDIR)/romdisk.o

frontend: $(FEEXE)

# (the menus' build: a fixed .rodata window of its own, as the release's, 32 kB past where .rodata followed its code
# (0x8c0dc808 at frontend-sd 44c93e80 with 60c4129e), and .hotdata 64 kB later than its next 16 kB block was then
# (0x8c16c000), through its own FRNES_RODATA_SPAN: the menus' .rodata (fonts, art) is ~0x7c000 bytes, more than the
# release's span holds. Code growth then moves neither .rodata nor .hotdata, .data, .bss and the heap's start; the link
# stops when the code reaches the window, or .rodata and the cold code reach .hotdata, tools/data_budget.py reporting
# the room in both; the span 64 kB more since, 0x98000 -> 0xa8000, for the rest of the mappers' boards)
RODATA_WINDOW_FRONTEND = $(if $(PLACE),0x8c13c000,0x8c0e4000)
RODATA_SPAN_FRONTEND = 0xa8000
$(FEEXE): override KOS_LDFLAGS := $(if $(PLACE),$(subst $(HOT_XC),$(FEDIR)/shlelf_place.xc,$(KOS_LDFLAGS)),$(KOS_LDFLAGS))
$(FEEXE): RODATA_WINDOW = $(if $(filter 1,$(HOT_SLOTS_LOOSE)),0,$(RODATA_WINDOW_FRONTEND))
$(FEEXE): HOT_DATA_LDFLAGS += -Wl,--defsym=FRNES_RODATA_SPAN=$(RODATA_SPAN_FRONTEND)
$(FEEXE): $(if $(PLACE),$(FEDIR)/shlelf_place.xc) $(FEOBJS) $(FEDIR)/frnes_hot_data.ld tools/hot_slots.ld $(wildcard tools/bss_order/*.ld) tools/shlelf_hot.xc tools/hot_frame.txt
	# (the emulator's .bss in the release's order: the frontend's comes after it, tools/shlelf_hot.xc)
	cp tools/bss_order/$(BSS_ORDER).ld $(FEDIR)/bss_order.ld
	$(if $(PLACE),cp $(basename $(PLACE)).hud.ld,: >) $(FEDIR)/frnes_hud_ranges.ld
	$(if $(DATA_PLACE),cp $(DATA_PLACE),: >) $(FEDIR)/frnes_data_place.ld; $(if $(DATA_PLACE),cp $(DATA_PLACE:.data.ld=.data_end.ld),: >) $(FEDIR)/frnes_data_place_end.ld
	$(KOS_CC) $(KOS_CFLAGS) $(KOS_LDFLAGS) -L$(FEDIR) $(HOT_DATA_LDFLAGS) $(KOS_START) -o $(FEEXE) $(FEOBJS) -lpng -lz -lm -lbz2 -lkosfat -lsh4zam $(KOS_LIBS)
	[ -n "$(PLACE)" ] || python3 tools/hot_frame_check.py $(FEDIR)/map.txt $(HOT_DATA_SRCS)
	$(KOS_STRIP) $(FEEXE)

# the menus are built without LTO: linked into the emulator's LTO they changed its inlining and where its hot data
# went (before the general rule: make 3.81 takes the first that matches)
$(FEDIR)/frontend/%.o: frontend/%.c FORCE
	@mkdir -p $(dir $@)
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) $(FE_FONT_FLAGS) -fno-lto -MMD -MP -o $@ $<

$(FEDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -MMD -MP -o $@ $<

$(FEDIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -o $@ $<

# (the player options reach these as in release, so the emulator is built the same way)
$(FEDIR)/main.o: main.c FORCE
	@mkdir -p $(dir $@)
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) $(REL_FLAGS) -DFRNES_FRONTEND -MMD -MP -o $@ $<

$(FEDIR)/apu_synth.o: apu_synth.c FORCE
	@mkdir -p $(dir $@)
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) $(REL_FLAGS) -MMD -MP -o $@ $<

$(FEDIR)/vmu_save.o: vmu_save.c FORCE
	@mkdir -p $(dir $@)
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) $(REL_FLAGS) -MMD -MP -o $@ $<

$(FEDIR)/sd_card.o: sd_card.c
	@mkdir -p $(dir $@)
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -fno-lto -MMD -MP -o $@ $<

$(FEDIR)/mapper_boards.o: mapper_boards.c
	@mkdir -p $(dir $@)
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -fno-lto -DFRNES_PLAYER_BUILD -MMD -MP -o $@ $<

$(FEDIR)/input_ports.o: input_ports.c
	@mkdir -p $(dir $@)
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -fno-lto -MMD -MP -o $@ $<

$(FEDIR)/fds.o: fds.c
	@mkdir -p $(dir $@)
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -fno-lto -MMD -MP -o $@ $<

$(FEDIR)/frame_hud_perf.o: frame_hud_perf.c
	@mkdir -p $(dir $@)
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -fno-lto -fno-jump-tables -Os -MMD -MP -o $@ $<

$(FEDIR)/mapper_mmc3_sync.o: mapper_mmc3_sync.c
	@mkdir -p $(dir $@)
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -fno-lto -fno-jump-tables -Os -MMD -MP -o $@ $<

$(FEDIR)/nsf.o: nsf.c
	@mkdir -p $(dir $@)
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -fno-lto -MMD -MP -o $@ $<

$(FEDIR)/game_menu.o: game_menu.c
	@mkdir -p $(dir $@)
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -fno-lto -MMD -MP -o $@ $<

$(FEDIR)/kos_mainline.o: kos_mainline.c
	@mkdir -p $(dir $@)
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -fno-lto -MMD -MP -o $@ $<

# (genromfs takes one directory: the font and palette from frontend/assets, the ROMs from romdisk/)
# (rebuilt every time: ROM names have spaces, which make can't take as prerequisites)
$(FEDIR)/romdisk.o: FORCE
	@rm -rf $(FEDIR)/romdisk && mkdir -p $(FEDIR)/romdisk
	cp -R frontend/assets/ $(FEDIR)/romdisk/
	-cp romdisk/* $(FEDIR)/romdisk/ 2>/dev/null
	$(KOS_GENROMFS) -f $(FEDIR)/romdisk.img -d $(FEDIR)/romdisk -v -x .gitignore -x .DS_Store
	$(KOS_BASE)/utils/bin2c/bin2c $(FEDIR)/romdisk.img $(FEDIR)/romdisk_tmp.c romdisk
	$(KOS_CC) $(KOS_CFLAGS) -o $(FEDIR)/romdisk_tmp.o -c $(FEDIR)/romdisk_tmp.c
	$(KOS_CC) -o $@ -r $(FEDIR)/romdisk_tmp.o $(KOS_LIB_PATHS) -Wl,--whole-archive -lromdiskbase
	rm $(FEDIR)/romdisk_tmp.c $(FEDIR)/romdisk_tmp.o

-include $(FEOBJS:.o=.d)

# FrNES's disc, cdi/$(PROJECTNAME).cdi: the frontend (make frontend, without the dev builds' /pc paths) as 1ST_READ.BIN, and
# whatever else is in iso/ (FrNES/art, FrNES/disksys.rom, your ROMs: docs/box-art-and-manuals.md). Joliet and Rock
# Ridge for the art's folder names (up to ~60 characters; plain ISO 9660 cuts them at 31). make cdi-player: the
# stand-alone player's (release/) disc instead
CDI_TITLE = FRNES
cdi: frontend | prepcdidir
	$(KOS_OBJCOPY) -R .stack -O binary $(FEEXE) $(FEDIR)/FrNES.bin
	$(KOS_BASE)/utils/scramble/scramble $(FEDIR)/FrNES.bin $(ISOBIN)
	$(MAKE) cdi-image

cdi-player: release | prepcdidir
	$(KOS_BASE)/utils/scramble/scramble $(RELBIN) $(ISOBIN)
	$(MAKE) cdi-image

# (iso/ as an image, its bootstrap, and the self-booting CDI. The bootstrap: ip.txt's fields when there is one, else
# makeip's defaults with FrNES's title; iplogo.png, if there is one, as its logo)
CDI_IPLOGO = $(if $(wildcard iplogo.png),-l iplogo.png)
cdi-image: | prepcdidir
	mkisofs -V FrNES -l -J -joliet-long -r -C 0,11702 -o $(CDIISO) ./$(ISODIR)
	$(if $(wildcard ip.txt),makeip -f $(CDI_IPLOGO) ip.txt $(CDIIPBIN),makeip -f $(CDI_IPLOGO) -g "$(CDI_TITLE)" $(CDIIPBIN))
	( cat $(CDIIPBIN); dd if=$(CDIISO) bs=2048 skip=16 ) > $(CDIBOOTSTRAPISO)
	cdi4dc $(CDIBOOTSTRAPISO) $(CDIFILE)

clean:
	# (iso/ keeps what you put on the disc: FrNES/art, your ROMs; only its 1ST_READ.BIN goes)
	rm -rf debug release cdi $(ISOBIN) $(FEDIR) romdisk.*

