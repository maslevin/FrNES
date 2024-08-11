#ifndef _MAPPER_4_H
#define _MAPPER_4_H

#include "Mapper.h"

void Mapper_4_Init();
unsigned char Mapper_4_Read( uint16 wAddr );
void Mapper_4_Write( uint16 wAddr, unsigned char byData );
void Mapper_4_HSync();

#endif