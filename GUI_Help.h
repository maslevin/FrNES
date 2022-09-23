/*===================================================================*/
/*                                                                   */
/*  GUI_ControlPage.h : GUI Page for Control Settings                */
/*                      Interface, Variables, and Event Handling     */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#ifndef GUI_HELP
#define GUI_HELP

#include <kos.h>
#include "TextWindow.h"

#include "GUI_MainMenu.h"

//Function Definitions
void setup_help_screen();
void Handle_Help_Interface(cont_state_t* my_state);

//External Variables Required
extern Window_Style mystyle;
extern Window_Data mydata;
extern Window_Style helpstyle;
extern Window_Data helpdata;

extern int keyhit;
extern int invalida;
extern int xkeyhit;
extern int romselstatus;
extern const int MAX_CLIP_PIXELS;
extern const int title_offset_y;
extern const int Max_Frameskip;

extern char* Main_Keys[];
extern const int Num_Main_Keys;
extern char* Options_Keys[];
extern const int Num_Options_Keys;

#endif