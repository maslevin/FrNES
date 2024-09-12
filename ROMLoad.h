/*===================================================================*/
/*                                                                   */
/*  ROMLoad.h : ROM Checksums an Read/Write/Checksum Operations      */
/*                                                                   */
/*  2001/12/22  ReGex   FrNES 0.60 Source Release                    */
/*                                                                   */
/*===================================================================*/

#ifndef __ROMLOAD_H
#define __ROMLOAD_H

#include <kos.h>
#include "string.h"

typedef struct __attribute__ ((packed, aligned(4))) RomInfo_s {
	char FileName[65];
	char PhysFileName[65];
	int FileSize;
	int IsRead;
} RomInfo_t;

typedef struct {
	char FileName[65];
	uint32 CheckSum;
} RomCheck;

//Initializes a RomInfo array to the unread values
void InitializeFileInfos(RomInfo_t* RomInfoArray, char** RomPtrArray, int NumBuffers);

//Start the Search Process
int StartFileSearch(char* Path, RomInfo_t* RomInfoArray);

//End the Search Process
void EndFileSearch();

//Return the Number of Roms found
int ReturnCurrentNumRoms();

//Loads a file info without using checksum identification
int LoadNextFileSimple(RomInfo_t* RomInfoArray, char* current_path);

//Loads a file info using checksum identification
int LoadNextFileComplex(RomInfo_t* RomInfoArray);

uint32 ReturnChecksum(const char* filepath, uint32 filesize, unsigned char* temprom);

#endif