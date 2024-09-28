/*===================================================================*/
/*                                                                   */
/*  Mapper.h : Mapper Function                                       */
/*                                                                   */
/*  1999/11/03  Racoon     New preparation                           */
/*  2001/12/22  maslevin   0.60 Final Release                        */
/*                         Mapper fixes + 7 + 9 initial drivers      */
/*  2024/07/24  maslevin   Fully rework mapper framework             */
/*                                                                   */
/*===================================================================*/

#ifndef _MAPPER_H
#define _MAPPER_H

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/
#include <kos.h>

#include "pNesX.h"
#include "cartridge.h"
#include "K6502.h"
#include "pNesX_System_DC.h"

/*-------------------------------------------------------------------*/
/*  Mapper Configuration                                             */
/*-------------------------------------------------------------------*/
typedef struct __attribute__ ((packed, aligned(4))) Mapper_s {
	void (*init)();
	void (*write)( uint16 wAddr, unsigned char byData );
	unsigned char (*read)( uint16 wAddr );
	void (*vsync)();
	void (*hsync)();
} Mapper_t;

typedef struct __attribute__ ((packed, aligned(4))) MapperTable_s {
  int nMapperNo;
  Mapper_t * mapper;
} MapperTable_t;

extern MapperTable_t Mappers[];
extern Mapper_t* mapper;
extern uint8 MapperNo;
extern uint8 SubmapperNo;

/*-------------------------------------------------------------------*/
/*  Macros                                                           */
/*-------------------------------------------------------------------*/

/* The address of 8Kbytes unit of the ROM */
#define ROMPAGE(a)     &ROM[ (a) * 0x2000 ]
/* From behind the ROM, the address of 8kbytes unit */
#define ROMLASTPAGE(a) &ROM[ NesHeader.byRomSize * 0x4000 - ( (a) + 1 ) * 0x2000 ]
/* The address of 1Kbytes unit of the VROM */
#define VROMPAGE(a)    &VROM[ (a) * 0x400 ]

#endif