# In the menus

- The D-pad moves, A chooses, B goes back.
- Left and right change an option; the status window below says what it does.
- The triggers page through long lists (on the Controls page they change tabs).

# In a game

These are the default hotkeys once you've loaded a game ROM
- **A+B+X+Y+Start** ends the game and goes back to the browser. Any saves in progress will be written first.
- **LT+RT** will summon the in-game menu, where you can change certain settings while the game has been loaded.
- **RT** will flip the disk in an FDS game.
- **HOLD RT** will enable the on-screen frame display

# The Zapper

- Light gun games (Duck Hunt, Hogan's Alley...) get the NES Zapper on port 2 by themselves.
- On Auto it's aimed with a **Dreamcast light gun** if one's plugged in, else the **DC mouse**, else controller 1.
- **Light gun**: aim at the screen and pull the trigger; **B** shoots off the screen. Light guns work on CRT TVs only, not on VGA, and only see bright parts of the picture: over dark areas the aim stays where it last was, so FrNES fills the edges round the game's picture with grey.
- **Calibrating the light gun**: every TV and gun reads a little differently, so line yours up once. On the light gun's tab of the Controls page, choose **Calibrate**: a white screen shows a target in each corner in turn. Stand where you'll play, aim at the centre of each target and pull the trigger. FrNES works out how far across and down your shots landed and saves it with your options, for every game. If the gun stops answering (too far from the TV), the screen waits up to 5 seconds for it; **B** gives up.
- The same tab shows the calibration's four numbers (across and down, each a scale and an offset) to adjust by hand, **Reset calibration** to go back to FrNES's own, and **Reticle** to hide the crosshair while you aim with the gun alone.
- **Mouse**: move it to aim, any button fires.
- **Controller**: the analog stick or D-pad moves the reticle, **A** fires, **B** fires off the screen. The Hotkeys tab sets the reticle's speed.
- Games with both a pad and the Zapper (The Adventures of Bayou Billy) start with the Zapper off: turn **ZAPPER 2** on in the game menu.
- **Is a: Zapper** on a port makes that device the Zapper in any game. A port set to **Plugs into: Nothing** is never used, a light gun or mouse there included.
- Vs. System Zapper games: **LT+Start** puts in a coin, then shoot to start.

# The Arkanoid paddle

- Arkanoid, Arkanoid II and Taito Chase H.Q. are played with Taito's Vaus paddle, a knob and a button: FrNES gives them one by itself.
- The **DC mouse** is the paddle: move it left and right to turn the knob, any button fires.
- Without a mouse, controller 1 is the paddle: the analog stick or D-pad turns the knob, **A** fires.
- In the Famicom games (Arkanoid II, Chase H.Q. and the Japanese Arkanoid) the paddle is on the expansion port, so controllers 1 and 2 still work as pads: **Select** and **Start** in the menus. The US Arkanoid's paddle takes NES port 2.
- **Is a: Paddle** on a port picks which device is the paddle.

# Saves

- Battery saves and Save States can be saved to a VMU (or an SD Card if you have an SD adapter).
- FrNES will commit any changes to a save, a few seconds after the game writes them and you will see an onscreen indicator while this is happening. 
- The **Saves** menu will let you choose where saves should go: a VMU (and which), or the SD card.  You can also disable saving here if you would like.

# Files

- **.nes** games are played in the emulator.
- **.fds** games need the FDS BIOS, disksys.rom, which FrNES does not include: put your own copy in the FrNES folder on the SD card (or on the disc).
- **.nsf** music plays with its own player: left and right choose the current track.
- **Movies** (FrNES .frm, BizHawk .bk2) play over their game. FrNES finds the game in the movie's folder, or asks you to choose it once.
- With an SD card in, **START** in the browser turns recording on: the next game you play is recorded as a movie in /sd/FrNES/movies, then recording is off again (START again before that turns it off). A recording starts from power on, without the game's battery save.

The browser shows what each file's header says while it's highlighted.

# Cheats

- **X** in the browser opens a game's Game Genie codes: three slots, each on or off.
- Fill a slot by typing a code on the Game Genie's letters, or by choosing one from the game's list.
- FrNES has the official code books' codes for over 400 games.
- Your own list is a text file beside the game, named like it with **.cheats** in place of **.nes**, or in **/sd/FrNES/cheats**: a code and what it does on each line.
- The codes that are on are put in when the game starts. The game menu switches each one on or off while you play.
