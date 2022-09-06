/*===================================================================*/
/*                                                                   */
/*  GUI_GUIPage.h : GUI Page for GUI Custom Settings                 */
/*                  Interface, Variables, and Event Handling         */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#ifndef GUI_GUIPAGE
#define GUI_GUIPAGE

#include <kos.h>
#include "TextWindow.h"
#include "BMFFile.h"

//Function Definitions
void setup_gui_options_screen();
void Generate_GUI_Options_List();
void Handle_GUI_Interface(cont_cond_t my_cond);
void Allocate_GUI_Options();
void Free_GUI_Options();

//External Variables Required
extern Window_Style mystyle;
extern Window_Data mydata;
extern Window_Style helpstyle;
extern Window_Data helpdata;

extern BMF_Character largefont[];
extern BMF_Character smallfont[];
extern BMF_Character medfont[];

extern pvr_ptr_t PVR_MainWindow_Offset;
extern pvr_ptr_t PVR_SmallWindow_Offset;

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

//These are our options variables
extern uint16 GUI_OutsideWindowColor;
extern uint16 GUI_InsideWindowColor;
extern uint16 GUI_TextColor;
extern uint16 GUI_SelectedTextColor;
extern uint32 GUI_BGColor;

extern const char Tag_Slider[];
extern const char SLIDERBOX_MAX[];
extern const char SLIDERBOX_MIN[];
extern const char SLIDERBOX_VALUE[];

#endif