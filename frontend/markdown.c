/*===================================================================*/
/*                                                                   */
/*  markdown.c : Markdown pages (help, about) in the main window     */
/*                                                                   */
/*===================================================================*/

#include "markdown.h"
#include "frontend.h"
#include "options.h"
#include "text_window.h"

#include <png/png.h>

// in the laid out text, a link's text runs from LINK_START to LINK_END
#define LINK_START '\x01'
#define LINK_END '\x02'
// (a link carried on from the line before: drawn as a link, not listed again)
#define LINK_CARRIED '\x03'

#define HEADING1_SCALE 0.8f
#define HEADING2_SCALE 0.62f
#define TEXT_SCALE 0.5f
#define BULLET_INDENT 18.0f

typedef enum {
	LINE_HEADING1,
	LINE_HEADING2,
	LINE_TEXT,
	LINE_BULLET,        // a bullet's first line (its others are LINE_TEXT, indented)
	LINE_INDENTED,
	LINE_IMAGE,
	LINE_RULE,
	LINE_GAP
} Line_Kind;

typedef struct {
	Line_Kind kind;
	char* text;
	int image;
	float top, height;
	// the links whose text starts on this line: links[first_link ...]
	int first_link, link_count;
} Md_Line;

typedef struct {
	char* text;
	char* url;
} Md_Link;

#define MAX_IMAGES 8

static Md_Line* lines = NULL;
static int line_count = 0, line_capacity = 0;
static Md_Link* links = NULL;
static int link_count = 0, link_capacity = 0;
static struct {
	pvr_ptr_t texture;
	uint32 width, height;
	// the texture's sides: the picture's, each rounded up to a power of two (the PVR takes nothing else), the
	// picture in its top left corner, its last column and row repeated into the rest (for the bilinear filter)
	uint32 texture_width, texture_height;
	// as drawn: fitted inside the page's width and its visible height (markdown_load's), up or down, its shape kept,
	// centred across
	float draw_width, draw_height;
} images[MAX_IMAGES];
static int image_count = 0;
static float total_height = 0.0f;
static float layout_width = 0.0f, layout_height = 0.0f;

static Font* line_font(Line_Kind kind) {
	return ((kind == LINE_HEADING1) || (kind == LINE_HEADING2)) ? font : font_mono;
}

static float line_scale(Line_Kind kind) {
	return (kind == LINE_HEADING1) ? HEADING1_SCALE : ((kind == LINE_HEADING2) ? HEADING2_SCALE : TEXT_SCALE);
}

// a run of text's width, leaving out the link marks
static float text_width(Font* f, const char* text, size_t length, float scale) {
	char plain[256];
	size_t used = 0;
	for (size_t i = 0; (i < length) && (used < sizeof(plain) - 1); i++) {
		if ((text[i] != LINK_START) && (text[i] != LINK_END) && (text[i] != LINK_CARRIED)) {
			plain[used++] = text[i];
		}
	}
	plain[used] = '\0';
	float width = 0.0f;
	if (used > 0) {
		measureLine(f, plain, scale, &width);
	}
	return width;
}

static Md_Line* line_add(Line_Kind kind, const char* text, size_t length) {
	if (line_count == line_capacity) {
		int capacity = line_capacity ? line_capacity * 2 : 64;
		Md_Line* grown = realloc(lines, capacity * sizeof(Md_Line));
		if (grown == NULL) {
			return NULL;
		}
		lines = grown;
		line_capacity = capacity;
	}
	Md_Line* line = &lines[line_count++];
	memset(line, 0, sizeof(*line));
	line -> kind = kind;
	line -> image = -1;
	line -> first_link = link_count;
	if (text != NULL) {
		line -> text = malloc(length + 1);
		if (line -> text != NULL) {
			memcpy(line -> text, text, length);
			line -> text[length] = '\0';
		}
	}
	return line;
}

static void link_add(const char* text, size_t text_length, const char* url, size_t url_length) {
	if (link_count == link_capacity) {
		int capacity = link_capacity ? link_capacity * 2 : 16;
		Md_Link* grown = realloc(links, capacity * sizeof(Md_Link));
		if (grown == NULL) {
			return;
		}
		links = grown;
		link_capacity = capacity;
	}
	Md_Link* link = &links[link_count++];
	link -> text = strndup(text, text_length);
	link -> url = strndup(url, url_length);
}

/*
 * A paragraph's text (its inline marks done: links marked, bold and code marks dropped) wrapped to the width, a word
 * at a time; a link split across lines is marked again on each
 */
static void wrap(Line_Kind kind, const char* text, float width) {
	Font* f = line_font(kind);
	float scale = line_scale(kind);
	Line_Kind first = kind, rest = (kind == LINE_BULLET) ? LINE_INDENTED : kind;
	if ((kind == LINE_BULLET) || (kind == LINE_INDENTED)) {
		width -= BULLET_INDENT;
	}
	const char* start = text;
	int in_link = 0;
	while (*start != '\0') {
		// take words while they fit (at least one)
		const char* end = start;
		const char* fits = NULL;
		for (;;) {
			const char* word_end = end;
			while (*word_end == ' ') {
				word_end++;
			}
			while ((*word_end != ' ') && (*word_end != '\0')) {
				word_end++;
			}
			if ((fits != NULL) && (text_width(f, start, word_end - start, scale) > width)) {
				break;
			}
			fits = word_end;
			end = word_end;
			if (*end == '\0') {
				break;
			}
		}
		// (a link carried over from the line before starts this one)
		char buffer[256];
		size_t used = 0;
		if (in_link) {
			buffer[used++] = LINK_CARRIED;
		}
		for (const char* c = start; (c < fits) && (used < sizeof(buffer) - 2); c++) {
			buffer[used++] = *c;
			if (*c == LINK_START) {
				in_link = 1;
			} else if (*c == LINK_END) {
				in_link = 0;
			}
		}
		if (in_link) {
			buffer[used++] = LINK_END;
		}
		line_add(first, buffer, used);
		first = rest;
		start = fits;
		while (*start == ' ') {
			start++;
		}
	}
}

// a paragraph's inline marks: [text](url) marked as a link (and listed), ** and ` dropped
static char* inline_marks(const char* source, size_t length) {
	char* out = malloc(length + 1);
	if (out == NULL) {
		return NULL;
	}
	size_t used = 0;
	for (size_t i = 0; i < length; i++) {
		char c = source[i];
		if ((c == '*') && (i + 1 < length) && (source[i + 1] == '*')) {
			i++;
			continue;
		}
		if (c == '`') {
			continue;
		}
		if (c == '[') {
			const char* close = memchr(source + i, ']', length - i);
			if ((close != NULL) && (close + 1 < source + length) && (close[1] == '(')) {
				const char* url_end = memchr(close, ')', source + length - close);
				if (url_end != NULL) {
					const char* text = source + i + 1;
					size_t text_length = close - text;
					link_add(text, text_length, close + 2, url_end - (close + 2));
					out[used++] = LINK_START;
					memcpy(out + used, text, text_length);
					used += text_length;
					out[used++] = LINK_END;
					i = url_end - source;
					continue;
				}
			}
		}
		out[used++] = c;
	}
	out[used] = '\0';
	return out;
}

static uint32 texture_side(uint32 side) {
	uint32 p = 8;
	while (p < side) {
		p <<= 1;
	}
	return p;
}

static int image_add(const char* path) {
	if (image_count >= MAX_IMAGES) {
		return -1;
	}
	kos_img_t img;
	if (png_to_img(path, PNG_FULL_ALPHA, &img) != 0) {
		printf("markdown: unable to load [%s]\n", path);
		return -1;
	}
	uint32 w = img.w, h = img.h, tw = texture_side(w), th = texture_side(h);
	uint16* padded = (tw <= 1024) && (th <= 1024) && (w > 0) && (h > 0) ? malloc(tw * th * 2) : NULL;
	pvr_ptr_t texture = padded ? pvr_mem_malloc(tw * th * 2) : NULL;
	if (texture == NULL) {
		printf("markdown: no room for [%s] (%lu x %lu)\n", path, (unsigned long)w, (unsigned long)h);
		free(padded);
		free(img.data);
		return -1;
	}
	const uint16* src = (const uint16*)img.data;
	for (uint32 row = 0; row < th; row++) {
		const uint16* from = src + ((row < h) ? row : h - 1) * w;
		uint16* to = padded + row * tw;
		memcpy(to, from, w * 2);
		for (uint32 col = w; col < tw; col++) {
			to[col] = from[w - 1];
		}
	}
	pvr_txr_load_ex(padded, texture, tw, th, PVR_TXRLOAD_16BPP);
	free(padded);
	free(img.data);  // (png_to_img malloc()s it; kos_img_free is in libkosutils, not linked)
	images[image_count].texture = texture;
	images[image_count].width = w;
	images[image_count].height = h;
	images[image_count].texture_width = tw;
	images[image_count].texture_height = th;
	return image_count++;
}

// a paragraph (or bullet) collected over its lines, laid out
static void flush(Line_Kind kind, char* paragraph, size_t* length, float width) {
	if (*length == 0) {
		return;
	}
	int first_link = link_count;
	char* marked = inline_marks(paragraph, *length);
	int first_line = line_count;
	if (marked != NULL) {
		wrap(kind, marked, width);
		free(marked);
	}
	// (the paragraph's links: listed on the line each link's text starts on)
	for (int l = first_line; l < line_count; l++) {
		lines[l].first_link = first_link;
		lines[l].link_count = 0;
		for (const char* c = lines[l].text; (c != NULL) && (*c != '\0'); c++) {
			if (*c == LINK_START) {
				lines[l].link_count++;
			}
		}
		first_link += lines[l].link_count;
	}
	*length = 0;
}

int markdown_load(const char* path, float width, float height) {
	markdown_free();
	layout_width = width;
	layout_height = height;
	file_t handle = fs_open(path, O_RDONLY);
	if (handle == -1) {
		return -1;
	}
	size_t size = fs_total(handle);
	char* source = malloc(size + 1);
	if (source == NULL) {
		fs_close(handle);
		return -1;
	}
	size_t got = fs_read(handle, source, size);
	fs_close(handle);
	source[(got <= size) ? got : 0] = '\0';

	char* paragraph = malloc(size + 1);
	size_t length = 0;
	Line_Kind kind = LINE_TEXT;
	char* line = source;
	while ((paragraph != NULL) && (line != NULL) && (*line != '\0')) {
		char* next = strchr(line, '\n');
		if (next != NULL) {
			*next++ = '\0';
		}
		size_t n = strlen(line);
		if ((n > 0) && (line[n - 1] == '\r')) {
			line[--n] = '\0';
		}
		char* text = line;
		while (*text == ' ') {
			text++;
		}
		if (*text == '\0') {
			flush(kind, paragraph, &length, width);
			if ((line_count > 0) && (lines[line_count - 1].kind != LINE_GAP)) {
				line_add(LINE_GAP, NULL, 0);
			}
		} else if ((text[0] == '#') && (text[1] == ' ')) {
			flush(kind, paragraph, &length, width);
			kind = LINE_HEADING1;
			memcpy(paragraph, text + 2, strlen(text + 2));
			length = strlen(text + 2);
			flush(kind, paragraph, &length, width);
			kind = LINE_TEXT;
		} else if ((text[0] == '#') && (text[1] == '#')) {
			flush(kind, paragraph, &length, width);
			char* heading = text;
			while (*heading == '#') {
				heading++;
			}
			while (*heading == ' ') {
				heading++;
			}
			length = strlen(heading);
			memcpy(paragraph, heading, length);
			flush(LINE_HEADING2, paragraph, &length, width);
			kind = LINE_TEXT;
		} else if (strncmp(text, "---", 3) == 0) {
			flush(kind, paragraph, &length, width);
			line_add(LINE_RULE, NULL, 0);
			kind = LINE_TEXT;
		} else if ((text[0] == '!') && (text[1] == '[')) {
			flush(kind, paragraph, &length, width);
			char* open = strstr(text, "](");
			char* close = (open != NULL) ? strchr(open, ')') : NULL;
			if (close != NULL) {
				*close = '\0';
				Md_Line* image_line = line_add(LINE_IMAGE, NULL, 0);
				if (image_line != NULL) {
					image_line -> image = image_add(open + 2);
				}
			}
			kind = LINE_TEXT;
		} else if (((text[0] == '-') || (text[0] == '*')) && (text[1] == ' ')) {
			flush(kind, paragraph, &length, width);
			kind = LINE_BULLET;
			length = strlen(text + 2);
			memcpy(paragraph, text + 2, length);
		} else {
			// (more of the paragraph or bullet: a space between its lines)
			if (length > 0) {
				paragraph[length++] = ' ';
			}
			size_t add = strlen(text);
			memcpy(paragraph + length, text, add);
			length += add;
		}
		line = next;
	}
	if (paragraph != NULL) {
		flush(kind, paragraph, &length, width);
	}
	free(paragraph);
	free(source);

	// each line's place
	float y = 0.0f;
	for (int i = 0; i < line_count; i++) {
		Md_Line* l = &lines[i];
		switch (l -> kind) {
			case LINE_IMAGE:
				if (l -> image >= 0) {
					// (fitted inside the page's width and visible height, its shape kept, so it's seen whole (Matt: the
					// 256x240 AccuracyCoin frame at the page's width was 512 tall); one that's still taller, a
					// page set with no height, is drawn cropped to the page as it scrolls, markdown_draw)
					float w = (float)images[l -> image].width, h = (float)images[l -> image].height;
					float scale = width / w;
					if ((layout_height > 0.0f) && (h * scale > layout_height)) {
						scale = layout_height / h;
					}
					images[l -> image].draw_width = w * scale;
					images[l -> image].draw_height = h * scale;
				}
				l -> height = ((l -> image >= 0) ? images[l -> image].draw_height : 0.0f) + 6.0f;
				break;
			case LINE_RULE:
				l -> height = 12.0f;
				break;
			case LINE_GAP:
				l -> height = 8.0f;
				break;
			default:
				l -> height = line_font(l -> kind) -> fontHeight * line_scale(l -> kind);
				break;
		}
		l -> top = y;
		y += l -> height;
	}
	total_height = y;
	return 0;
}

void markdown_free() {
	for (int i = 0; i < line_count; i++) {
		free(lines[i].text);
	}
	free(lines);
	lines = NULL;
	line_count = line_capacity = 0;
	for (int i = 0; i < link_count; i++) {
		free(links[i].text);
		free(links[i].url);
	}
	free(links);
	links = NULL;
	link_count = link_capacity = 0;
	for (int i = 0; i < image_count; i++) {
		pvr_mem_free(images[i].texture);
	}
	image_count = 0;
	total_height = 0.0f;
}

float markdown_height() {
	return total_height;
}

// a line's text in runs, the links' in the highlight colour
static void draw_text(Md_Line* l, uint32 list, float x, float y) {
	Font* f = line_font(l -> kind);
	float scale = line_scale(l -> kind);
	uint32 color = (l -> kind == LINE_HEADING1) || (l -> kind == LINE_HEADING2) ? 0xFF00FF00 : options.GUI_TextColor;
	const char* run = l -> text;
	int in_link = 0;
	while ((run != NULL) && (*run != '\0')) {
		const char* end = run;
		while ((*end != '\0') && (*end != LINK_START) && (*end != LINK_END) && (*end != LINK_CARRIED)) {
			end++;
		}
		if (end > run) {
			char piece[256];
			size_t n = ((size_t)(end - run) < sizeof(piece) - 1) ? (size_t)(end - run) : sizeof(piece) - 1;
			memcpy(piece, run, n);
			piece[n] = '\0';
			float width;
			measureLine(f, piece, scale, &width);
			draw_string(f, list, piece, x, y, 35.0f, width + 2.0f, 40.0f, SINGLE, LEFT, in_link ? options.GUI_SelectedTextColor : color, scale);
			x += width;
		}
		if ((*end == LINK_START) || (*end == LINK_CARRIED)) {
			in_link = 1;
			end++;
		} else if (*end == LINK_END) {
			in_link = 0;
			end++;
		}
		run = end;
	}
}

void markdown_draw(uint32 list, float x, float y, float height, float scroll) {
	pvr_poly_cxt_t cxt;
	pvr_poly_hdr_t hdr;
	for (int i = 0; i < line_count; i++) {
		Md_Line* l = &lines[i];
		float top = l -> top - scroll;
		// (a picture partly on the page is drawn cropped to it; other lines only whole)
		if ((l -> kind == LINE_IMAGE) ? ((top + l -> height <= 0.0f) || (top >= height)) :
			((top < 0.0f) || (top + l -> height > height))) {
			continue;
		}
		float ly = y + top;
		switch (l -> kind) {
			case LINE_IMAGE:
				if ((list == PVR_LIST_TR_POLY) && (l -> image >= 0)) {
					pvr_poly_cxt_txr(&cxt, list, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_TWIDDLED,
						images[l -> image].texture_width, images[l -> image].texture_height,
						images[l -> image].texture, PVR_FILTER_BILINEAR);
					pvr_poly_compile(&hdr, &cxt);
					pvr_prim(&hdr, sizeof(hdr));
					pvr_vertex_t v;
					float w = images[l -> image].draw_width, h = images[l -> image].draw_height;
					// (cropped to the page: the quad's top and bottom moved in, V in proportion)
					float y0 = (ly < y) ? y : ly, y1 = (ly + h > y + height) ? y + height : ly + h;
					// (the picture's share of the texture: U and V to its right and bottom edges)
					float u_max = (float)images[l -> image].width / (float)images[l -> image].texture_width;
					float v_max = (float)images[l -> image].height / (float)images[l -> image].texture_height;
					float v0 = v_max * (y0 - ly) / h, v1 = v_max * (y1 - ly) / h;
					v.z = 34.0f;
					v.argb = 0xFFFFFFFF;
					v.oargb = 0;
					v.flags = PVR_CMD_VERTEX;
					// (centred across the page)
					float ix = x + (layout_width - w) / 2.0f;
					v.x = ix; v.y = y1; v.u = 0.0f; v.v = v1; pvr_prim(&v, sizeof(v));
					v.x = ix; v.y = y0; v.u = 0.0f; v.v = v0; pvr_prim(&v, sizeof(v));
					v.x = ix + w; v.y = y1; v.u = u_max; v.v = v1; pvr_prim(&v, sizeof(v));
					v.flags = PVR_CMD_VERTEX_EOL;
					v.x = ix + w; v.y = y0; v.u = u_max; v.v = v0; pvr_prim(&v, sizeof(v));
				}
				break;
			case LINE_RULE:
				if (list == PVR_LIST_OP_POLY) {
					draw_cornered_layer(list, x, ly + 5.0f, 33.0f, layout_width, 2.0f, 0.0f, options.GUI_TextColor);
				}
				break;
			case LINE_GAP:
				break;
			case LINE_BULLET:
				if (list == PVR_LIST_OP_POLY) {
					draw_cornered_layer(list, x + 5.0f, ly + l -> height / 2 - 2.5f, 33.0f, 5.0f, 5.0f, 0.0f, options.GUI_TextColor);
				} else {
					draw_text(l, list, x + BULLET_INDENT, ly);
				}
				break;
			case LINE_INDENTED:
				if (list == PVR_LIST_TR_POLY) {
					draw_text(l, list, x + BULLET_INDENT, ly);
				}
				break;
			default:
				if (list == PVR_LIST_TR_POLY) {
					draw_text(l, list, x, ly);
				}
				break;
		}
	}
}

int markdown_links(float scroll, float height, char out[][96], int max_lines) {
	int count = 0;
	for (int i = 0; (i < line_count) && (count < max_lines); i++) {
		Md_Line* l = &lines[i];
		float top = l -> top - scroll;
		if ((top < 0.0f) || (top + l -> height > height)) {
			continue;
		}
		for (int k = 0; (k < l -> link_count) && (count < max_lines); k++) {
			Md_Link* link = &links[l -> first_link + k];
			snprintf(out[count++], 96, "%s: %s", link -> text, link -> url);
		}
	}
	return count;
}
