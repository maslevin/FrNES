/*===================================================================*/
/*                                                                   */
/*  BMF2PVR.h : Turn a BitmapFont into PVR textures                  */
/*              Not especially efficient... but works                */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#ifndef __BMF2PVR_H
#define __BMF2PVR_H

#include "BMFFile.h"
#include "BMFConsts.h"
#include <kos.h>

int bf_char_index(char str);
void bf_load_file_pvr(char* filename, 
    void* PVR_Offset,
    uint32 MaxBytesPerChar,
    unsigned char* WidthsArray,
    unsigned char* HeightArray);
void bf_load_file_pvr_colors(char* filename, void* PVR_Offset, uint32 MaxBytesPerChar, unsigned char* WidthsArray, unsigned char* HeightsArray, uint16 fontcolor, uint16 bgcolor);
void bf_ta_submit_character(void* PVR_Offset, float xposition, float yposition, float zposition, float screenwidth, float screenheight, unsigned char width, unsigned char height);
void bf_ta_submit_string(void* PVR_Offset, float xposition, float yposition, float zposition, float screenwidth, float screenheight, unsigned char* fontwidths, unsigned char* fontheights, char* my_string);
void bf_ta_submit_string_alpha(void* PVR_Offset, float xposition, float yposition, float zposition, float screenwidth, float screenheight, unsigned char* fontwidths, unsigned char* fontheights, char* my_string, float alpha);
#endif