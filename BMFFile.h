/* 
File:        BMFFile.h
Project:     Platform Development Tools
Date:        December. 11th, 2000
Programmer:  ReGex
Description: File that contains a single bitmapped font face
Update: October 9th, 2001 - Updated to Use Dynamic Memory Allocation Under KOS
*/

#ifndef __BITMAP_FONT_FILE_H
#define __BITMAP_FONT_FILE_H

#include <string.h>
#include <kos.h>
#include "BMFConsts.h"

typedef struct
{
	int height;
	int width;
	unsigned char* bitdata;
} BMF_Character;

/*-----------------------------------------------------------------------------------
Private Function (read the bitdata for a font into the buffer specified, from the
                  prebuffered font file)

Inputs: 
	int numbytes (number of bytes to read into the buffer)
	unsigned char* buffer: destination for the pixel data

Output:
    numbytes from file pointer is copied into buffer
	file pointer is incremented numbytes times
	returns nothing
-------------------------------------------------------------------------------------*/
void bf_read_bitdata(int numbytes, unsigned char* buffer);

/*-----------------------------------------------------------------------------------
Private Function (reads a faux-16 bit unsigned integer (MS lib bug) from the prebuffered
                  font file and returns it)

Inputs: 
    none
	precondition: prebuffered file is loaded into memory

Output:
    numbytes from file pointer is copied into buffer
	file pointer is incremented numbytes times
	returns nothing
-------------------------------------------------------------------------------------*/
uint16 bf_read16();

/*-----------------------------------------------------------------------------------
Private Function (reads a bitmapped font file from the cd into memory)

Inputs: 
    char* filename: absolute path of the font file to load
	precondition: my_cd_buffer has a been allocated a buffer size large enough to 
	              accomodate the file
	     
Output:
    int filesize is the number of bytes actually read
    my_cd_buffer is the full file from the cd
	my_file_pointer is the head of my_cd_buffer
	returns success or failure:
		0 - file not found
		1 - success
-------------------------------------------------------------------------------------*/
int bf_open(char* filename);

/*-----------------------------------------------------------------------------------
Private Function (closes the prebuffered file)

Inputs: 
	precondition: my_cd_buffer is a buffer
	     
Output:
	my_cd_buffer = NULL;
	my_file_pointer = NULL;
	returns nothing
-------------------------------------------------------------------------------------*/
void bf_close();

/*-----------------------------------------------------------------------------------
Public Function (return pointer to the BMF_character in a font that corresponds to 
                 a char)

Inputs: 
	BMF_Character* font: Font to return the character from
	char str: character to return

Output:
    returns pointer to the corresponding BMF_Character from the font
-------------------------------------------------------------------------------------*/
BMF_Character* bf_char_ptr(BMF_Character* font, char str);

/*-----------------------------------------------------------------------------------
Public Function (return width of a string in a font, in pixels)

Inputs: 
	BMF_Character* font: Font to get the widths from
	char* str: string to get the width of

Output:
    returns width of the string in pixels
-------------------------------------------------------------------------------------*/
int bf_str_width(BMF_Character* font, char* str);

/*-----------------------------------------------------------------------------------
Public Function (return height of a string in a font, in pixels)

Inputs: 
	BMF_Character* font: Font to get the height from
	char* str: string to get the width of

Output:
    returns width of the string in pixels
-------------------------------------------------------------------------------------*/
int bf_str_height(BMF_Character* font, char* str);

/*-----------------------------------------------------------------------------------
Public Function (load bitmapped font from cd)
Format of the file is as such:

bits 0-5: char "BMFONT"
byte 6: character width
byte 7: unused
byte 8: character height
byte 9: unused
byte 10 -> byte 6 * byte 8 : bit data
  byte == 0 : bit not on
  byte == 1 : bit's on

this format continues NUM_CHARS times for the number of characters defined

Bytes are used because later versions of the file format could use palette data,
or intensities to generate smooth cornered fonts.  This was done as a quick and dirty
way of getting variable width, nice looking fonts.

Inputs:
    char* filename       : absolute path (all caps) of .bmf file
	BMF_Character* target: pointer a buffer of BMF_Character, NUM_CHARS long where the font
	                       will be stored

Output:
	font stored in buffer, target
	returns error or success:
		0 - file not found
		1 - success
		2 - file is not a bitmap font
		4 - invalid font dimension - 32 x 32 Chars are the Maximum
-------------------------------------------------------------------------------------*/
int bf_load_file(char* filename, BMF_Character* target);

/*-----------------------------------------------------------------------------------
Public Function (deallocate font from memory)

Inputs:
	BMF_Character* target: pointer a buffer of BMF_Character, NUM_CHARS long where the font
	                       resides

Output:
	None: Font no longer in heap
-------------------------------------------------------------------------------------*/
void bf_free_font(BMF_Character* font);

/*-----------------------------------------------------------------------------------
Public Function (draws the passed string to the destination buffer,
                 functionally similar to libdream function: bfont_draw_str)

Inputs: 
	BMF_Character* font: Font to get the widths from
	uint16 color:  Color to draw the string in
	int bufwidth: Width of the buffer we're drawing to
	char* str: string to draw

Output:
	copies str to the destination buffer, in color
    returns nothing
-------------------------------------------------------------------------------------*/
void bf_draw_str(BMF_Character* font, uint16 color, uint16* buffer, int bufwidth, char* str);
void bf_bgdraw_str(BMF_Character* font, uint16 color, uint16 bgcolor, uint16* buffer, int bufwidth, char* str);
void bf_draw_str_widthclip(BMF_Character* font, uint16 color, uint16* buffer, int bufwidth, char* str, int maxwidth);
void bf_bgdraw_str_widthclip(BMF_Character* font, uint16 color, uint16 bgcolor, uint16* buffer, int bufwidth, char* str, int maxwidth);
void bf_draw_str_rlalign(BMF_Character* font, uint16 color, uint16* buffer, int bufwidth, char* str, int maxwidth);
void bf_bgdraw_str_rlalign(BMF_Character* font, uint16 color, uint16 bgcolor, uint16* buffer, int bufwidth, char* str, int maxwidth);

#endif