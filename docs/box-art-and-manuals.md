# Box art and manuals

FrNES can show a game's box (the US one, and the Famicom one when there is one) and its instruction manual from the
ROM browser. The pictures aren't part of FrNES: you make them from your own scans with `tools/frnes_assets.py`, which
turns them into Dreamcast textures, and put the result on the SD card or on the disc.

## In the menus

- In the ROM browser, a game with art shows its box small at the right of the status window once the highlight
  rests on it. **Y** opens it full screen.
- On the box: **A** opens the manual (the US box's US manual, the Famicom box's Famicom one, when there is one), **X**
  switches between the US and Famicom boxes (when there are both), **B** goes back.
- In the manual: left and right turn the page, **Start** goes to the next page, **LT** / **RT** zoom out and in, the
  D-pad (or the analog stick) moves round a zoomed page, **X** switches between the US and Famicom manuals (when there
  are both), **B** goes back to the box.

## Making the textures

### What you need

- Python 3.
- [ImageMagick](https://imagemagick.org) 7 (the `magick` command).
- [Ghostscript](https://www.ghostscript.com) (the `gs` command), for the manuals' PDFs.
- KallistiOS's `pvrtex`, built: `make -C $KOS_BASE/utils/pvrtex`. The tool looks for it at
  `$KOS_BASE/utils/pvrtex/pvrtex` (`KOS_BASE` defaults to `/opt/toolchains/dc/kos`).

With Homebrew (macOS): `brew install imagemagick ghostscript`.

### The scans

The tool reads one folder (`ASSETS_DIR` below) with up to four folders in it, one for each kind of picture:

```
ASSETS_DIR/
  nes_boxes/          US boxes, a PNG or JPG per game
      Legend of Zelda, The (USA).png
  famicom_boxes/      Famicom boxes, a PNG or JPG per game
      Legend of Zelda, The (Japan).jpg
  nes_manuals/        manuals, a PDF per game
      Legend of Zelda, The.pdf
  famicom_manuals/    Famicom manuals, a PDF per game
      Ganbare Goemon.pdf
```

Any of them can be left out, and folders inside them are searched too, so a downloaded set can go in as it
came (a link to it works as well). Each file's name is its game's name. A manual may also be named
`<game> - Manual (...).pdf`. When a game has several manuals, a `(Clearscan)` scan is used first, then
`(Searchable)`, then `(Cleartext)`, then any other.

### How a game finds its pictures

Each game's textures go in a folder named by its **key**: the game's name in lower case, letters and digits run
together. The key leaves out anything in brackets, a leading "The" (or ", The"), "Disney's", "Part" and apostrophes.
It also turns II-VI into digits and "&" into "and". The browser makes the same key from a ROM's file name and looks for
that folder, so these all meet at `legendofzelda`:

- `Legend of Zelda, The (USA).png`
- `Legend of Zelda, The - Manual (Clearscan).pdf` (or `Legend of Zelda, The.pdf`)
- `Legend of Zelda, The (U) (PRG1).nes`

If the whole name doesn't match, the browser tries again with the name cut at its subtitle (at `:`, `--` or ` - `), so
`Double Dragon II - The Revenge (U).nes` still finds `doubledragon2`. A ROM whose name is spelled differently from the
scan's (a translation, a hack, another region's title) finds nothing. Rename the ROM, or the scan, to match.

`OUT_DIR/index.txt` lists every key the tool made, with the game's name, to check against.

### Running it

```bash
python3 tools/frnes_assets.py ASSETS_DIR OUT_DIR
```

This makes `OUT_DIR/<key>/` for each game:

| File | What it is | Default size |
| --- | --- | --- |
| `info.txt` | the game's name, which pictures there are, and each picture's size | |
| `box.dt` | the US box | 256 x 512, about 34 kB |
| `box_j.dt` | the Famicom box (landscape) | 512 x 256, about 34 kB |
| `page000.dt`, `page001.dt` ... | the manual's pages | 1024 x 1024, about 258 kB each |
| `pagej000.dt`, `pagej001.dt` ... | the Famicom manual's pages | the same |

The textures are VQ-compressed RGB565 `.dt` files, which the PVR draws directly.

Running it again only remakes games whose scans are newer than their folder, or that were made at other sizes.
Useful options:

- `--jobs N`: how many games at once (default: all the computer's cores but two).
- `--only zelda`: just the games whose key contains `zelda`. Good for trying it out.
- `--limit N`: at most N games.

### Fitting a CD

At the default sizes a large library is well over a CD: about 1,500 games come to about 1.3 GB, nearly all of it
manual pages. Check first, then pick smaller sizes:

```bash
python3 tools/frnes_assets.py ASSETS_DIR OUT_DIR --estimate --page 512
```

`--estimate` counts every manual's pages and prints what the library would take, without making anything. To trade
pictures for room:

| Option | Effect |
| --- | --- |
| `--page 512` / `--page 256` | manual pages at 512 (about 66 kB) or 256 (about 18 kB) instead of 1024 |
| `--box 256` / `--box 128` | boxes at about 10 kB or 4 kB instead of 34 kB |
| `--no-famicom` | no Famicom boxes or manuals |
| `--no-manuals` | boxes only |
| `--no-boxes` | manuals only |

A CD holds about 650-700 MB, and FrNES and your ROMs need some of that. Pages at 512 are still easy to read zoomed in.

## Where the pictures go

FrNES looks in these places, in this order, and uses the first folder it finds for a game:

1. **SD card** (with an SD card adapter): `/sd/FrNES/art/<key>/`. Copy `OUT_DIR`'s contents into `FrNES/art` at the
   card's root.
2. **Disc**: `/cd/FrNES/art/<key>/`, i.e. a folder `FrNES/art` at the root of the disc.
3. **Development builds** only: the folder the build names with `FRNES_ART_PATH` (for example `/pc/art`, served from
   the computer running dc-tool-ip).

## Putting them on a CD

The disc FrNES reads is an ordinary ISO 9660 filesystem with the boot files. Lay its root out like this:

```
1ST_READ.BIN            FrNES (the frontend, scrambled)
FrNES/
  art/                  OUT_DIR's contents: index.txt and a folder per game
    legendofzelda/
      info.txt
      box.dt
      page000.dt ...
      pagej000.dt ...
  disksys.rom           the FDS BIOS, if you have it (FrNES doesn't include it)
ROMs/                   your games, in any folders you like: the browser lists the whole disc
```

Then build the disc with the Makefile's `cdi` target. It builds the frontend, puts it in `iso/` as the scrambled
`1ST_READ.BIN`, makes the image of all of `iso/` and its bootstrap, and writes `cdi/SH4_6502.cdi`, a self-booting disc
image to burn. So put `FrNES/` (and your ROMs) in `iso/` first. With the art in `art_out/`:

```bash
mkdir -p iso/FrNES
cp -R art_out iso/FrNES/art
make cdi
```

The image is made with Joliet and Rock Ridge (`mkisofs -J -joliet-long -r`), which KallistiOS reads. Game keys run up
to about 60 characters, and plain ISO 9660 names stop at 31, so without them games with long keys wouldn't find their
art. (`make cdi-player` makes the same disc with the stand-alone player, `release/`, in place of the frontend.)

Never put ROMs or the BIOS on a disc you share.
