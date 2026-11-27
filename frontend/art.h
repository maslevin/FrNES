/*===================================================================*/
/*                                                                   */
/*  art.h : A game's box art and manual (tools/frnes_assets.py)      */
/*                                                                   */
/*===================================================================*/

#ifndef __ART_H
#define __ART_H

#include <kos.h>

/*
 * The library tools/frnes_assets.py compiles: a folder per game, by its name's key (game_name.h), holding info.txt,
 * box.dt (US), box_j.dt (Famicom), page000.dt.. (the manual) and pagej000.dt.. (the Famicom manual), KOS pvrtex .dt
 * textures. Looked for on the SD card
 * (/sd/FrNES/art), then the CD (/cd/FrNES/art), then FRNES_ART_PATH (a build's own, e.g. /pc/FrNES_dev/art).
 */

typedef struct {
	pvr_ptr_t texture;
	uint32 format;              // PVR_TXRFMT_* for pvr_poly_cxt_txr
	uint16 width, height;       // the texture's
	uint16 picture_width, picture_height;   // the picture's, from its top left corner (the rest is black)
} Art_Texture;

#define ART_MAX_PAGES 160

typedef struct {
	char folder[128];
	char name[64];
	int has_box, has_box_j;
	uint16 box_size[2], box_j_size[2];
	// the manual's pages (page000.dt..) and the Famicom manual's (pagej000.dt..), each page's picture size (pages after
	// ART_MAX_PAGES are taken as filling their texture)
	int pages, pages_j;
	uint16 page_size[ART_MAX_PAGES][2];
	uint16 page_j_size[ART_MAX_PAGES][2];
} Art_Game;

/**
 * The art for a ROM (its file name, with or without the path), into *game; 0, or -1 if the library has none
 */
int art_find(const char* rom_path, Art_Game* game);

/**
 * Loads a texture of the game's ("box", "box_j" or a page's number as "page003", the Famicom manual's as
 * "pagej003") into VRAM; 0 or -1
 */
int art_load(const Art_Game* game, const char* which, Art_Texture* texture);
void art_free(Art_Texture* texture);

/**
 * Draws a texture's picture in the box (x, y) w by h, keeping its shape (centred), in the translucent list; u0, v0,
 * u1, v1 pick part of the picture (0-1 of it), for a zoomed view
 */
void art_draw(const Art_Texture* texture, float x, float y, float w, float h, float z, float u0, float v0, float u1, float v1);

#endif
