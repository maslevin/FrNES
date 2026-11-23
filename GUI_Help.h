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
#include "pNesX_System_DC.h"

#include "GUI_MainMenu.h"

//Function Definitions
void setup_help_screen();
void Handle_Help_Interface(cont_state_t* my_state);

#endif