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
#include "pNesX_System_DC.h"
#include "options.h"

#include "GUI_MainMenu.h"

//Function Definitions
void setup_gui_options_screen();
void Generate_GUI_Options_List();
void Handle_GUI_Interface(cont_state_t* my_state);

#endif