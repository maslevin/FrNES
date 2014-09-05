/*===================================================================*/
/*                                                                   */
/*  pNesX_Utils.h : Utility Functions                                */
/*                                                                   */
/*  2001/12/22  ReGex   FrNES 0.60 Source Release                    */
/*                                                                   */
/*===================================================================*/

#ifndef PNESX_UTILS
#define PNESX_UTILS

char pNesX_nibtoc(uint32 nibble);
int pNesX_ctoi(char character);
void pNesX_i32toa(uint32 i, char* buffer);
int pNesX_pow10(int order);
int pNesX_atoi(char* numbuffer);
void pNesX_itoa(uint32 i, char* buffer);

#endif