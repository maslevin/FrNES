# KallistiOS 1.1.5
#
# FrNES 0.60/Makefile
# (c)2001 ReGex


BIN = pNesX
#OBJS = BMFFile.o pNesX_System_DC.o pNesX.o TextWindow.o pNesX_Sound_DC.o K6502.o vmu_dc.o K6502_rw.o BMF2PVR.o ROMLoad.o pNesX_Mapper.o sincos.o aica_fw.o
#OBJS = BMFFile.o pNesX_System_DC.o pNesX_DrawLine_Spr_Asm.o pNesX_DrawLine_BG_Asm.o pNesX_DrawLine_Merge_Asm.o pNesX_BuildCharAsm.o pNesX_PPU_DC.o pNesX.o TextWindow.o pNesX_Sound_APU.o pNesX_Sound_DC.o K6502.o vmu_dc.o K6502_rw.o BMF2PVR.o ROMLoad.o pNesX_Mapper.o aica_fw.o
#OBJS = ROMLoad.o pNesX_Sound_APU.o aica_fw.o K6502_rw.o pNesX_DrawLine_BG_C.o pNesX_BuildCharAsm.o pNesX_Sound_DC.o K6502.o pNesX_PPU_DC.o pNesX_DrawLine_Spr_C_Map9.o pNesX_DrawLine_BG_C_Map9.o vmu_dc.o BMF2PVR.o pNesX_DrawLine_Spr_Asm.o pNesX.o pNesX_Mapper.o pNesX_Utils.o BMFFile.o GUI_VideoPage.o GUI_SystemPage.o GUI_ControlPage.o GUI_GUIPage.o pNesX_System_DC.o TextWindow.o 
OBJS = ROMLoad.o pNesX_Sound_DC.o aica_fw.o K6502_rw.o pNesX_Sound_APU.o pNesX_BuildCharAsm.o K6502.o pNesX_DrawLine_Spr_Asm.o pNesX_PPU_DC.o pNesX_DrawLine_BG_C.o pNesX_DrawLine_Spr_C_Map9.o pNesX_DrawLine_BG_C_Map9.o BMF2PVR.o pNesX.o pNesX_Mapper.o pNesX_Utils.o BMFFile.o GUI_VideoPage.o GUI_SystemPage.o GUI_ControlPage.o GUI_GUIPage.o pNesX_System_DC.o vmu_dc.o TextWindow.o FrNESImg.o

#pNesX_DrawLine_BG_C.o pNesX_DrawLine_Spr_C.o

include ../Makefile.prefab