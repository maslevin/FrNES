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
#include "pNesX_System_DC.h"

#include "GUI_MainMenu.h"

//Function Definitions
void setup_system_options_screen();
void Generate_System_Options_List();
void Handle_System_Interface(cont_state_t* my_state);

extern maple_device_t* VMUs[];

#endif