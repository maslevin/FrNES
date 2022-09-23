/*===================================================================*/
/*                                                                   */
/*  GUI_ControlPage.h : GUI Page for Control Settings                */
/*                      Interface, Variables, and Event Handling     */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#ifndef GUI_MAINMENU
#define GUI_MAINMENU

#include <kos.h>
#include "TextWindow.h"
#include "font.h"

extern Font* font;

//Function Definitions
void setup_main_menu_screen();
void Handle_Main_Menu_Interface(cont_state_t* my_state);

//External Variables Required
extern Window_Style mystyle;
extern Window_Data mydata;
extern Window_Style helpstyle;
extern Window_Data helpdata;

extern int keyhit;
extern int invalida;
extern int xkeyhit;
extern int romselstatus;
extern int disable_rom_interface;
extern int menuscreen;
extern const int MAX_CLIP_PIXELS;
extern const int title_offset_y;
extern const int Max_Frameskip;

extern char* Main_Keys[];
extern const int Num_Main_Keys;
extern char* Options_Keys[];
extern const int Num_Options_Keys;

extern uint32 GUI_BGColor;
extern uint32 GUI_OutsideWindowColor;
extern uint32 GUI_InsideWindowColor;
extern uint32 GUI_TextColor;
extern uint32 GUI_SelectedTextColor;

#endif