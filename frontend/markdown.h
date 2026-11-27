/*===================================================================*/
/*                                                                   */
/*  markdown.h : Markdown pages (help, about) in the main window     */
/*                                                                   */
/*===================================================================*/

#ifndef __MARKDOWN_H
#define __MARKDOWN_H

#include <kos.h>

/*
 * The Markdown the menus read: # and ## headings, paragraphs (lines run together until a blank one), - or * bullets,
 * --- rules, ![alt](/rd/art/picture.png) pictures on a line of their own (PNG, any size up to 1024 x 1024; drawn as
 * wide as the page, cropped to it as it scrolls), and [text](url) links, shown in the highlight colour with their
 * addresses in the status window while they're on screen. **bold** and `code` marks are dropped (the fonts have no
 * bold).
 */

/**
 * Reads and lays out a Markdown file for a column 'width' pixels wide and 'height' high in view (pictures fitted
 * inside both, their shape kept, centred across; height 0: to the width only); 0, or -1 if it couldn't be read
 */
int markdown_load(const char* path, float width, float height);
void markdown_free();

/**
 * Draws the part from 'scroll' pixels down into the box at (x, y), height high (lines that don't fit whole are left
 * out), in the list's pass
 */
void markdown_draw(uint32 list, float x, float y, float height, float scroll);

// the laid out height, for scrolling
float markdown_height();

/**
 * The links on the lines in view (scroll to scroll + height), as "text: url" lines into lines[]; how many
 */
int markdown_links(float scroll, float height, char lines[][96], int max_lines);

#endif
