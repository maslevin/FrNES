/*===================================================================*/
/*                                                                   */
/*  image.h : Pictures for the menus (PNG textures)                  */
/*                                                                   */
/*===================================================================*/

#ifndef __IMAGE_H
#define __IMAGE_H

#include <kos.h>

/*
 * The menus' pictures (frontend/assets/art, drawn by tools/draw_art.py): RGBA PNGs with power-of-two sides, loaded
 * into VRAM with the fonts (images_load / images_free, around each game) and drawn in the translucent list
 */
typedef enum {
	IMAGE_CONTROLLER,
	IMAGE_ARCADE_STICK,
	IMAGE_LIGHT_GUN,
	IMAGE_MOUSE,
	IMAGE_ICONS,
	IMAGE_COUNT
} Image_Id;

// the icon strip's icons, 32x32 each
typedef enum {
	ICON_FOLDER,
	ICON_UP,
	ICON_NES,
	ICON_FDS,
	ICON_NSF,
	ICON_MOVIE
} Icon_Id;

void images_load();
void images_free();

/**
 * Draws a picture (or the part of it from u0, v0 to u1, v1, as fractions of its width and height) in the box from
 * (x, y), w by h, at depth z, tinted by color (0xFFFFFFFF: as it is)
 */
void image_draw(Image_Id id, float x, float y, float w, float h, float u0, float v0, float u1, float v1, float z, uint32 color);

// one of the icon strip's icons, size by size
void image_draw_icon(Icon_Id icon, float x, float y, float size, float z);

#endif
