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

typedef struct
{
	char FileName[65];
	char PhysFileName[65];
	int FileSize;
	int IsRead;
} RomInfo;

typedef struct
{
	char FileName[65];
	uint32 CheckSum;
} RomCheck;

//Initializes a RomInfo array to the unread values
void InitializeFileInfos(RomInfo* RomInfoArray, char** RomPtrArray, int NumBuffers);

//Start the Search Process
int StartFileSearch(char* Path, RomInfo* RomInfoArray);

//End the Search Process
void EndFileSearch();

//Return the Number of Roms found
int ReturnCurrentNumRoms();

//Loads a file info without using checksum identification
int LoadNextFileSimple(RomInfo* RomInfoArray, unsigned char* current_path);

//Loads a file info using checksum identification
int LoadNextFileComplex(RomInfo* RomInfoArray);

uint32 ReturnChecksum(const unsigned char* filepath, uint32 filesize, unsigned char* temprom);

#endif