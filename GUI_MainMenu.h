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
#include "pNesX_System_DC.h"

#include "options.h"

//Function Definitions
void setup_main_menu_screen();
void Handle_Main_Menu_Interface(cont_state_t* my_state);

extern char* Main_Keys[];
extern const int Num_Main_Keys;

#endif