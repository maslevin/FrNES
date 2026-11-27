/*===================================================================*/
/*                                                                   */
/*  game_name.h : A game's name as a key, for matching lists to ROMs */
/*                                                                   */
/*===================================================================*/

#ifndef __GAME_NAME_H
#define __GAME_NAME_H

#include <stddef.h>

/**
 * A game's name as a key (lower case letters and digits run together; see game_name.c), whole or up to a subtitle
 * (title); the cheats list and the art library (tools/frnes_assets.py, the same steps in Python) are found by it
 */
void name_key(const char* name, char* key, size_t size, int title);

#endif
