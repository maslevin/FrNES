![FrNES](/rd/art/frnes_logo.png)
by [Matt Slevinsky](https://github.com/maslevin)

FrNES is an NES emulator for the Sega Dreamcast. After 25 years in development, we have finally reached version 1.0.0!  This release makes FrNES the first and only cycle-accurate NES emulator to be released for the Sega Dreamcast.  It has been fully rewritten for this release and now contains no code from any other emulation project.

## New in this release

![Accuracycoin_143](/rd/art/accuracycoin_143.png)

- Cycle accurate CPU and PPU cores (passes AccuracyCoin with 143/147 tests - More than Mesen!)
- Sound sampled at 44.1kHz
- 60FPS, no frame skipping required at all
- Support for all Commercial Mappers, Expansion Audio, and Most Pirate / Copier Mappers (more to come)
- FDS Support
- VS. System Support
- Save States Support
- In-Game Menu
- Game Box and Manual Viewer
- NSF player support (with SH4ZAM powered, WinAmp inspired frequency analyzer)
- Movie Support (Record FrNES Movies to an SD Card if you have an adapter.  Plays FrNES movies and BizHawk movies if you want to replay speedruns on your DC)
- Cheat (GameGenie) Support
- Zapper and DC Lightgun Support
- DC Mouse Support (as a Zapper, or as an Arkanoid Paddle)
- Arkanoid Paddle Support (NES and Famicom Vaus controllers: Arkanoid, Arkanoid II, Chase H.Q.)

## Thanks to
- [Racoon](https://archive.org/download/pNesX_aloha): for pNesX, the original NES emulator for Playstation which kicked this whole project off so many years ago
- Megan Potter for starting [KallistiOS](https://github.com/KallistiOS/KallistiOS) and major thanks to all its contributors and maintainers that have kept it running over the years
- [Rand Linden](https://github.com/RandalLinden) for sharing his Dreamcast technical knowledge with me many years ago, and for Bleem!
- [Falco Girgis](https://github.com/gyrovorbis) for SH4ZAM and his major support, technical help and promotion of the Dreamcast homebrew and hobbyist scene 
- [Paul Cercueil](https://github.com/pcercuei) for sharing emulation ideas, VQ multi-codebook and for Bloom
- [jnmartin84](https://github.com/jnmartin84) for sharing information on MMU usage
- M. Bibaud and the MesenCE team for [Mesen](https://www.mesen.ca), which was extremely helpful and influential in the development of FrNES 1.0.0
- BizHawk (NesHawk), used with Mesen 2 as an accuracy reference; NesHawk's power-on and reset timing is followed so BizHawk movies sync
- The [NESdev wiki](https://www.nesdev.org/wiki/) and its community, for the hardware's documentation and the 2C02G, 2C07 and 2C03 palettes
- NewRisingSun, for the [NES 2.0 XML Database](https://forums.nesdev.org/viewtopic.php?t=19940), FrNES's game database (mappers, Vs. System settings and controllers)
- The Game Genie codes: a compilation of Galoob's official NES code books, from [NES World](http://www.nesworld.com/gamegenie.php), and [GameGenie.com](https://gamegenie.com/cheats/gamegenie/nes/index.html)
- uthash / utarray by Troy D. Hanson (BSD license), in the menus
- [JetBrains Mono](https://www.jetbrains.com/lp/mono/), the list font, under the SIL Open Font License 1.1 (OFL_JetBrainsMono.txt)
- Neuropol, the title font (neuro.fnt), under CC0 1.0 (public domain dedication)

## Third-party licences

FrNES is built with these, under their own licences (their full texts are in docs/third-party-licenses.md):
- KallistiOS: Copyright (C) 1997-2024 KallistiOS Contributors. All rights reserved. The KOS License (BSD). libkosfat: Copyright (C) 2012, 2013, 2014, 2016, 2019 Lawrence Sebald
- SH4ZAM: Copyright (c) 2025, 2026 Falco Girgis. The MIT License
- libpng: Copyright (c) 1995-2019 The PNG Reference Library Authors; Cosmin Truta, Glenn Randers-Pehrson, Andreas Dilger, Guy Eric Schalnat. The PNG Reference Library License version 2
- zlib: Copyright (C) 1995-2026 Jean-loup Gailly and Mark Adler. The zlib License
- bzip2: Copyright (C) 1996-2019 Julian Seward. The bzip2 License (BSD-style)
- newlib: Copyright Red Hat, Inc. and others, BSD-style licences
- uthash / utarray: Copyright (c) 2008-2022, Troy D. Hanson. BSD licence
- Neuropol (the title font): CC0. JetBrains Mono (the list font): the SIL Open Font License 1.1

---

Do not distribute FrNES with ROM images!
