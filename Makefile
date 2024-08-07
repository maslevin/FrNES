# KallistiOS 2.0.0
#
# FrNES/Makefile
# (c)2000-2022 Matt Slevinsky
#

KOS_ROMDISK_DIR = romdisk
include $(KOS_BASE)/Makefile.rules

SRCS = nes_apu.c aica_fw.c font.c GUI_MainMenu.c GUI_Credits.c GUI_FileBrowser.c GUI_Help.c GUI_ControlPage.c GUI_GUIPage.c GUI_SystemPage.c GUI_VideoPage.c K6502_rw.c K6502.c pNesX_DrawLine_BG_C_Map9.c pNesX_DrawLine_BG_C.c pNesX_DrawLine_Spr_C_Map9.c pNesX_DrawLine_Spr_C.c pNesX_Mapper.c pNesX_PPU_DC.c pNesX_Sound_APU.c pNesX_System_DC.c pNesX_Utils.c pNesX.c ROMLoad.c TextWindow.c

ASMS = 
OBJS = $(SRCS:.c=.o) $(ASMS:.s=.o) romdisk.o 
PROJECTNAME = FrNES
ELFFILE = $(PROJECTNAME).elf
BINFILE = $(PROJECTNAME).bin

#DEBUG BUILD SETTINGS
DBGDIR = debug
DBGEXE = $(DBGDIR)/$(ELFFILE)
DBGOBJS = $(addprefix $(DBGDIR)/, $(OBJS))
DBGCFLAGS = -g -O0 -DDEBUG

#RELEASE BUILD SETTINGS
RELDIR = release
RELEXE = $(RELDIR)/$(ELFFILE)
RELBIN = $(RELDIR)/$(BINFILE)
RELOBJS = $(addprefix $(RELDIR)/, $(OBJS))

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

$(DBGEXE): $(DBGOBJS)
	$(KOS_CC) $(KOS_CFLAGS) $(DBGCFLAGS) $(KOS_LDFLAGS) $(KOS_START) -o $(DBGEXE) $^ -lpng -lz -lm -lbz2 -lkosfat $(KOS_LIBS)

$(DBGDIR)/%.o: %.c | prepdbgdir
	$(KOS_CC) -c $(KOS_CFLAGS) $(DBGCFLAGS) $(KOS_LDFLAGS) -o $@ $<

$(DBGDIR)/%.o: %.s | prepdbgdir
	$(KOS_CC) -c $(KOS_CFLAGS) $(DBGCFLAGS) $(KOS_LDFLAGS) -o $@ $<

$(DBGDIR)/romdisk.o: romdisk.o
	cp romdisk.o $(DBGDIR)/romdisk.o

release: $(RELEXE)

$(RELEXE): $(RELOBJS)
	$(KOS_CC) $(KOS_CFLAGS) $(KOS_LDFLAGS) $(KOS_START) -o $(RELEXE) $^ -lpng -lz -lm -lbz2 -lkosfat $(KOS_LIBS)
	$(KOS_STRIP) $(RELEXE)
	$(KOS_OBJCOPY) -R .stack -O binary $(RELEXE) $(RELBIN)

$(RELDIR)/%.o: %.c | prepreldir
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -o $@ $<

$(RELDIR)/%.o: %.s | prepreldir
	$(KOS_CC) -c $(KOS_CFLAGS) $(KOS_LDFLAGS) -o $@ $<

$(RELDIR)/romdisk.o: romdisk.o
	cp romdisk.o $(RELDIR)/romdisk.o

cdi: release | prepcdidir
	/opt/toolchains/dc/kos/utils/scramble/scramble $(RELBIN) $(ISOBIN)
	mkisofs -V $(PROJECTNAME) -l -C 0,11702 -o $(CDIISO) ./$(ISODIR)
	makeip ip.txt $(CDIIPBIN)
	( cat $(CDIIPBIN); dd if=$(CDIISO) bs=2048 skip=16 ) > $(CDIBOOTSTRAPISO)
	cdi4dc $(CDIBOOTSTRAPISO) $(CDIFILE)

clean:
	rm -rf debug release cdi iso romdisk.*

