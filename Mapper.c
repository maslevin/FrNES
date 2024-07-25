/*===================================================================*/
/*                                                                   */
/*  Mapper.c : pNesX Mapper Function                                 */
/*                                                                   */
/*  1999/11/03  Racoon  New preparation                              */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                      Mapper fixes + 7 + 9 initial drivers         */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/
#include "kos.h"
#include "macros.h"

#include "pNesX.h"
#include "pNesX_System.h"

#include "pNesX_System_DC.h"
#include "K6502.h"

#include "Mapper_0.h"
#include "Mapper_1.h"
#include "Mapper_2.h"
#include "Mapper_3.h"
#include "Mapper_4.h"
#include "Mapper_5.h"
#include "Mapper_7.h"
#include "Mapper_9.h"
#include "Mapper_30.h"

/*-------------------------------------------------------------------*/
/*  Table of Mapper initialize function                              */
/*-------------------------------------------------------------------*/

MapperTable Mappers[] = {
	{
		0, &(struct Mapper) { 
			.init = Mapper_0_Init, 
			.write = Mapper_0_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync
		}
	},
	{ 
		1, &(struct Mapper) {
			.init = Mapper_1_Init, 
			.write = Mapper_1_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync
		} 
	},
	{ 
		2, &(struct Mapper) { 
			.init = Mapper_2_Init, 
			.write = Mapper_2_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync
		}
	},
	{ 
		3, &(struct Mapper) { 
			.init = Mapper_3_Init, 
			.write = Mapper_3_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync
		}
	},
	{
		4, &(struct Mapper) { 
			.init = Mapper_4_Init, 
			.write = Mapper_4_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_4_VSync,
			.hsync = Mapper_4_HSync
		}
	},
  	{ 5, NULL },
  	{ 6, NULL },
    { 
		7, &(struct Mapper) { 
			.init = Mapper_7_Init, 
			.write = Mapper_7_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync		
		}
	},
	{ 8, NULL },
  	{
		9, &(struct Mapper) { 
			.init = Mapper_9_Init, 
			.write = Mapper_9_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync		
		}
	},
	{ 10, NULL },
	{ 11, NULL },
	{ 12, NULL },
	{ 13, NULL },
	{ 14, NULL },
	{ 15, NULL },
	{ 16, NULL },
	{ 17, NULL }, 
	{ 18, NULL },
	{ 19, NULL },
	{ 20, NULL },
	{ 21, NULL },
	{ 22, NULL },
	{ 23, NULL },
	{ 24, NULL },
	{ 25, NULL },
	{ 26, NULL },
	{ 27, NULL },
	{ 28, NULL },
	{ 29, NULL },
	{
		30, &(struct Mapper) { 
			.init = Mapper_30_Init, 
			.write = Mapper_30_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync		
		}
	},
	{ -1, NULL }
};