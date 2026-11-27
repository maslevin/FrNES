# FrNES 1.0.0 frontend

The menus, options and ROM browser, built on the FrNES 0.7.1 frontend (`numechanix/frnes`, branch `dev`), over the
1.0 emulator core.

```
make frontend            # frontend_build/FrNES.elf
dc-tool-ip -t <ip> -x frontend_build/FrNES.elf
```

The romdisk holds the font and palette from `frontend/assets` and whatever ROMs are in `romdisk/`. A romdisk with
`autorom.txt` naming a ROM plays it at start, then shows the menus.

## How it fits together

| File | What it does |
| --- | --- |
| `frontend.c` | `main()`: the menu loop, drawing, AutoROM, and `launchEmulator()`, which hands the PVR to the emulator for a game |
| `menu.c`, `menu.h` | Menu pages as data: a page is a table of rows (text, action, toggle, choice, number, slider) with the value each changes |
| `pages.c` | The pages themselves: main menu, Video, Controls, System, Menu Colors, Help, About |
| `browser.c` | The ROM browser: devices, folders first, `.nes` / `.fds`, remembers its place, shows the highlighted ROM's header |
| `input.c` | Controller input for the menus: any controller, presses once, repeats for directions and page keys |
| `options.c` | `FrNES.opt` on the VMU: a versioned header, then the colors, the emulator's settings and the browser's place |
| `rom_info.c` | What a ROM file's header says (iNES / NES 2.0, FDS, NSF, movies), for the status window |
| `dialog.c` | A message over the screen, closed with A (a ROM that didn't start, a mode that's on the way) |
| `palettes.c` | The NES palettes (`assets/palettes/*.pal`), passed to the emulator through `frnes_palette()` |
| `image.c` | The menus' pictures (`assets/art`, drawn by `tools/draw_art.py`): controllers and the file type icons |
| `color_picker.c` | The menu colours' picker: a hue / saturation field and a brightness bar |
| `markdown.c` | Help and About from `assets/help.md` and `about.md`: headings, paragraphs, bullets, pictures, links |
| `cheats.c` | A game's Game Genie codes (X in the browser): three slots, typed or from a `.cheats` list |
| `text_window.c`, `font.c` | 0.7.1's rounded windows and bitmap font (rows are typed `window_row_t`s now, not tagged strings) |

The emulator side is `frnes.h` at the top of the repo:

- `frnes_run(path)` plays a game until A+B+X+Y+Start and returns. It owns the PVR and the store queues while it runs.
- `frnes_settings` is what it plays with: crop by the pixel at each edge, picture mode (full screen, exact 2x,
  8:7), bilinear filter, frame display, both pads' button mappings, pacing (vsync or sound), and a reserved field
  for frameskip.

## Adding an option

1. Add the field to `frnes_settings_t` (the emulator reads it) or to `Options_t` (only the menus use it).
2. Bump `OPTIONS_VERSION` in `options.h`: older files are then ignored rather than read with the wrong layout.
3. Set its default in `frnes_default_settings()` or `options_set_defaults()`.
4. Add a row to the page's table in `pages.c`.

## Not done yet

- Frameskip has a reserved field and no row. (Vsync is System > Timing, on by default as in the player builds.)
- Recording and playback from the menus (the core's `.frm` recordings), an in-game menu, palette choice, an NSF
  player, box art and manuals.
- `disksys.rom` is looked for in fixed places (`fds.c`), not beside the game.
