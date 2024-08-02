#ifndef _MAPPER_24_H
#define _MAPPER_24_H

#include "Mapper.h"

void Mapper_24_Init();
void Mapper_24_Write( uint16 wAddr, unsigned char byData );
void Mapper_24_HSync();

#endif