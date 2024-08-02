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
#include "Mapper_24.h"
#include "Mapper_26.h"
#include "Mapper_30.h"
#include "Mapper_69.h"
#include "Mapper_85.h"

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
  	{
		5, &(struct Mapper) { 
			.init = Mapper_5_Init, 
			.write = Mapper_5_Write,
			.read = Mapper_5_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_5_HSync
		} 
	},
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
	{
		24, &(struct Mapper) { 
			.init = Mapper_24_Init, 
			.write = Mapper_24_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_24_HSync		
		} 
	},
	{ 25, NULL },
	{
		26, &(struct Mapper) { 
			.init = Mapper_26_Init, 
			.write = Mapper_26_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_26_HSync		
		} 
	},
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
	{ 31, NULL },
	{ 32, NULL },
	{ 33, NULL },
	{ 34, NULL },
	{ 35, NULL },
	{ 36, NULL },
	{ 37, NULL }, 
	{ 38, NULL },
	{ 39, NULL },
	{ 40, NULL },
	{ 41, NULL },
	{ 42, NULL },
	{ 43, NULL },
	{ 44, NULL },
	{ 45, NULL },
	{ 46, NULL },
	{ 47, NULL },
	{ 48, NULL },
	{ 49, NULL },
	{ 50, NULL },
	{ 51, NULL },
	{ 52, NULL },
	{ 53, NULL },
	{ 54, NULL },
	{ 55, NULL },
	{ 56, NULL },
	{ 57, NULL }, 
	{ 58, NULL },
	{ 59, NULL },
	{ 60, NULL },
	{ 61, NULL },
	{ 62, NULL },
	{ 63, NULL },
	{ 64, NULL },
	{ 65, NULL },
	{ 66, NULL },
	{ 67, NULL },
	{ 68, NULL },
	{
		69, &(struct Mapper) { 
			.init = Mapper_69_Init, 
			.write = Mapper_69_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_69_HSync		
		} 
	},
	{ 70, NULL },
	{ 71, NULL },
	{ 72, NULL },
	{ 73, NULL },
	{ 74, NULL },
	{ 75, NULL },
	{ 76, NULL },
	{ 77, NULL }, 
	{ 78, NULL },
	{ 79, NULL },
	{ 80, NULL },
	{ 81, NULL },
	{ 82, NULL },
	{ 83, NULL },
	{ 84, NULL },
	{ 
		85, &(struct Mapper) { 
			.init = Mapper_85_Init, 
			.write = Mapper_85_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_85_HSync		
		} 
	},
	{ 86, NULL },
	{ 87, NULL }, 
	{ 88, NULL },
	{ 89, NULL },
	{ -1, NULL }
};