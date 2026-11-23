/*===================================================================*/
/*                                                                   */
/*  GUI_ControlPage.h : GUI Page for Control Settings                */
/*                      Interface, Variables, and Event Handling     */
/*                                                                   */
/*  2001/12/22  ReGex   0.60 Final Release                           */
/*                                                                   */
/*===================================================================*/

#ifndef GUI_CONTROLPAGE
#define GUI_CONTROLPAGE

#include <kos.h>
#include "options.h"
#include "pNesX_System_DC.h"

#include "TextWindow.h"
#include "GUI_MainMenu.h"

//Function Definitions
void setup_control_options_screen();
void Generate_Control_Options_List();
void Handle_Control_Interface(cont_state_t* my_state);

#endif