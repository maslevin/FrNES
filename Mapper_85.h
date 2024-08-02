#ifndef _MAPPER_85_H
#define _MAPPER_85_H

#include "Mapper.h"

void Mapper_85_Init();
void Mapper_85_Write( uint16 wAddr, unsigned char byData );
void Mapper_85_HSync();

#endif