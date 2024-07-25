#ifndef _MAPPER_0_H
#define _MAPPER_0_H

#include "Mapper.h"

void Mapper_0_Init();
uint16 Mapper_0_Read( uint16 wAddr );
void Mapper_0_Write( uint16 wAddr, unsigned char byData );
void Mapper_0_VSync();
void Mapper_0_HSync();

#endif