/*===================================================================*/
/*                                                                   */
/*  image.c : Pictures for the menus (PNG textures)                  */
/*                                                                   */
/*===================================================================*/

#include "image.h"
#include <png/png.h>

static const char* const paths[IMAGE_COUNT] = {
	"/rd/art/controller.png",
	"/rd/art/arcade_stick.png",
	"/rd/art/light_gun.png",
	"/rd/art/mouse.png",
	"/rd/art/icons.png"
};

static struct {
	pvr_ptr_t texture;
	uint32 width, height;
} images[IMAGE_COUNT];

void images_load() {
	for (int i = 0; i < IMAGE_COUNT; i++) {
		images[i].texture = NULL;
		if (png_load_texture(paths[i], &images[i].texture, PNG_FULL_ALPHA, &images[i].width, &images[i].height) != 0) {
			printf("images: unable to load [%s]\n", paths[i]);
			images[i].texture = NULL;
		}
	}
}

void images_free() {
	for (int i = 0; i < IMAGE_COUNT; i++) {
		if (images[i].texture != NULL) {
			pvr_mem_free(images[i].texture);
			images[i].texture = NULL;
		}
	}
}

void image_draw(Image_Id id, float x, float y, float w, float h, float u0, float v0, float u1, float v1, float z, uint32 color) {
	if ((id >= IMAGE_COUNT) || (images[id].texture == NULL)) {
		return;
	}
	pvr_poly_cxt_t cxt;
	pvr_poly_hdr_t hdr;
	pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_TWIDDLED, images[id].width, images[id].height,
		images[id].texture, PVR_FILTER_BILINEAR);
	pvr_poly_compile(&hdr, &cxt);
	pvr_prim(&hdr, sizeof(hdr));
	pvr_vertex_t v;
	v.z = z;
	v.argb = color;
	v.oargb = 0;
	v.flags = PVR_CMD_VERTEX;
	v.x = x; v.y = y + h; v.u = u0; v.v = v1; pvr_prim(&v, sizeof(v));
	v.x = x; v.y = y; v.u = u0; v.v = v0; pvr_prim(&v, sizeof(v));
	v.x = x + w; v.y = y + h; v.u = u1; v.v = v1; pvr_prim(&v, sizeof(v));
	v.flags = PVR_CMD_VERTEX_EOL;
	v.x = x + w; v.y = y; v.u = u1; v.v = v0; pvr_prim(&v, sizeof(v));
}

void image_draw_icon(Icon_Id icon, float x, float y, float size, float z) {
	// (32x32 icons across a 256x32 strip)
	float u0 = (float)icon / 8.0f;
	image_draw(IMAGE_ICONS, x, y, size, size, u0, 0.0f, u0 + 1.0f / 8.0f, 1.0f, z, 0xFFFFFFFF);
}
