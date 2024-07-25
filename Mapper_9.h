#ifndef _MAPPER_9_H
#define _MAPPER_9_H

#include "Mapper.h"

void Mapper_9_Init();
void Mapper_9_Write( uint16 wAddr, unsigned char byData );
void Mapper_9_PPU_Latch_FDFE(uint16 wAddr);

#endif