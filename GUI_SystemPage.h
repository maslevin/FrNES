/*===================================================================*/
/*                                                                   */
/*  GUI_SystemPage.h : GUI Page for System Settings                  */
/*                     Interface, Variables, and Event Handling      */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#ifndef GUI_SYSTEMPAGE
#define GUI_SYSTEMPAGE

#include <kos.h>
#include "TextWindow.h"
#include "BMFFile.h"

//Function Definitions
void setup_system_options_screen();
void Generate_System_Options_List();
void Handle_System_Interface(cont_cond_t my_cond);
void Allocate_System_Options();
void Free_System_Options();

//External Variables Required
extern Window_Style mystyle;
extern Window_Data mydata;
extern Window_Style helpstyle;
extern Window_Data helpdata;

extern BMF_Character largefont[];
extern BMF_Character smallfont[];
extern BMF_Character medfont[];

extern void* PVR_MainWindow_Offset;
extern void* PVR_SmallWindow_Offset;

extern int keyhit;
extern int invalida;
extern int xkeyhit;
extern int menuscreen;
extern const int MAX_CLIP_PIXELS;
extern unsigned char isMainChanged;
extern unsigned char isSmallChanged;
extern const int title_offset_y;
extern const int Max_Frameskip;

extern char* Main_Keys[];
extern char* Options_Keys[];
extern const int Num_Options_Keys;

extern int8 VMUs[];

extern uint16 GUI_OutsideWindowColor;
extern uint16 GUI_InsideWindowColor;
extern uint16 GUI_TextColor;
extern uint16 GUI_SelectedTextColor;

//Options Variables
extern uint16* opt_SoundEnabled;
extern uint16* opt_FrameSkip;
extern uint16* opt_AutoFrameSkip;
extern uint16* opt_DiscFormat;
extern int16* opt_VMUPort;
extern uint16* opt_SRAM;


#endif