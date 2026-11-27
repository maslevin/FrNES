# How to run FrNES 1.0.0 on a Dreamcast

This guide covers the three ways to get FrNES 1.0.0 running on a real Dreamcast:

1. [Boot the release disc image](#1-boot-the-release-disc-image) from a burned CD-R or an optical drive emulator
   (ODE), and play ROMs from an SD card adapter.
2. [Make your own bootable disc image](#2-make-your-own-bootable-disc-image) with your own ROMs (and box art and
   manuals) on it.
3. [Load the release `.elf` through dcload](#3-load-the-release-elf-through-dcload), for development or for playing
   ROMs straight from a computer on your network.

FrNES 1.0.0 doesn't include any games. You need your own ROMs, and for Famicom Disk System games your own FDS BIOS
(`disksys.rom`). Never share a disc image that has ROMs or the BIOS on it.

## What FrNES reads

Whichever way it's started, FrNES works the same once it's running:

- **The ROM browser** lists every device it finds: **CD-ROM** (`/cd`, the disc or the ODE's image), **SD card**
  (`/sd`), **PC (dcload)** (`/pc`, only when started through dcload) and **Romdisk** (`/rd`). ROMs can be in any
  folder on any of them.
- **Files it plays**: `.nes` (iNES and NES 2.0), `.fds` (Famicom Disk System), `.nsf` (music) and movies (`.frm`,
  `.bk2`). Zipped ROMs aren't read: unzip them first.
- **The FDS BIOS**, `disksys.rom`, is looked for at `/sd/FrNES/disksys.rom` first, then `/cd/FrNES/disksys.rom`, then
  `/cd/disksys.rom`.
- **Saves** (battery saves and save states) go to a VMU by default. With an SD card in, the **Saves** menu can put them
  on the card instead (`/sd/FrNES/...`). Movies are recorded to `/sd/FrNES/movies`, so recording needs an SD card.
- **Box art and manuals** are read from `/sd/FrNES/art` first, then `/cd/FrNES/art`
  ([box-art-and-manuals.md](box-art-and-manuals.md)).
- **Your own Game Genie lists** go beside the game (`Game.cheats` next to `Game.nes`) or in `/sd/FrNES/cheats`.

The menus' **Help** page has the controls. The ones to know: **A+B+X+Y+Start** ends a game and goes back to the
browser, **LT+RT** opens the in-game menu, and **RT** flips the disk in an FDS game.

## 1. Boot the release disc image

The release's `.cdi` is a self-booting disc image with FrNES's menus on it and nothing else. You add the games on an
SD card, or on a separate CD put in once FrNES has booted.

### On a burned CD-R

1. Burn the `.cdi` to a CD-R with a tool that writes DiscJuggler (`.cdi`) images, such as DiscJuggler or ImgBurn on
   Windows. Use a good quality CD-R and a slow write speed (4x-8x): the Dreamcast's drive reads slow burns most
   reliably.
2. Put the disc in the Dreamcast and turn it on. It boots straight into FrNES.

Burned CD-Rs only boot on Dreamcasts that support MIL-CD discs. Most consoles do; some of the last ones made don't
(they show the disc as an audio CD, or go to the Dreamcast's own menu). Use an ODE or dcload on those.

### On an ODE

An optical drive emulator (GDEMU, MODE, USB-GDROM and the like) replaces the Dreamcast's disc drive and loads disc
images from its own SD card or USB drive.

1. Copy the `.cdi` onto the ODE's card the way your ODE expects (for example in its own numbered folder on a GDEMU),
   as you would any other disc image.
2. Choose FrNES in the ODE's menu, or let it boot.

FrNES sees the ODE as a CD-ROM (`/cd`): it can't read the ODE's card itself. Games on the ODE's card have to be inside
a disc image (see [section 2](#2-make-your-own-bootable-disc-image)), or on an SD card in an SD card adapter.

### Playing ROMs from a separate CD

FrNES runs from memory once it has booted, so the disc can be swapped. Boot FrNES, open the drive (or change discs on
an ODE that can do that), and put in a CD with your ROMs on it: an ordinary data CD-R (ISO 9660, Joliet or Rock Ridge
names are all read) doesn't need to be bootable. Choose **CD-ROM** in the browser to browse it. The FDS BIOS and box
art are read from the disc that's in at the time (`/cd/FrNES/...`), so put them on the ROM CD too if you use them.

### Playing ROMs from an SD card adapter

An SD card adapter plugs into the Dreamcast's serial port, at the back. It works the same with a burned disc, an ODE
or dcload-ip (not with dcload-serial: they need the same port).

1. Format the card as **FAT32** (or FAT16) with an ordinary **MBR** partition table. FrNES reads the card's first
   partition only. Large cards that come formatted as exFAT have to be reformatted as FAT32 first.
2. Lay out the card like this. Only the `FrNES` folder's name matters; the ROMs can go in any folders you like.

   ```
   FrNES/
     disksys.rom         the FDS BIOS, if you have one
     art/                box art and manuals, if you make them (docs/box-art-and-manuals.md)
     cheats/             your own Game Genie lists, if you want them
   ROMs/
     NES/
       Legend of Zelda, The (USA).nes
       ...
     FDS/
       ...
   ```

   FrNES makes the folders it writes to (`FrNES/states`, `FrNES/movies` and the saves) by itself.
3. Put the card in the adapter, the adapter in the serial port, and boot FrNES. In the browser, choose **SD card**, then
   your game.

A card put in after FrNES has started is found the next time the browser opens (go back to the device list). If the
browser has no **SD card**, check the card's format, and that the card is pushed all the way into the adapter and the
adapter firmly into the port: a loose adapter is the most common reason a card isn't found.

## 2. Make your own bootable disc image

The disc FrNES reads is an ordinary ISO 9660 disc. Everything in the repository's `iso/` folder goes on it, beside
FrNES itself, so a disc image with your own games is: put your files in `iso/`, then `make cdi`.

### What you need

- A Dreamcast build environment: the SH4 toolchain and KallistiOS (KOS), with `KOS_BASE` set by KOS's `environ.sh`.
  See [building.md](building.md). The release was built with mainline KOS, the patches in `tools/patches/` and LTO;
  FrNES also builds on stock mainline KOS (with the libkosfat patches for the SD browser), about 0.8 % slower.
- KOS's tools, built: `scramble` (`$KOS_BASE/utils/scramble`) and `makeip` (`$KOS_BASE/utils/makeip`).
- `mkisofs` (from cdrtools; `brew install cdrtools` on macOS).
- `cdi4dc` (from img4dc), which writes the self-booting `.cdi`.

### Steps

1. Put what you want on the disc in `iso/` at the repository's root:

   ```
   iso/
     FrNES/
       disksys.rom       the FDS BIOS, if you have one
       art/              box art and manuals, if you made them
     ROMs/
       ...your games, in any folders
   ```

2. Build the disc image:

   ```bash
   make cdi
   ```

   This builds FrNES's menus (`make frontend`), puts them in `iso/` as the scrambled `1ST_READ.BIN`, makes an image of
   all of `iso/` with its bootstrap (`IP.BIN`), and writes **`cdi/SH4_6502.cdi`**. Burn it or copy it to your ODE as in
   [section 1](#1-boot-the-release-disc-image).

`make clean` leaves your files in `iso/` alone (only `iso/1ST_READ.BIN` goes). `make cdi-player` makes the same disc
with the stand-alone player (`release/`, which plays one built-in ROM with no menus) in place of the menus.

### Things to know

- **Size**: the disc holds about 650-700 MB, all of it yours less FrNES's few MB. A full NES library is far smaller than
  that; box art and manuals are what fill a disc ([Fitting a CD](box-art-and-manuals.md#fitting-a-cd) has the options
  that make them smaller).
- **Long names**: the image is made with Joliet and Rock Ridge (`mkisofs -J -joliet-long -r`), so ROMs keep their full
  names, spaces and all.
- **The boot screen**: by default the disc's bootstrap is made by `makeip` with the title `FRNES`. To change its fields
  (title, maker, version and so on), put an `ip.txt` in `makeip`'s format at the repository's root. To show your own
  logo on the Dreamcast's boot screen, put an `iplogo.png` there: 320 x 90 or smaller, under 128 colours (see `makeip`'s
  README). Both are used when they're there.
- **Saves**: a disc is read only. Saves go to a VMU, or to an SD card if there's one in an adapter.
- **Without building FrNES**: to use the released binary instead of building your own, scramble the release's `.bin`
  (the plain binary of the `.elf`) into `iso/` as `1ST_READ.BIN` and make the disc from `iso/` as it is, with
  `make cdi-image` (it builds nothing). The Makefile still reads KOS's `Makefile.rules`, so `KOS_BASE` has to be set,
  and `scramble` has to be built:

  ```bash
  $KOS_BASE/utils/scramble/scramble FrNES.bin iso/1ST_READ.BIN
  make cdi-image
  ```

  (with the release's `.bin` in place of `FrNES.bin`).

## 3. Load the release `.elf` through dcload

[dcload](https://github.com/KallistiOS/dcload-ip) loads programs onto a Dreamcast over the network (dcload-ip, with a
Broadband Adapter or LAN adapter) or a serial cable (dcload-serial), without burning anything. It also lets the program
read files from the computer as `/pc`, which FrNES's browser lists as **PC (dcload)**.

1. Boot the dcload disc on the Dreamcast (burned, or from an ODE). It shows the console's IP address.
2. On the computer, load the release's `.elf`:

   ```bash
   dc-tool-ip -t 192.168.1.100 -x FrNES.elf
   ```

   with your Dreamcast's IP address after `-t` and the release's `.elf` in place of `FrNES.elf`. Leave `dc-tool-ip` running while you play: it's FrNES's connection to
   the computer.
3. To play ROMs from the computer, give `dc-tool-ip` a folder to serve as `/pc`:

   ```bash
   dc-tool-ip -t 192.168.1.100 -m ~/nes -x FrNES.elf
   ```

   Then choose **PC (dcload)** in the browser: it lists that folder. `-m` is in current versions of `dc-tool-ip`
   (`dc-tool-ip -h` lists it). Without it, `/pc` is the computer's whole filesystem, so browse from its root, or use
   `-c <folder>` (which needs super-user rights).

With dcload-serial, use `dc-tool-ser` the same way. The serial cable takes the serial port, so an SD card adapter can't
be used at the same time; dcload-ip leaves the port free for one.

Reading from `/pc` is slower than from a card or a disc (each file read is a few round trips over the network), so
large folders take a moment to list and box art takes a moment to appear. Games themselves load once and then run at
full speed.

## Box art and manuals

FrNES can show each game's box (US and Famicom) and its instruction manual from the browser. They come from your own
scans: [box-art-and-manuals.md](box-art-and-manuals.md) covers `tools/frnes_assets.py`, which turns a folder of box
scans (PNG or JPG) and manual PDFs into Dreamcast textures:

```bash
python3 tools/frnes_assets.py ASSETS_DIR OUT_DIR
```

with the scans in `ASSETS_DIR/nes_boxes`, `famicom_boxes`, `nes_manuals` and `famicom_manuals`. It needs Python 3,
ImageMagick 7, Ghostscript and KOS's `pvrtex`. Then copy `OUT_DIR`'s contents to `FrNES/art` on the SD card, or to
`iso/FrNES/art` before `make cdi`. `--estimate` tells you how much room your library's art will take, before you make
any of it.
