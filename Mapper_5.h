#ifndef _MAPPER_5_H
#define _MAPPER_5_H

#include "Mapper.h"

unsigned char Mapper_5_PPU_Latch_RenderScreen(uint8 mode, uint32 addr);

void Mapper_5_Init();
void Mapper_5_Write( uint16 wAddr, unsigned char byData );
void Mapper_5_HSync();

#endif