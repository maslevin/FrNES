#ifndef _MAPPER_25_H
#define _MAPPER_25_H

#include "Mapper.h"

void Mapper_25_Init();
void Mapper_25_Write( uint16 wAddr, unsigned char byData );
void Mapper_25_HSync();

#endif