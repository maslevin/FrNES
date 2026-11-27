#ifndef __GAME_MENU_H
#define __GAME_MENU_H

/*
 * The game menu (game_menu.c): opened by its hotkey at the end of a frame, the game paused and its sound muted while
 * it's open. Resume, save / load state and the slot, the Game Genie codes on or off, reset, the FDS disk side, and
 * back to the menus. Returns 1 when it chose to end the game.
 */
int game_menu_run(void);

#endif
