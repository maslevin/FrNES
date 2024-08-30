#ifndef _MAPPER_64_H
#define _MAPPER_64_H

#include "Mapper.h"

void Mapper_64_Init();
void Mapper_64_Write( uint16 wAddr, unsigned char byData );
void Mapper_64_HSync();

#endif