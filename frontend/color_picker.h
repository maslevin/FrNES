/*===================================================================*/
/*                                                                   */
/*  color_picker.h : A colour picked from a hue / saturation field   */
/*                                                                   */
/*===================================================================*/

#ifndef __COLOR_PICKER_H
#define __COLOR_PICKER_H

#include <kos.h>

/**
 * Opens the picker over the screen for *color (ARGB): the D-pad or analog stick moves through a field of hues
 * (across) and saturations (down), the triggers set the brightness, A keeps the colour and B puts the old one back.
 * The colour changes as it moves, and changed() is called each time (to restyle the windows)
 */
void color_picker_open(const char* name, uint32* color, void (*changed)(void));

/**
 * Takes the input while it's open (every frame: the analog stick moves smoothly); 1 if it did
 */
int color_picker_input(uint32 input);

void color_picker_draw(uint32 list);

#endif
