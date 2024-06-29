FrNES is an NES emulator for the Sega Dreamcast console.  It was based around Racoon's pNESx emulator for the original PlayStation.

In 2000, I had been working on a port of Hu-Go the PC Engine emulator to the Dreamcast and ran into a few issues with the port.  I found the pNESx source online and decided to take my working menu code from the Hu-Go port and hook it up to the pNESx emulator core.  The initial port took only a few hours to get running as the playstation code translated quite well to the libdream APIs which were available at the time.  A month or so later I released the emulator to the DC Scene and spent the next year or so improving elements of the emulator.  I added the Nofrendo sound code and implemented some of the more popular mappers, and called it quits after a Christmas release of the emulator in 2002.

In 2013 I decided to dig out the source code and try to build it against KOS 2.0.0.  Over the years I brought it up to date with modern KOS.  I also eventually incorporated the VQFB technique for rendering, 
better sound code, a more modern file browser and fixed a number of CPU opcode and address mode issues.  I released this updated version as version 0.7 on June 28, 2024.

