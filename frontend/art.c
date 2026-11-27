/*===================================================================*/
/*                                                                   */
/*  art.c : A game's box art and manual (tools/frnes_assets.py)      */
/*                                                                   */
/*===================================================================*/

#include "art.h"
#include "game_name.h"

#include <stdio.h>
#include <string.h>

static const char* const places[] = {
	"/sd/FrNES/art",
	"/cd/FrNES/art",
#ifdef FRNES_ART_PATH
	FRNES_ART_PATH,
#endif
};
#define NUM_PLACES (sizeof(places) / sizeof(places[0]))

// a folder's info.txt into *game; 0, or -1 if there's none
static int read_info(const char* folder, Art_Game* game) {
	char path[160];
	snprintf(path, sizeof(path), "%s/info.txt", folder);
	file_t handle = fs_open(path, O_RDONLY);
	if (handle == -1) {
		return -1;
	}
	char text[4096];
	int got = fs_read(handle, text, sizeof(text) - 1);
	fs_close(handle);
	if (got <= 0) {
		return -1;
	}
	text[got] = '\0';
	memset(game, 0, sizeof(*game));
	snprintf(game -> folder, sizeof(game -> folder), "%s", folder);
	char* save = NULL;
	for (char* line = strtok_r(text, "\n", &save); line != NULL; line = strtok_r(NULL, "\n", &save)) {
		int w, h, n;
		if (strncmp(line, "name ", 5) == 0) {
			snprintf(game -> name, sizeof(game -> name), "%s", line + 5);
		} else if (sscanf(line, "box_j %d %d", &w, &h) == 2) {
			game -> has_box_j = 1;
			game -> box_j_size[0] = w;
			game -> box_j_size[1] = h;
		} else if (sscanf(line, "box %d %d", &w, &h) == 2) {
			game -> has_box = 1;
			game -> box_size[0] = w;
			game -> box_size[1] = h;
		} else if (sscanf(line, "pages_j %d", &n) == 1) {
			game -> pages_j = n;
		} else if ((sscanf(line, "page_j %d %d %d", &n, &w, &h) == 3) && (n >= 0) && (n < ART_MAX_PAGES)) {
			game -> page_j_size[n][0] = w;
			game -> page_j_size[n][1] = h;
		} else if (sscanf(line, "pages %d", &n) == 1) {
			game -> pages = n;
		} else if ((sscanf(line, "page %d %d %d", &n, &w, &h) == 3) && (n >= 0) && (n < ART_MAX_PAGES)) {
			game -> page_size[n][0] = w;
			game -> page_size[n][1] = h;
		}
	}
	return 0;
}

/*
 * Lookups remembered: under dcload each one is several round trips to the Mac (a /pc read costs 25 ms or more), and the
 * browser asks again whenever the highlight comes back to a game. The games with no art (by their paths' hashes) and
 * the last one found
 */
#define ART_MISSES 64
static uint32 art_misses[ART_MISSES];
static uint32 art_miss_next = 0;
static uint32 art_hit_hash = 0;
static Art_Game art_hit;

static uint32 art_hash(const char* text) {
	uint32 hash = 2166136261u;
	while (*text) {
		hash = (hash ^ (uint8)*text++) * 16777619u;
	}
	return hash ? hash : 1;
}

static int art_lookup(const char* rom_path, Art_Game* game);

int art_find(const char* rom_path, Art_Game* game) {
	uint32 hash = art_hash(rom_path);
	if (hash == art_hit_hash) {
		*game = art_hit;
		return 0;
	}
	for (uint32 i = 0; i < ART_MISSES; i++) {
		if (art_misses[i] == hash) {
			return -1;
		}
	}
	if (art_lookup(rom_path, game) == 0) {
		art_hit_hash = hash;
		art_hit = *game;
		return 0;
	}
	art_misses[art_miss_next] = hash;
	art_miss_next = (art_miss_next + 1) % ART_MISSES;
	return -1;
}

static int art_lookup(const char* rom_path, Art_Game* game) {
	// (the ROM's name, without its folder or extension, as a key: whole, then up to a subtitle)
	const char* slash = strrchr(rom_path, '/');
	char name[128];
	snprintf(name, sizeof(name), "%s", slash ? slash + 1 : rom_path);
	char* dot = strrchr(name, '.');
	if (dot != NULL) {
		*dot = '\0';
	}
	char keys[2][96];
	name_key(name, keys[0], sizeof(keys[0]), 0);
	name_key(name, keys[1], sizeof(keys[1]), 1);
	for (int k = 0; k < 2; k++) {
		if (!keys[k][0] || ((k == 1) && (strcmp(keys[0], keys[1]) == 0))) {
			continue;
		}
		for (uint32 p = 0; p < NUM_PLACES; p++) {
			char folder[128];
			snprintf(folder, sizeof(folder), "%s/%s", places[p], keys[k]);
			if (read_info(folder, game) == 0) {
				return 0;
			}
		}
	}
	return -1;
}

/*
 * A .dt texture: a 32-byte header ("DcTx", the file's size, version, header size in 32-byte units less one, the
 * codebook's and palette's sizes, the picture's width and height, then the PVR's size and mode bits), then the
 * texture as the PVR takes it (pvrtex's file_dctex.h)
 */
typedef struct {
	char fourcc[4];
	uint32 chunk_size;
	uint8 version, header_size, codebook_size, colors_used;
	uint16 width_pixels, height_pixels;
	uint32 pvr_type;
} Dt_Header;

int art_load(const Art_Game* game, const char* which, Art_Texture* texture) {
	memset(texture, 0, sizeof(*texture));
	char path[180];
	snprintf(path, sizeof(path), "%s/%s.dt", game -> folder, which);
	file_t handle = fs_open(path, O_RDONLY);
	if (handle == -1) {
		return -1;
	}
	size_t size = fs_total(handle);
	uint8* data = memalign(32, size);
	if ((data == NULL) || (fs_read(handle, data, size) != (ssize_t)size)) {
		fs_close(handle);
		free(data);
		return -1;
	}
	fs_close(handle);
	const Dt_Header* header = (const Dt_Header*)data;
	size_t start = ((size_t)header -> header_size + 1) * 32;
	if ((memcmp(header -> fourcc, "DcTx", 4) != 0) || (start >= size)) {
		free(data);
		return -1;
	}
	uint32 type = header -> pvr_type;
	texture -> width = 8 << ((type >> 3) & 7);
	texture -> height = 8 << (type & 7);
	// (the mode bits are the PVR's: VQ, the pixel format, not twiddled)
	texture -> format = type & 0x7E000000;
	texture -> texture = pvr_mem_malloc(size - start);
	if (texture -> texture == NULL) {
		free(data);
		return -1;
	}
	pvr_txr_load(data + start, texture -> texture, size - start);
	free(data);
	// (the picture's size: info.txt has it for the boxes and pages)
	texture -> picture_width = texture -> width;
	texture -> picture_height = texture -> height;
	if (strcmp(which, "box") == 0) {
		texture -> picture_width = game -> box_size[0];
		texture -> picture_height = game -> box_size[1];
	} else if (strcmp(which, "box_j") == 0) {
		texture -> picture_width = game -> box_j_size[0];
		texture -> picture_height = game -> box_j_size[1];
	} else if ((strncmp(which, "pagej", 5) == 0) && (atoi(which + 5) < ART_MAX_PAGES) && game -> page_j_size[atoi(which + 5)][0]) {
		texture -> picture_width = game -> page_j_size[atoi(which + 5)][0];
		texture -> picture_height = game -> page_j_size[atoi(which + 5)][1];
	} else if ((strncmp(which, "page", 4) == 0) && (atoi(which + 4) < ART_MAX_PAGES) && game -> page_size[atoi(which + 4)][0]) {
		texture -> picture_width = game -> page_size[atoi(which + 4)][0];
		texture -> picture_height = game -> page_size[atoi(which + 4)][1];
	}
	return 0;
}

void art_free(Art_Texture* texture) {
	if (texture -> texture != NULL) {
		pvr_mem_free(texture -> texture);
	}
	memset(texture, 0, sizeof(*texture));
}

void art_draw(const Art_Texture* texture, float x, float y, float w, float h, float z, float u0, float v0, float u1, float v1) {
	if (texture -> texture == NULL) {
		return;
	}
	// (the part of the picture shown, fitted to the box keeping its shape)
	float part_w = texture -> picture_width * (u1 - u0), part_h = texture -> picture_height * (v1 - v0);
	float scale = fminf(w / part_w, h / part_h);
	float dw = part_w * scale, dh = part_h * scale;
	x += (w - dw) / 2;
	y += (h - dh) / 2;
	float tu = (float)texture -> picture_width / texture -> width, tv = (float)texture -> picture_height / texture -> height;
	pvr_poly_cxt_t cxt;
	pvr_poly_hdr_t hdr;
	pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, texture -> format, texture -> width, texture -> height, texture -> texture,
		PVR_FILTER_BILINEAR);
	pvr_poly_compile(&hdr, &cxt);
	pvr_prim(&hdr, sizeof(hdr));
	pvr_vertex_t v;
	v.z = z;
	v.argb = 0xFFFFFFFF;
	v.oargb = 0;
	v.flags = PVR_CMD_VERTEX;
	v.x = x; v.y = y + dh; v.u = u0 * tu; v.v = v1 * tv; pvr_prim(&v, sizeof(v));
	v.x = x; v.y = y; v.u = u0 * tu; v.v = v0 * tv; pvr_prim(&v, sizeof(v));
	v.x = x + dw; v.y = y + dh; v.u = u1 * tu; v.v = v1 * tv; pvr_prim(&v, sizeof(v));
	v.flags = PVR_CMD_VERTEX_EOL;
	v.x = x + dw; v.y = y; v.u = u1 * tu; v.v = v0 * tv; pvr_prim(&v, sizeof(v));
}
