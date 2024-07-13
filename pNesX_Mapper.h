/*===================================================================*/
/*                                                                   */
/*  pNesX_Mapper.h : pNesX Mapper Function                           */
/*                                                                   */
/*  1999/11/03  Racoon  New preparation                              */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                      Mapper fixes + 7 + 9 initial drivers         */
/*                                                                   */
/*===================================================================*/

#ifndef PNESX_MAPPER_H_INCLUDED
#define PNESX_MAPPER_H_INCLUDED

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/

#include <kos.h>

/*-------------------------------------------------------------------*/
/*  Macros                                                           */
/*-------------------------------------------------------------------*/

/* The address of 8Kbytes unit of the ROM */
#define ROMPAGE(a)     &ROM[ (a) * 0x2000 ]
/* From behind the ROM, the address of 8kbytes unit */
#define ROMLASTPAGE(a) &ROM[ NesHeader.byRomSize * 0x4000 - ( (a) + 1 ) * 0x2000 ]

/* The address of 1Kbytes unit of the VROM */
#define VROMPAGE(a)    &VROM[ (a) * 0x400 ]

/*-------------------------------------------------------------------*/
/*  Table of Mapper initialize function                              */
/*-------------------------------------------------------------------*/

struct MapperTable_tag
{
  int nMapperNo;
  void (*pMapperInit)();
};

extern struct MapperTable_tag MapperTable[];

/*-------------------------------------------------------------------*/
/*  Function prototypes                                              */
/*-------------------------------------------------------------------*/

void Map0_Init();
void Map0_Write( uint16 wAddr, unsigned char byData );
void Map0_VSync();
void Map0_HSync();

void Map1_Init();
void Map1_Write( uint16 wAddr, unsigned char byData );

void Map2_Init();
void Map2_Write( uint16 wAddr, unsigned char byData );

void Map3_Init();
void Map3_Write( uint16 wAddr, unsigned char byData );

void Map4_Init();
void Map4_Write( uint16 wAddr, unsigned char byData );
void Map4_VSync();
void Map4_HSync();

void Map7_Init();
void Map7_Write( uint16 wAddr, unsigned char byData );

void Map9_Init();
void Map9_Write( uint16 wAddr, unsigned char byData );
void Map9_set_VROM_0000();
void Map9_set_VROM_1000();
void Map9_PPU_Latch_FDFE(uint16 wAddr);

void Map30_Init();
void Map30_Write( uint16 wAddr, unsigned char byData );

#endif /* !PNESX_MAPPER_H_INCLUDED */