# FrNES compatibility and known issues

What doesn't work yet, and why. Mapper numbers are iNES / NES 2.0 ("12.1" is mapper 12, submapper 1).

## Known game issues

- **三國志 – 群雄爭霸 (Sangokushi, CRC32 `FB2F949F`, mapper 112):** hangs on the title screen (Start does nothing).
  It is the same NMI timing case as Battletoads TAS 4271: an NMI 6 dots before the end of a taken branch that stays on
  the same page. NesHawk always delays this NMI and Mesen 2 never does. FrNES follows NesHawk and the nesdev wiki,
  because removing the delay desyncs that TAS at frame 316.
- **TAS movies with soft resets** may desync. Dr. Mario #4058 depends on NesHawk's own reset behaviour: FrNES syncs 4 of
  its 20 resets, and Mesen 2 syncs 1.
- **Super Card 14-in-1 (King007, mapper 395): Super Contra 6** reaches its reset code, then idles with the menu still
  on screen. The cart's other games start, and no emulator has the board to check it against.
- **Dragon Ball Z 外伝 サイヤ人絶滅計画 (YY0807, mapper 529)** stays black, as it does in Mesen 2: it saves to a 93C56
  serial EEPROM, which isn't emulated.
- **СЮБОР ОБУЧАЮЩИЙ КОМПЬЮТЕР (mapper 167)** reaches its BASIC screen but expects the Subor keyboard.
- **吞食天地 三国外传's PAL dump (mapper 198)** corrupts its CHR RAM uploads (see PAL timing below); the NTSC dump plays.

## Mappers and features not supported yet

| Mapper / feature | Reason |
|---|---|
| 96 (Oeka Kids) | CHR banks switch on the PPU's nametable fetch addresses, which needs a PPU fetch hook |
| 547 (Konami QTa / VRC5) | each background tile reads the board's QTRAM to choose between CHR RAM and a 1 bpp kanji ROM (with a synthesised second plane): a per-tile hook in the renderer's background fetch, hot PPU code |
| 209's CHR latch | MMC2-style latch switched by the PPU's pattern fetches, which needs a PPU fetch hook |
| 12.1 (Magic Card 4M) extras | the copier hardware beyond its banking and timer IRQ is not emulated |
| 165 | MMC3 with an MMC2-style CHR latch, which needs a PPU fetch hook |
| Dendy and PAL timing | Dendy and PAL dumps run at NTSC timing. Most play, but a few depend on the longer frame (198's PAL dump) |
| VT03 / 256 (OneBus) | the enhanced VT03 PPU and APU are not emulated |
| 405 | the UM6578's enhanced mode is not emulated |
| 168 (Racermate) | 64 kB of CHR including NVRAM, and the exercise bike's input |
| 116 | a large multi-mode board, not done yet |
| 380 | no reference to check it against |
| 411 | its documentation is ambiguous |
| 428 | undocumented |
| 42.2 (Green Beret, LH09) | the submapper's 160 kB board is undocumented and Mesen 2 shows garbage too, so there is no reference to check it against |
| 235: the Golden Game 1500-in-1 | its menu never comes up, and Mesen 2 shows garbage too, so there is no reference to check it against |
| 217 (Unchained Melody 500 / 2000-in-1, 21-in-1, Super 9999-in-1) | no working reference: Mesen 2's board (FCEUX's registers) shows garbage or a blank screen on all 4 dumps tested |
| 218 (Magic Floor, Starfight) | its CHR is the nametable RAM: a nametable write would have to change the pattern data the PPU fetches, which is PPU code |
| 403: the Sachen Superpacks (A, B, C) | their menus come up, but games don't start from them (A's first game shows only solid colours); no reference emulator has the board |
| 55 (the Malee SMB1 bootlegs: Fly Merio, Genius Merio, Malee 2) | the game boots from its $6000 ROM, then lands on its IRQ vector in the first frame here; Mesen 2 doesn't load the iNES dumps, so there's no reference |
| 303 (Kaiser KS-7017: Armana no Kiseki) | a board written from Mesen 2's registers (the FDS-style IRQ timer at $4020 / $4021, $4030) boots, but after the first timer IRQs its RAM code takes another path than in Mesen 2 and crashes at frame 22; cause not found yet |
| 27 (迷糊車, 奇妙車, 迷魂車) | a VRC4 with its register select on A0 / A1: run as mapper.c's 23 the top of the screen plays but the rest stays a fixed pattern, and Mesen 2's (untested) VRC4_27 shows a blank screen, so no reference |
| 234 (Maxi 15) | its registers also react to reads of $FF80-$FF9F, ROM reads in the CPU's hot path |
| 147 (Sachen 3018: Challenge of the Dragon, Chinese Kungfu) | a board as Mesen 2's (the TXC chip's JV001 form, the data rotated by 2) gives the same register writes as Mesen 2 up to the game's start; after it the game's main loop makes more bank switch rounds per frame than in Mesen 2 (no $2002 polling in it) and the playfield's graphics differ; not found yet |
| 304: Super Mario Bros. 2 (LE10), Zanac A.I. (LF11) | blank or crashing here and blank in Mesen 2 too (its Volleyball LE08 runs in both) |
| 199 | none of the 5 dumps tested run in Mesen 2 either (blank screen), so there is nothing to check the board against |
| 561 / 562 (Game Doctor copier dumps) | 108 of the 492 dumps tested still jam or stay dark (Dragon Quest IV, Quarth, Q*bert, Fantasy Zone II, Marble Madness, Sansara Naga and others), most likely other lockout bytes or copier behaviour the wiki doesn't describe |
| 518 (Dance 2000, Subor learning cards) | CHR switches on the PPU's nametable addresses, which needs a PPU hook |
| 342 (Coolgirl) | a programmable board emulating about 25 mappers (MMC1 / MMC3 / VRC IRQs, flash, 512 kB CHR RAM): a large board of its own |
| 515 (Family 노래방 karaoke) | its YM2413 clone's sound (9 channels, rhythm and its own instruments, beyond the VRC7 synth) and the microphone ADC; banking alone would give a silent karaoke cartridge |
| 208 (快打傳説 Street Fighter IV) | its protection runs code out of the $5800 registers, and opcode fetches there don't go through the board (the CPU's fetch path) |
| 258 (158B) | its protection read values are only in emulator sources; the wiki doesn't describe them |
| 355 (3D Block, Block Force) | a PIC16C54 microcontroller watching the address bus raises the IRQs |
| 406 (Haradius Zero), 451 (Haratyler) | the games check the flash ROM's software ID through ROM reads (451 also has an MP3 player) |
| 446 (Mindkids SMD172B) | an FPGA board emulating many mappers |
| 344 (GN-26) | the wiki's register bit layout is ambiguous and no emulator has the board; read one way, only some of its games start |
| 512 (中國大亨) | four pages of cartridge VRAM beside CIRAM (six nametable pages) and CHR RAM beside CHR ROM; no reference emulator |
| ROMs over 2 MB of PRG or CHR | they don't fit in the Dreamcast's RAM alongside the emulator |

## Peripherals not emulated

- Family Trainer mat
- Space Shadow gun
- Karaoke Studio's microphone
- Datach barcode reader
- Oeka Kids tablet
- The 93C66 EEPROMs on mappers 164 and 558, and the 93C56 on 529's YY0807
- The Subor keyboard (mapper 167's Russian cartridge)

## Planned for a later release

The v1.0 scope is every mapper with two or more dumps in the test set. These 134 mappers have one dump each there and
wait for a later release:

14, 38, 41, 100, 101, 104, 107, 127, 128, 145, 170, 174, 175, 186, 204, 214, 229, 237, 238, 243, 244, 252, 259, 262, 266, 269, 271, 272, 274, 277, 289, 291, 292, 297, 298, 299, 301, 308, 315, 320, 322, 326, 330, 331, 333, 334, 335, 338, 339, 340, 341, 347, 349, 353, 356, 357, 360, 364, 365, 369, 370, 372, 375, 376, 382, 383, 385, 389, 391, 393, 397, 398, 399, 401, 402, 404, 407, 408, 410, 412, 413, 414, 416, 417, 418, 420, 421, 427, 429, 430, 431, 432, 433, 436, 437, 440, 441, 442, 444, 447, 448, 450, 452, 453, 454, 455, 456, 457, 458, 500, 501, 502, 513, 514, 517, 523, 525, 526, 531, 533, 538, 539, 540, 541, 544, 548, 549, 550, 553, 554, 556, 557, 559, 560

Three of them were already looked at:
- **554** (KS-7010: 悪魔城 Dracula) switches banks on reads.
- **549** (KS-7016B: 迷宮寺院 Dabava) has no documentation.
- **266** (City Fighter IV) has its own 4-bit PCM DAC, which the synth would need.
