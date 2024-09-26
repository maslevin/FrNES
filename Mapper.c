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
#include "Mapper_11.h"
#include "Mapper_21.h"
#include "Mapper_22.h"
#include "Mapper_23.h"
#include "Mapper_24.h"
#include "Mapper_25.h"
#include "Mapper_26.h"
#include "Mapper_30.h"
#include "Mapper_64.h"
#include "Mapper_66.h"
#include "Mapper_67.h"
#include "Mapper_68.h"
#include "Mapper_69.h"
#include "Mapper_73.h"
#include "Mapper_75.h"
#include "Mapper_85.h"
#include "Mapper_111.h"
#include "Mapper_119.h"

/*-------------------------------------------------------------------*/
/*  Table of Mapper initialize function                              */
/*-------------------------------------------------------------------*/

MapperTable Mappers[] = {
	{
		0, &(Mapper) { 
			.init = Mapper_0_Init, 
			.write = Mapper_0_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync
		}
	},
	{ 
		1, &(Mapper) {
			.init = Mapper_1_Init, 
			.write = Mapper_1_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync
		} 
	},
	{ 
		2, &(Mapper) { 
			.init = Mapper_2_Init, 
			.write = Mapper_2_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync
		}
	},
	{ 
		3, &(Mapper) { 
			.init = Mapper_3_Init, 
			.write = Mapper_3_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync
		}
	},
	{
		4, &(Mapper) { 
			.init = Mapper_4_Init, 
			.write = Mapper_4_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_4_VSync,
			.hsync = Mapper_4_HSync
		}
	},
  	{
		5, &(Mapper) { 
			.init = Mapper_5_Init, 
			.write = Mapper_5_Write,
			.read = Mapper_5_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_5_HSync
		} 
	},
  	{ 6, NULL },
    { 
		7, &(Mapper) { 
			.init = Mapper_7_Init, 
			.write = Mapper_7_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync		
		}
	},
	{ 8, NULL },
  	{
		9, &(Mapper) { 
			.init = Mapper_9_Init, 
			.write = Mapper_9_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync		
		}
	},
	{ 10, NULL },
	{
		11, &(Mapper) { 
			.init = Mapper_11_Init, 
			.write = Mapper_11_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync		
		}
	},
	{ 12, NULL },
	{ 13, NULL },
	{ 14, NULL },
	{ 15, NULL },
	{ 16, NULL },
	{ 17, NULL }, 
	{ 18, NULL },
	{ 19, NULL },
	{ 20, NULL },
	{ 
		21, &(Mapper) { 
			.init = Mapper_21_Init, 
			.write = Mapper_21_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_21_HSync
		}  
	},
	{
		22, &(Mapper) { 
			.init = Mapper_22_Init, 
			.write = Mapper_22_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync
		}
	},
	{ 
		23, &(Mapper) { 
			.init = Mapper_23_Init, 
			.write = Mapper_23_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_23_HSync
		}
	},
	{
		24, &(Mapper) { 
			.init = Mapper_24_Init, 
			.write = Mapper_24_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_24_HSync		
		} 
	},
	{ 
		25, &(Mapper) { 
			.init = Mapper_25_Init, 
			.write = Mapper_25_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_25_HSync		
		}  
	},
	{
		26, &(Mapper) { 
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
		30, &(Mapper) { 
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
	{ 
		64, &(Mapper) { 
			.init = Mapper_64_Init, 
			.write = Mapper_64_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_64_HSync
		}  
	},
	{ 65, NULL },
	{ 
		66, &(Mapper) { 
			.init = Mapper_66_Init, 
			.write = Mapper_66_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync
		} 
	},
	{ 
		67, &(Mapper) { 
			.init = Mapper_67_Init, 
			.write = Mapper_67_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_67_HSync		
		} 
	},
	{
		68, &(Mapper) { 
			.init = Mapper_68_Init, 
			.write = Mapper_68_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync		
		}  
	},
	{
		69, &(Mapper) { 
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
	{ 
		73, &(Mapper) {
			.init = Mapper_73_Init,
			.write = Mapper_73_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_73_HSync
		}
	},
	{ 74, NULL },
	{ 
		75, &(Mapper) {
			.init = Mapper_75_Init,
			.write = Mapper_75_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync
		}
	},
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
		85, &(Mapper) { 
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
	{ 90, NULL },
	{ 91, NULL },
	{ 92, NULL },
	{ 93, NULL },
	{ 94, NULL },
	{ 95, NULL },
	{ 96, NULL },
	{ 97, NULL }, 
	{ 98, NULL },
	{ 99, NULL },
	{ 100, NULL },
	{ 101, NULL },
	{ 102, NULL },
	{ 103, NULL },
	{ 104, NULL },
	{ 105, NULL },
	{ 106, NULL },
	{ 107, NULL }, 
	{ 108, NULL },
	{ 109, NULL },
	{ 110, NULL },
	{ 
		111, &(Mapper) { 
			.init = Mapper_111_Init, 
			.write = Mapper_111_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_0_HSync		
		}
	},
	{ 112, NULL },
	{ 113, NULL },
	{ 114, NULL },
	{ 115, NULL },
	{ 116, NULL },
	{ 117, NULL }, 
	{ 118, NULL },
	{ 
		119, &(Mapper) { 
			.init = Mapper_119_Init, 
			.write = Mapper_119_Write,
			.read = Mapper_0_Read,
			.vsync = Mapper_0_VSync,
			.hsync = Mapper_119_HSync		
		}
	},
	{ 120, NULL },			
	{ -1, NULL }
};