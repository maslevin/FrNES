/*===================================================================*/
/*                                                                   */
/*  viewer.h : A game's boxes and manual, over the screen            */
/*                                                                   */
/*===================================================================*/

#ifndef __VIEWER_H
#define __VIEWER_H

#include <kos.h>
#include "art.h"

/**
 * Opens the viewer for a game's art (art_find): its box (the US one, X for the Famicom one), and its manual (A)
 * paged with left / right, zoomed with the triggers and moved with the analog stick or the D-pad when zoomed
 */
void viewer_open(const Art_Game* game);

int viewer_active();

// takes the input while it's open (each frame: the stick pans smoothly); 1 if it did
int viewer_input(uint32 input);

void viewer_draw(uint32 list);

#endif
