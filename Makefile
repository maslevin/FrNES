# KallistiOS 2.0.0
#
# FrNES 1.0/Makefile
# (c)2014 Matt Slevinsky
#

all: rm-elf frnes.elf

KOS_ROMDISK_DIR = romdisk
include $(KOS_BASE)/Makefile.rules

#OBJS = pNesX_System_DC.o pNesX_Utils.o TextWindow.o BMFFile.o BMF2PVR.o romdisk.o
OBJS = pNesX_System_DC.o BMFont.o romdisk.o TextWindow.o nesrom.o

clean:
	rm -f frnes.elf $(OBJS)

rm-elf:
	rm -f frnes.elf
	rm -f romdisk.img

frnes.elf: $(OBJS)
	kos-cc -o frnes.elf $(OBJS) -lpng -lz -lm

run: frnes.elf
	$(KOS_LOADER) frnes.elf

dist:
	rm -f $(OBJS) romdisk.*
	$(KOS_STRIP) frnes.elf
